// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/browser_switcher/alternative_browser_driver.h"

#include <stdlib.h>

#include <string_view>

#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/process/launch.h"
#include "base/ranges/algorithm.h"

#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/thread_pool.h"
#include "base/threading/scoped_blocking_call.h"
#include "build/build_config.h"
#include "chrome/browser/browser_switcher/browser_switcher_prefs.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "third_party/re2/src/re2/re2.h"
#include "url/gurl.h"

namespace browser_switcher {

namespace {

using LaunchCallback = AlternativeBrowserDriver::LaunchCallback;

const char kUrlVarName[] = "${url}";

// TODO(crbug.com/40147515): add ${edge} on macOS/Linux once it's released on
// those platforms.

#if BUILDFLAG(IS_MAC)
const char kChromeExecutableName[] = "Google Chrome";
const char kFirefoxExecutableName[] = "Firefox";
const char kOperaExecutableName[] = "Opera";
const char kSafariExecutableName[] = "Safari";
const char kEdgeExecutableName[] = "Microsoft Edge";
#else
const char kChromeExecutableName[] = "google-chrome";
const char kFirefoxExecutableName[] = "firefox";
const char kOperaExecutableName[] = "opera";
#endif

const char kChromeVarName[] = "${chrome}";
const char kFirefoxVarName[] = "${firefox}";
const char kOperaVarName[] = "${opera}";
#if BUILDFLAG(IS_MAC)
const char kSafariVarName[] = "${safari}";
const char kEdgeVarName[] = "${edge}";
#endif

struct BrowserVarMapping {
  const char* var_name;
  const char* executable_name;
  const char* browser_name;
  BrowserType browser_type;
};

const BrowserVarMapping kBrowserVarMappings[] = {
    {kChromeVarName, kChromeExecutableName, "", BrowserType::kChrome},
    {kFirefoxVarName, kFirefoxExecutableName, "Mozilla Firefox",
     BrowserType::kFirefox},
    {kOperaVarName, kOperaExecutableName, "Opera", BrowserType::kOpera},
#if BUILDFLAG(IS_MAC)
    {kSafariVarName, kSafariExecutableName, "Safari", BrowserType::kSafari},
    {kEdgeVarName, kEdgeExecutableName, "Microsoft Edge", BrowserType::kEdge},
#endif
};

bool ExpandUrlVarName(std::string* arg, const GURL& url) {
  size_t url_index = arg->find(kUrlVarName);
  if (url_index == std::string::npos)
    return false;
  arg->replace(url_index, strlen(kUrlVarName), url.spec());
  return true;
}

void ExpandTilde(std::string* arg) {
  if (base::StartsWith(*arg, "~", base::CompareCase::SENSITIVE))
    arg->replace(0, 1, getenv("HOME"));
}

void ExpandEnvironmentVariables(std::string* arg) {
  static re2::LazyRE2 re = {
      "\\$\\{([a-zA-Z_][a-zA-Z_0-9]*)\\}|\\$([a-zA-Z_][a-zA-Z_0-9]*)"};
  std::string out;
  std::string_view view(*arg);
  std::string_view submatch[3] = {};
  size_t start = 0;
  bool matched = false;
  while (re->Match(view, start, arg->size(), re2::RE2::Anchor::UNANCHORED,
                   submatch, std::size(submatch))) {
    out.append(view, start, submatch[0].data() - (arg->data() + start));
    if (submatch[0] == kUrlVarName) {
      // Don't treat '${url}' as an environment variable, leave it as is.
      out.append(kUrlVarName);
    } else {
      std::string var_name((submatch[1].empty() ? submatch[2] : submatch[1]));
      const char* var_value = getenv(var_name.c_str());
      if (var_value != nullptr)
        out.append(var_value);
    }
    start = submatch[0].end() - view.begin();
    matched = true;
  }
  if (!matched)
    return;
  out.append(view.data() + start, view.size() - start);
  std::swap(out, *arg);
}

#if BUILDFLAG(IS_MAC)
bool ContainsUrlVarName(const std::vector<std::string>& tokens) {
  return base::ranges::any_of(tokens, [](const std::string& token) {
    return token.find(kUrlVarName) != std::string::npos;
  });
}
#endif  // BUILDFLAG(IS_MAC)

void AppendCommandLineArguments(base::CommandLine* cmd_line,
                                const std::vector<std::string>& raw_args,
                                const GURL& url,
                                bool always_append_url) {
  bool contains_url = false;
  for (const auto& arg : raw_args) {
    std::string expanded_arg = arg;
    ExpandTilde(&expanded_arg);
    ExpandEnvironmentVariables(&expanded_arg);
    if (ExpandUrlVarName(&expanded_arg, url))
      contains_url = true;
    cmd_line->AppendArg(expanded_arg);
  }
  if (always_append_url && !contains_url)
    cmd_line->AppendArg(url.spec());
}

const BrowserVarMapping* FindBrowserMapping(std::string_view path) {
#if BUILDFLAG(IS_MAC)
  // Unlike most POSIX platforms, MacOS always has another browser than Chrome,
  // so admins don't have to explicitly configure one.
  if (path.empty())
    path = kSafariVarName;
#endif
  for (const auto& mapping : kBrowserVarMappings) {
    if (!path.compare(mapping.var_name))
      return &mapping;
  }
  return nullptr;
}

void ExpandPresetBrowsers(std::string* str) {
  const auto* mapping = FindBrowserMapping(*str);
  if (mapping)
    *str = mapping->executable_name;
}

base::CommandLine CreateCommandLine(const GURL& url,
                                    const std::string& original_path,
                                    const std::vector<std::string>& params) {
  std::string path = original_path;
  ExpandPresetBrowsers(&path);
  ExpandTilde(&path);
  ExpandEnvironmentVariables(&path);

#if BUILDFLAG(IS_MAC)
  // On MacOS, if the path doesn't start with a '/', it's probably not an
  // executable path. It is probably a name for an application, e.g. "Safari" or
  // "Google Chrome". Those can be launched using the `open(1)' command.
  //
  // It may use the following syntax (first syntax):
  //     open -a <browser_path> <url> [--args <browser_params...>]
  //
  // Or, if |browser_params| contains "${url}" (second syntax):
  //     open -a <browser_path> --args <browser_params...>
  //
  // Safari only supports the first syntax.
  if (!path.empty() && path[0] != '/') {
    base::CommandLine cmd_line(std::vector<std::string>{"open"});
    cmd_line.AppendArg("-a");
    cmd_line.AppendArg(path);
    if (!ContainsUrlVarName(params)) {
      // First syntax.
      cmd_line.AppendArg(url.spec());
    }
    if (!params.empty()) {
      // First or second syntax, depending on what is in |browser_params_|.
      cmd_line.AppendArg("--args");
      AppendCommandLineArguments(&cmd_line, params, url,
                                 /* always_append_url */ false);
    }
    return cmd_line;
  }
#endif
  base::CommandLine cmd_line(std::vector<std::string>{path});
  AppendCommandLineArguments(&cmd_line, params, url,
                             /* always_append_url */ true);
  return cmd_line;
}

void TryLaunchBlocking(GURL url,
                       std::string path,
                       std::vector<std::string> params,
                       LaunchCallback cb) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  CHECK(url.SchemeIsHTTPOrHTTPS() || url.SchemeIsFile());

