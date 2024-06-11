// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser_switcher/alternative_browser_driver.h"

#include <windows.h>

#include <ddeml.h>
#include <shellapi.h>
#include <shlobj.h>
#include <wininet.h>

#include <string_view>

#include "base/containers/heap_array.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/process/launch.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/thread_pool.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/win/registry.h"
#include "base/win/win_util.h"
#include "chrome/browser/browser_switcher/browser_switcher_prefs.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "url/gurl.h"

namespace browser_switcher {

namespace {

using LaunchCallback = AlternativeBrowserDriver::LaunchCallback;

const wchar_t kUrlVarName[] = L"${url}";

const wchar_t kIExploreKey[] =
    L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\App Paths\\IEXPLORE.EXE";
const wchar_t kFirefoxKey[] =
    L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\App Paths\\firefox.exe";
// Opera does not register itself here for now but it's no harm to keep this.
const wchar_t kOperaKey[] =
    L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\App Paths\\opera.exe";
const wchar_t kSafariKey[] =
    L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\App Paths\\safari.exe";
const wchar_t kChromeKey[] =
    L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\App Paths\\chrome.exe";
const wchar_t kEdgeKey[] =
    L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\App Paths\\msedge.exe";

const wchar_t kIExploreDdeHost[] = L"IExplore";

const wchar_t kChromeVarName[] = L"${chrome}";
const wchar_t kIEVarName[] = L"${ie}";
const wchar_t kFirefoxVarName[] = L"${firefox}";
const wchar_t kOperaVarName[] = L"${opera}";
const wchar_t kSafariVarName[] = L"${safari}";
const wchar_t kEdgeVarName[] = L"${edge}";

// Case-insensitive, typical filenames for popular browsers' executables.
const wchar_t kChromeTypicalExecutable[] = L"chrome.exe";
const wchar_t kIETypicalExecutable[] = L"iexplore.exe";
const wchar_t kFirefoxTypicalExecutable[] = L"firefox.exe";
const wchar_t kOperaTypicalExecutable[] = L"launcher.exe";
const wchar_t kEdgeTypicalExecutable[] = L"msedge.exe";

struct BrowserVarMapping {
  const wchar_t* var_name;
  const wchar_t* registry_key;
  const wchar_t* typical_executable;
  const char* browser_name;
  BrowserType browser_type;
};

const BrowserVarMapping kBrowserVarMappings[] = {
    {kChromeVarName, kChromeKey, kChromeTypicalExecutable, "",
     BrowserType::kChrome},
    {kIEVarName, kIExploreKey, kIETypicalExecutable, "Internet Explorer",
     BrowserType::kIE},
    {kFirefoxVarName, kFirefoxKey, kFirefoxTypicalExecutable, "Mozilla Firefox",
     BrowserType::kFirefox},
    {kOperaVarName, kOperaKey, kOperaTypicalExecutable, "Opera",
     BrowserType::kOpera},
    {kSafariVarName, kSafariKey, L"", "Safari", BrowserType::kSafari},
    {kEdgeVarName, kEdgeKey, kEdgeTypicalExecutable, "Microsoft Edge",
     BrowserType::kEdge},
};

// DDE Callback function which is not used in our case at all.
HDDEDATA CALLBACK DdeCallback(UINT type,
                              UINT format,
                              HCONV handle,
                              HSZ string1,
                              HSZ string2,
                              HDDEDATA data,
                              ULONG_PTR data1,
                              ULONG_PTR data2) {
  return NULL;
}

void PercentEncodeCommas(std::wstring* url) {
  size_t pos = url->find(L",");
  while (pos != std::wstring::npos) {
    url->replace(pos, 1, L"%2C");
    pos = url->find(L",", pos);
  }
}

void PercentUnencodeQuotes(std::wstring* url) {
  base::ReplaceSubstringsAfterOffset(url, 0, L"%27", L"'");
}

std::wstring GetBrowserLocation(const wchar_t* regkey_name) {
  DCHECK(regkey_name);
  base::win::RegKey key;
  if (ERROR_SUCCESS != key.Open(HKEY_LOCAL_MACHINE, regkey_name, KEY_READ) &&
      ERROR_SUCCESS != key.Open(HKEY_CURRENT_USER, regkey_name, KEY_READ)) {
    LOG(ERROR) << "Could not open registry key " << regkey_name
               << "! Error Code:" << GetLastError();
    return std::wstring();
  }
  std::wstring location;
  if (ERROR_SUCCESS != key.ReadValue(NULL, &location))
    return std::wstring();
  return location;
}

const BrowserVarMapping* FindBrowserMapping(std::wstring_view path,
                                            bool compare_typical_executable) {
  // If |compare_typical_executable| is true: also look at executable filenames,
  // to reduce false-negatives when the path is specified explicitly by the
  // admin.
  if (path.empty())
    path = kIEVarName;
  for (const auto& mapping : kBrowserVarMappings) {
    if (!path.compare(mapping.var_name) ||
        (compare_typical_executable && *mapping.typical_executable &&
         base::EndsWith(path, mapping.typical_executable,
                        base::CompareCase::INSENSITIVE_ASCII))) {
      return &mapping;
    }
  }
  return nullptr;
}

void ExpandPresetBrowsers(std::wstring* str) {
  const auto* mapping = FindBrowserMapping(*str, false);
  if (mapping)
    *str = GetBrowserLocation(mapping->registry_key);
}

bool ExpandUrlVarName(std::wstring* arg, const std::wstring& url_spec) {
  size_t url_index = arg->find(kUrlVarName);
  if (url_index == std::wstring::npos)
    return false;
  arg->replace(url_index, wcslen(kUrlVarName), url_spec);
  return true;
}

void AppendCommandLineArguments(base::CommandLine* cmd_line,
                                const std::vector<std::string>& raw_args,
                                const GURL& url) {
  std::wstring url_spec = base::UTF8ToWide(url.spec());
  // IE has some quirks with quote characters. Send them verbatim instead
  // of percent-encoding them.
  PercentUnencodeQuotes(&url_spec);
  std::vector<std::wstring> command_line;
  bool contains_url = false;
  for (const auto& arg : raw_args) {
    auto wide_arg = base::UTF8ToWide(arg);
    auto expanded_arg =
        base::win::ExpandEnvironmentVariables(wide_arg).value_or(
            std::move(wide_arg));
    if (ExpandUrlVarName(&expanded_arg, url_spec))
      contains_url = true;
    cmd_line->AppendArgNative(expanded_arg);
  }
  if (!contains_url)
    cmd_line->AppendArgNative(url_spec);
}

bool IsInternetExplorer(std::string_view path) {
  // We don't treat IExplore.exe as Internet Explorer here. This way, admins can
  // set |AlternativeBrowserPath| to IExplore.exe to disable DDE, if it's
  // causing issues or slowness.
  return path.empty() || base::EqualsASCII(base::as_u16cstr(kIEVarName), path);
}

bool TryLaunchWithDde(const GURL& url, const std::string& path) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::WILL_BLOCK);
  if (!IsInternetExplorer(path))
    return false;

  DWORD dde_instance = 0;
  UINT dml_error =
      DdeInitialize(&dde_instance, DdeCallback, CBF_FAIL_ALLSVRXACTIONS, 0);
  if (dml_error != DMLERR_NO_ERROR) {
    VLOG(1) << "DdeInitialize() failed: " << dml_error;
    return false;
  }

  bool success = false;
  HCONV openurl_service_instance;
  HCONV activate_service_instance;
  {
    HSZ service =
        DdeCreateStringHandle(dde_instance, kIExploreDdeHost, CP_WINUNICODE);
    HSZ openurl_topic =
        DdeCreateStringHandle(dde_instance, L"WWW_OpenURL", CP_WINUNICODE);
    HSZ activate_topic =
        DdeCreateStringHandle(dde_instance, L"WWW_Activate", CP_WINUNICODE);
    openurl_service_instance =
        DdeConnect(dde_instance, service, openurl_topic, NULL);
    activate_service_instance =
        DdeConnect(dde_instance, service, activate_topic, NULL);
    DdeFreeStringHandle(dde_instance, service);
    DdeFreeStringHandle(dde_instance, openurl_topic);
    DdeFreeStringHandle(dde_instance, activate_topic);
  }

  if (openurl_service_instance) {
    // Percent-encode commas and spaces because those mean something else
    // for the WWW_OpenURL verb and the url is trimmed on the first one.
    // Spaces are already encoded by GURL.
    std::wstring encoded_url(base::UTF8ToWide(url.spec()));
    PercentUnencodeQuotes(&encoded_url);
    PercentEncodeCommas(&encoded_url);

    success =
        DdeClientTransaction(
            reinterpret_cast<LPBYTE>(const_cast<wchar_t*>(encoded_url.data())),
            encoded_url.size() * sizeof(wchar_t), openurl_service_instance, 0,
            0, XTYP_EXECUTE, TIMEOUT_ASYNC, NULL) != 0;
    DdeDisconnect(openurl_service_instance);
    if (activate_service_instance) {
      if (success) {
        // Bring window to the front.
        wchar_t cmd[] = L"0xFFFFFFFF,0x0";
        DdeClientTransaction(reinterpret_cast<LPBYTE>(cmd), sizeof(cmd),
                             activate_service_instance, 0, 0, XTYP_EXECUTE,
                             TIMEOUT_ASYNC, NULL);
      }
      DdeDisconnect(activate_service_instance);
    }
  }
  dml_error = ::DdeGetLastError(dde_instance);
  if (dml_error != DMLERR_NO_ERROR)
    VLOG(1) << "DDE error: " << dml_error;
  DdeUninitialize(dde_instance);
  return success;
}