  auto cmd_line = CreateCommandLine(url, path, params);
  base::LaunchOptions options;
  // Don't close the alternative browser when Chrome exits.
  options.new_process_group = true;
  const bool success = base::LaunchProcess(cmd_line, options).IsValid();
  if (!success)
    LOG(ERROR) << "Could not start the alternative browser!";

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
#if !BUILDFLAG(IS_MAC)
  if (prefs_->GetAlternativeBrowserPath().empty()) {
    LOG(ERROR) << "Alternative browser not configured. "
               << "Aborting browser switch.";
    std::move(cb).Run(false);
    return;
  }
#endif

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
  std::string path = prefs_->GetAlternativeBrowserPath();
  const auto* mapping = FindBrowserMapping(path);
  return mapping ? mapping->browser_name : std::string();
}

BrowserType AlternativeBrowserDriverImpl::GetBrowserType() const {
  std::string path = prefs_->GetAlternativeBrowserPath();
  const auto* mapping = FindBrowserMapping(path);
  return mapping ? mapping->browser_type : BrowserType::kUnknown;
}

base::CommandLine AlternativeBrowserDriverImpl::CreateCommandLine(
    const GURL& url) {
  return browser_switcher::CreateCommandLine(
      url, prefs_->GetAlternativeBrowserPath(),
      prefs_->GetAlternativeBrowserParameters());
}

}  // namespace browser_switcher