base::CommandLine CreateCommandLine(const GURL& url,
                                    const std::string& utf8_path,
                                    const std::vector<std::string>& params) {
  std::wstring path = base::UTF8ToWide(utf8_path);
  ExpandPresetBrowsers(&path);
  auto expanded_path =
      base::win::ExpandEnvironmentVariables(path).value_or(std::move(path));
  base::CommandLine cmd_line(std::vector<std::wstring>{expanded_path});

  AppendCommandLineArguments(&cmd_line, params, url);

  return cmd_line;
}

bool TryLaunchWithExec(const GURL& url,
                       const std::string& path,
                       const std::vector<std::string>& args) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  CHECK(url.SchemeIsHTTPOrHTTPS() || url.SchemeIsFile());

  auto cmd_line = CreateCommandLine(url, path, args);

  base::LaunchOptions options;
  if (!base::LaunchProcess(cmd_line, options).IsValid()) {
    LOG(ERROR) << "Could not start the alternative browser! Error: "
               << GetLastError();
    return false;
  }
  return true;
}

void TryLaunchBlocking(GURL url,
                       std::string path,
                       std::vector<std::string> params,
                       LaunchCallback cb) {
  const bool success =
      (TryLaunchWithDde(url, path) || TryLaunchWithExec(url, path, params));
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](bool success, LaunchCallback cb) { std::move(cb).Run(success); },
          success, std::move(cb)));
}

}  // namespace

AlternativeBrowserDriver::~AlternativeBrowserDriver() = default;

AlternativeBrowserDriverImpl::AlternativeBrowserDriverImpl(
    const BrowserSwitcherPrefs* prefs)
    : prefs_(prefs) {}

AlternativeBrowserDriverImpl::~AlternativeBrowserDriverImpl() = default;

void AlternativeBrowserDriverImpl::TryLaunch(const GURL& url,
                                             LaunchCallback cb) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  VLOG(2) << "Launching alternative browser...";
  VLOG(2) << "  path = " << prefs_->GetAlternativeBrowserPath();
  VLOG(2) << "  url = " << url.spec();
  base::ThreadPool::PostTask(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::USER_BLOCKING,
       base::TaskShutdownBehavior::BLOCK_SHUTDOWN},
      base::BindOnce(&TryLaunchBlocking, url,
                     prefs_->GetAlternativeBrowserPath(),
                     prefs_->GetAlternativeBrowserParameters(), std::move(cb)));
}

std::string AlternativeBrowserDriverImpl::GetBrowserName() const {
  std::wstring path = base::UTF8ToWide(prefs_->GetAlternativeBrowserPath());
  const auto* mapping = FindBrowserMapping(path, false);
  return mapping ? mapping->browser_name : std::string();
}

BrowserType AlternativeBrowserDriverImpl::GetBrowserType() const {
  std::wstring path = base::UTF8ToWide(prefs_->GetAlternativeBrowserPath());
  const auto* mapping = FindBrowserMapping(path, true);
  return mapping ? mapping->browser_type : BrowserType::kUnknown;
}

base::CommandLine AlternativeBrowserDriverImpl::CreateCommandLine(
    const GURL& url) {
  return browser_switcher::CreateCommandLine(
      url, prefs_->GetAlternativeBrowserPath(),
      prefs_->GetAlternativeBrowserParameters());
}

}  // namespace browser_switcher
