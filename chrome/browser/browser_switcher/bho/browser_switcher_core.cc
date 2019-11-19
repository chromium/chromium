// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser_switcher/bho/browser_switcher_core.h"

#include <Shellapi.h>
#include <ShlObj.h>
#include <WinInet.h>

#include <algorithm>
#include <codecvt>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

#include "chrome/browser/browser_switcher/bho/logging.h"

namespace {

const wchar_t kChromeKey[] =
    L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\App Paths\\chrome.exe";

const wchar_t kChromeVarName[] = L"${chrome}";
const wchar_t kUrlVarName[] = L"${url}";

const wchar_t kWildcardUrl[] = L"*";

const int kMinSupportedFileVersion = 1;
const int kCurrentFileVersion = 1;

const size_t kMaxUrlFilterSize = 10000;

// Reads a line from a file and returns true on success and false otherwise.
bool ReadLineFromFile(std::wifstream* stream, std::wstring* line) {
  if (stream->eof())
    return false;
  std::getline(*stream, *line);
  if (stream->fail())
    return false;
  return true;
}

// Checks if the omitted prefix for a non-fully specified prefix rule is one of
// the expected parts that are allowed to be omitted.
bool IsValidPrefix(const std::wstring& prefix) {
  return (prefix == L"https://") || (prefix == L"https:") ||
         (prefix == L"http://") || (prefix == L"http:") ||
         (prefix == L"file://") || (prefix == L"file:");
}

}  // namespace

BrowserSwitcherCore::BrowserSwitcherCore() {
  Initialize();
}

BrowserSwitcherCore::~BrowserSwitcherCore() {
  ::CloseHandle(site_list_mutex_);
}

bool BrowserSwitcherCore::InvokeChrome(const std::wstring& url) const {
  std::wstring command_line = CompileCommandLine(GetChromeParameters(), url);
  HINSTANCE browser_instance =
      ::ShellExecute(NULL, NULL, chrome_path_.c_str(), command_line.c_str(),
                     NULL, SW_SHOWNORMAL);
  if (reinterpret_cast<int>(browser_instance) <= 32) {
    LOG(ERR) << "Could not start Chrome! Handle: " << browser_instance << " "
             << ::GetLastError() << std::endl;
    return false;
  }
  return true;
}

void BrowserSwitcherCore::SetChromePath(const std::wstring& path) {
  chrome_path_ = path;
  if (chrome_path_.empty() || chrome_path_.compare(kChromeVarName) == 0)
    chrome_path_ = GetBrowserLocation(kChromeKey);
  chrome_path_ = ExpandEnvironmentVariables(chrome_path_);
}

const std::wstring& BrowserSwitcherCore::GetChromePath() const {
  return chrome_path_;
}

void BrowserSwitcherCore::SetChromeParameters(const std::wstring& parameters) {
  chrome_parameters_ = parameters;
  chrome_parameters_ = ExpandEnvironmentVariables(chrome_parameters_);
}

const std::wstring& BrowserSwitcherCore::GetChromeParameters() const {
  return chrome_parameters_;
}

const BrowserSwitcherCore::UrlList& BrowserSwitcherCore::GetUrlsToRedirect()
    const {
  return urls_to_redirect_;
}

void BrowserSwitcherCore::SetUrlsToRedirect(const UrlList& urls) {
  urls_to_redirect_ = urls;
  ProcessUrlList(&urls_to_redirect_, &urls_to_redirect_type_);
}

const BrowserSwitcherCore::UrlList& BrowserSwitcherCore::GetUrlGreylist()
    const {
  return url_greylist_;
}

void BrowserSwitcherCore::SetUrlGreylist(const UrlList& urls) {
  url_greylist_ = urls;
  ProcessUrlList(&url_greylist_, &url_greylist_type_);
}

bool BrowserSwitcherCore::GetIESiteList(
    BrowserSwitcherCore::UrlList* list) const {
  // Wait for max 1s to avoid blocking the caller indefinetely.
  if (site_list_mutex_ &&
      ::WaitForSingleObject(site_list_mutex_, 1000) == WAIT_OBJECT_0) {
    *list = urls_from_site_list_;
    ::ReleaseMutex(site_list_mutex_);
    return true;
  }
  return false;
}

void BrowserSwitcherCore::SetIESiteList(const UrlList& urls) {
  urls_from_site_list_ = urls;
  ProcessUrlList(&urls_from_site_list_, &urls_from_site_list_type_);
}

void BrowserSwitcherCore::ProcessUrlList(UrlList* list,
                                         UrlListTypes* types) const {
  // Sort will push negative entries first because those should have higher
  // priority.
  std::sort(list->begin(), list->end());
  types->resize(list->size());
  for (size_t i = 0; i < list->size(); ++i) {
    if ((*list)[i].compare(kWildcardUrl) == 0) {
      (*types)[i] = WILDCARD;
      continue;
    }

    if ((*list)[i].find('/') != (*list)[i].npos)
      (*types)[i] = PREFIX;
    else
      (*types)[i] = HOST;

    if ((*list)[i].find('!') == 0)
      (*types)[i] = ((*types)[i] == HOST ? NEGATED_HOST : NEGATED_PREFIX);
  }
}

// static
void BrowserSwitcherCore::IsRuleMatching(const std::wstring& url,
                                         const std::wstring& hostname,
                                         const UrlListEntryType& rule_type,
                                         const std::wstring& rule_entry,
                                         TransitionDecision* decision,
                                         bool* all_in_alternative_browser) {
  // Employ a simple, yet powerful heuristic on the entries in the list:
  // If the entry has no slashes it is assumed to be a host name or substring of
  // one. In that case we match only the host part of the url to the entry. If
  // on the other hand we have at least one slash in the string it is assumed to
  // be a proper url prefix like "http://example.com/somepath". In this case we
  // compare the beginning whole url with the list entry (up to a few allowed
  // prefixes that can be omitted (see |IsValidPrefix|). Lastly if the entry
  // starts with a '!' we negate the check. An entry consisting of only '*'
  // means all should be opened in the alternative browser except the negated
  // ones.
  switch (rule_type) {
    case HOST:
      if (hostname.find(rule_entry) != hostname.npos)
        *decision = ALT_BROWSER;
      break;
    case PREFIX: {
      size_t pos = url.find(rule_entry);
      if (pos == 0) {
        *decision = ALT_BROWSER;
      } else if (pos != url.npos) {
        const std::wstring prefix = url.substr(0, pos);
        if (IsValidPrefix(prefix)) {
          *decision = ALT_BROWSER;
        }
      }
      break;
    }
    case NEGATED_HOST:
      if (hostname.find(rule_entry.substr(1)) != hostname.npos)
        *decision = CHROME;
      break;
    case NEGATED_PREFIX: {
      size_t pos = url.find(rule_entry.substr(1));
      if (pos == 0) {
        *decision = CHROME;
      } else if (pos != url.npos) {
        const std::wstring prefix = url.substr(0, pos);
        if (IsValidPrefix(prefix)) {
          *decision = CHROME;
        }
      }
      break;
    }
    case WILDCARD:
      *all_in_alternative_browser = true;
      break;
  }
}

bool BrowserSwitcherCore::ShouldOpenInAlternativeBrowser(
    const std::wstring& url) {
  TransitionDecision decision = NONE;

  // Since we can not decide in this case we should assume it is ok to use the
  // alternative browser.
  if (!HasValidConfiguration())
    return true;
  // In case the url cracking fails at least compare the whole url.
  std::wstring hostname = url;

  URL_COMPONENTS parsed_url;
  memset(&parsed_url, 0, sizeof(parsed_url));
  parsed_url.dwStructSize = sizeof(parsed_url);
  parsed_url.dwHostNameLength = static_cast<DWORD>(-1);
  parsed_url.dwSchemeLength = static_cast<DWORD>(-1);
  parsed_url.dwUrlPathLength = static_cast<DWORD>(-1);
  parsed_url.dwExtraInfoLength = static_cast<DWORD>(-1);
  if (InternetCrackUrl(url.c_str(), 0, 0, &parsed_url))
    hostname.assign(parsed_url.lpszHostName, parsed_url.dwHostNameLength);
  else
    LOG(ERR) << "URL Parsing failed!" << std::endl;

  bool all_in_alternative_browser = false;
  std::wstring decision_rule;
  for (size_t i = 0; i < urls_to_redirect_.size(); ++i) {
    TransitionDecision single_decision = NONE;
    bool single_all_in_alt_browser = false;
    IsRuleMatching(url, hostname, urls_to_redirect_type_[i],
                   urls_to_redirect_[i], &single_decision,
                   &single_all_in_alt_browser);
    if (single_decision != NONE || single_all_in_alt_browser) {
      if (decision_rule.length() < urls_to_redirect_[i].length()) {
        decision_rule = urls_to_redirect_[i];
        decision = single_decision;
        all_in_alternative_browser = single_all_in_alt_browser;
      }
    }
  }

  // Since the gray list can only contribute to staying in the alt and the
  // internal list is higher prio than site list, if there is a decision exit.
  if (decision == ALT_BROWSER || all_in_alternative_browser)
    return true;

  if (decision == NONE && site_list_mutex_) {
    if (::WaitForSingleObject(site_list_mutex_, 500) == WAIT_OBJECT_0) {
      for (size_t i = 0; i < urls_from_site_list_.size(); ++i) {
        TransitionDecision single_decision = NONE;
        bool single_all_in_alt_browser = false;
        IsRuleMatching(url, hostname, urls_from_site_list_type_[i],
                       urls_from_site_list_[i], &single_decision,
                       &single_all_in_alt_browser);
        if (single_decision != NONE || single_all_in_alt_browser) {
          if (decision_rule.length() < urls_from_site_list_[i].length()) {
            decision_rule = urls_from_site_list_[i];
            decision = single_decision;
            all_in_alternative_browser = single_all_in_alt_browser;
          }
        }
      }
      ::ReleaseMutex(site_list_mutex_);

      if (decision == ALT_BROWSER)
        return true;
    }
  }

  for (size_t i = 0; i < url_greylist_.size(); ++i) {
    // See comments on the matching behavior above.
    switch (url_greylist_type_[i]) {
      case HOST:
        // Pick the greylist decision over the other one if it is more precise.
        if (hostname.find(url_greylist_[i]) != hostname.npos) {
          if (decision == NONE ||
              url_greylist_[i].length() > decision_rule.length()) {
            return true;
          }
        }
        break;
      case PREFIX: {
        // Pick the greylist decision over the other one if it is more precise.
        const size_t pos = url.find(url_greylist_[i]);
        if (pos == 0 ||
            (pos != url.npos && IsValidPrefix(url.substr(0, pos)))) {
          if (decision == NONE ||
              url_greylist_[i].length() > decision_rule.length()) {
            return true;
          }
        }
        break;
      }
      // Negative entries have no meaning in the greylist.
      case NEGATED_HOST:
      case NEGATED_PREFIX:
        break;
      case WILDCARD:
        all_in_alternative_browser = true;
        break;
    }
  }

  if (decision != NONE)
    return decision == ALT_BROWSER;
  return all_in_alternative_browser;
}

void BrowserSwitcherCore::Initialize() {
  chrome_path_ = GetBrowserLocation(kChromeKey);
  configuration_valid_ = false;
  if (!LoadConfigFile())
    LOG(ERR) << "Confing file could not be loaded!" << std::endl;
  if (!LoadIESiteListCache())
    LOG(INFO) << "No IE Site List found or file can't be read." << std::endl;

  site_list_mutex_ = ::CreateMutex(NULL, FALSE, NULL);
  if (!site_list_mutex_) {
    LOG(ERR) << "Could not create mutex object for IE Site List thread. "
             << "Site list will not get updated at this run." << std::endl;
  }
}

bool BrowserSwitcherCore::LoadConfigFile() {
  std::wstring path_string = GetConfigFileLocation();
  // Protect against failed config file location retrieval.
  if (path_string.empty())
    return false;

  LOG(INFO) << "Loading cache from : " << path_string.c_str() << std::endl;

  std::wifstream config_file(path_string.c_str());
  if (config_file.bad()) {
    LOG(ERR) << "Can't open config file : " << ::GetLastError() << std::endl;
    return false;
  }

  int file_version = 0;
  config_file >> file_version;
  if (config_file.fail())
    return false;
  LOG(INFO) << "file_version : '" << file_version << "'" << std::endl;
  if (file_version < kMinSupportedFileVersion ||
      file_version > kCurrentFileVersion) {
    return false;
  }
  std::wstring skip_to_eol;
  std::getline(config_file, skip_to_eol);

  std::wstring alternative_browser_path;
  if (!ReadLineFromFile(&config_file, &alternative_browser_path))
    return false;
  LOG(INFO) << "alternative_browser_path : '" << alternative_browser_path
            << "'" << std::endl;
  std::wstring alternative_browser_parameters;
  if (!ReadLineFromFile(&config_file, &alternative_browser_parameters))
    return false;
  LOG(INFO) << "alternative_browser_parameters : '"
            << alternative_browser_parameters << "'" << std::endl;
  std::wstring chrome_path;
  if (!ReadLineFromFile(&config_file, &chrome_path))
    return false;
  LOG(INFO) << "chrome_path : '" << chrome_path << "'" << std::endl;
  std::wstring chrome_parameters;
  if (!ReadLineFromFile(&config_file, &chrome_parameters))
    return false;
  LOG(INFO) << "chrome_parameters : '" << chrome_parameters << "'" << std::endl;

  size_t urls_to_load = 0;
  config_file >> urls_to_load;
  if (config_file.fail())
    return false;
  LOG(INFO) << "url list size : '" << urls_to_load << "'" << std::endl;
  if (urls_to_load > kMaxUrlFilterSize) {
    return false;
  }
  std::getline(config_file, skip_to_eol);

  UrlList urls_to_redirect;
  std::wstring url;
  for (size_t i = 0; i < urls_to_load; ++i) {
    if (!ReadLineFromFile(&config_file, &url))
      return false;
    LOG(INFO) << "url : '" << url << "'" << std::endl;
    urls_to_redirect.push_back(url);
  }

  config_file >> urls_to_load;
  if (config_file.fail())
    return false;
  LOG(INFO) << "url grey list size : '" << urls_to_load << "'" << std::endl;
  if (urls_to_load > kMaxUrlFilterSize) {
    return false;
  }
  std::getline(config_file, skip_to_eol);

  UrlList url_greylist;
  for (size_t i = 0; i < urls_to_load; ++i) {
    if (!ReadLineFromFile(&config_file, &url))
      return false;
    LOG(INFO) << "url : '" << url << "'" << std::endl;
    url_greylist.push_back(url);
  }

  SetChromePath(chrome_path);
  SetChromeParameters(chrome_parameters);
  SetUrlsToRedirect(urls_to_redirect);
  SetUrlGreylist(url_greylist);
  configuration_valid_ = true;
  return true;
}

bool BrowserSwitcherCore::LoadIESiteListCache() {
  std::wstring path_string = GetIESiteListCacheLocation();
  // Protect against failed config file location retrieval.
  if (path_string.empty())
    return false;

  LOG(INFO) << "Loading IE Site List cache from : " << path_string.c_str()
            << std::endl;

  const std::locale wloc(std::locale::classic(),
                         new std::codecvt_utf8_utf16<wchar_t>);
  std::wifstream config_file(path_string.c_str());
  config_file.imbue(wloc);
  if (config_file.bad()) {
    LOG(ERR) << "Can't open config file : " << ::GetLastError() << std::endl;
    return false;
  }

  int file_version = 0;
  config_file >> file_version;
  if (config_file.fail())
    return false;
  LOG(INFO) << "file_version : '" << file_version << "'" << std::endl;
  if (file_version < kMinSupportedFileVersion ||
      file_version > kCurrentFileVersion) {
    return false;
  }
  std::wstring skip_to_eol;
  std::getline(config_file, skip_to_eol);

  size_t urls_to_load = 0;
  config_file >> urls_to_load;
  if (config_file.fail())
    return false;
  LOG(INFO) << "url list size : '" << urls_to_load << "'" << std::endl;
  if (urls_to_load > kMaxUrlFilterSize) {
    return false;
  }
  std::getline(config_file, skip_to_eol);

  UrlList urls_to_redirect;
  std::wstring url;
  for (size_t i = 0; i < urls_to_load; ++i) {
    if (!ReadLineFromFile(&config_file, &url))
      return false;
    LOG(INFO) << "url : '" << url << "'" << std::endl;
    urls_to_redirect.push_back(url);
  }

  SetIESiteList(urls_to_redirect);
  return true;
}

bool BrowserSwitcherCore::HasValidConfiguration() const {
  return configuration_valid_;
}

void BrowserSwitcherCore::SetConfigFileLocationForTest(
    const std::wstring& path) {
  config_file_path_ = path;
  configuration_valid_ = false;
}

void BrowserSwitcherCore::SetIESiteListCacheLocationForTest(
    const std::wstring& path) {
  site_list_cache_file_path_ = path;
}

void BrowserSwitcherCore::SetIESiteListLocationForTest(
    const std::wstring& path) {
  site_list_location_for_test_ = path;
}

std::wstring BrowserSwitcherCore::CompileCommandLine(
    const std::wstring& raw_command_line,
    const std::wstring& url) const {
  std::wstring sanitized_url;
  // In almost every case should this be enough for the sanitization because
  // any ASCII char will expand to at most 3 chars - %[0-9A-F][0-9A-F].
  DWORD length = static_cast<DWORD>(url.length() * 3 + 1);
  std::auto_ptr<wchar_t> buffer(new wchar_t[length]);
  if (!::InternetCanonicalizeUrl(url.c_str(), buffer.get(), &length, 0)) {
    DWORD error = ::GetLastError();
    if (error == ERROR_INSUFFICIENT_BUFFER) {
      // If we get this error it means that the buffer is too small to hold the
      // canoncial url. In that case resize the buffer to what the requested
      // size is (returned in |length| and try again.
      buffer.reset(new wchar_t[length]);
      if (::InternetCanonicalizeUrl(url.c_str(), buffer.get(), &length, 0)) {
        sanitized_url = buffer.get();
      }
    }
  } else {
    sanitized_url = buffer.get();
  }
  // If the API failed, do some poor man's sanitizing at least.
  if (sanitized_url.empty()) {
    LOG(WARNING) << "::InternetCanonicalizeUrl failed : " << ::GetLastError()
                 << std::endl;
    sanitized_url = SanitizeUrl(url);
  }

  std::wstring command_line = raw_command_line;
  size_t pos = command_line.find(kUrlVarName);
  if (pos != command_line.npos) {
    command_line =
        command_line.replace(pos, wcslen(kUrlVarName), sanitized_url);
  } else {
    if (command_line.empty())
      command_line = sanitized_url;
    else
      command_line.append(L" ").append(sanitized_url);
  }
  return command_line;
}

std::wstring BrowserSwitcherCore::SanitizeUrl(const std::wstring url) const {
  // In almost every case should this be enough for the sanitization because
  // any ASCII char will expand to at most 3 chars - %[0-9A-F][0-9A-F].
  std::wstring::const_iterator it = url.begin();
  std::wstring untranslated_chars(L".:/\\_-@~();");
  std::auto_ptr<wchar_t> sanitized_url(new wchar_t[url.length() * 3 + 1]);
  wchar_t* output = sanitized_url.get();

  while (it != url.end()) {
    if (isalnum(*it) || untranslated_chars.find(*it) != std::wstring::npos) {
      *output++ = *it;
    } else {
      // Will only work for ASCII chars but hey it's at least something.
      // Any unicode char will be truncated to its first 8 bits and encoded.
      *output++ = '%';
      int nibble = (*it & 0xf0) >> 4;
      *output++ = nibble > 9 ? nibble - 10 + 'A' : nibble + '0';
      nibble = *it & 0xf;
      *output++ = nibble > 9 ? nibble - 10 + 'A' : nibble + '0';
    }
    it++;
  }
  *output = '\0';

  return std::wstring(sanitized_url.get());
}

std::wstring BrowserSwitcherCore::GetConfigPath() const {
  std::wstring config_path;
  wchar_t path[MAX_PATH];
  if (!::SHGetSpecialFolderPath(0, path, CSIDL_LOCAL_APPDATA, false)) {
    LOG(ERR) << "Error locating %LOCAL_APPDATA%!" << std::endl;
    NOTREACHED();
    return config_path;
  }
  config_path.assign(path);
  ::CreateDirectory(config_path.append(L"\\Google").c_str(), NULL);
  ::CreateDirectory(config_path.append(L"\\BrowserSwitcher").c_str(), NULL);
  return config_path;
}

std::wstring BrowserSwitcherCore::GetConfigFileLocation() {
  if (config_file_path_.empty()) {
    config_file_path_ = GetConfigPath();
    config_file_path_.append(L"\\cache.dat");
  }
  return config_file_path_;
}

std::wstring BrowserSwitcherCore::GetIESiteListCacheLocation() {
  if (site_list_cache_file_path_.empty()) {
    site_list_cache_file_path_ = GetConfigPath();
    site_list_cache_file_path_.append(L"\\sitelistcache.dat");
  }
  return site_list_cache_file_path_;
}

std::wstring BrowserSwitcherCore::GetBrowserLocation(
    const wchar_t* key_name) const {
  HKEY key;
  if (ERROR_SUCCESS != ::RegOpenKey(HKEY_LOCAL_MACHINE, key_name, &key) &&
      ERROR_SUCCESS != ::RegOpenKey(HKEY_CURRENT_USER, key_name, &key)) {
    LOG(ERR) << "Could not open registry key " << key_name
             << "! Error Code:" << ::GetLastError() << std::endl;
    return std::wstring();
  }
  return ReadRegValue(key, NULL);
}

std::wstring BrowserSwitcherCore::ReadRegValue(HKEY key,
                                               const wchar_t* name) const {
  DWORD length = 0;
  if (ERROR_SUCCESS !=
      ::RegQueryValueEx(key, name, NULL, NULL, NULL, &length)) {
    LOG(ERR) << "Could not get size of the value!" << ::GetLastError()
             << std::endl;
    return std::wstring();
  }
  std::auto_ptr<wchar_t> browser_path(new wchar_t[length]);
  if (ERROR_SUCCESS !=
      ::RegQueryValueEx(key, name, NULL, NULL,
                        reinterpret_cast<LPBYTE>(browser_path.get()),
                        &length)) {
    LOG(ERR) << "Could not get the value!" << ::GetLastError() << std::endl;
    return std::wstring();
  }

  return std::wstring(browser_path.get());
}

std::wstring BrowserSwitcherCore::ExpandEnvironmentVariables(
    const std::wstring& str) const {
  std::wstring output = str;
  DWORD expanded_size = 0;
  expanded_size = ::ExpandEnvironmentStrings(str.c_str(), NULL, expanded_size);
  if (expanded_size != 0) {
    // The expected buffer length as defined in MSDN is chars + null + 1.
    std::auto_ptr<wchar_t> expanded_path(new wchar_t[expanded_size + 2]);
    expanded_size = ::ExpandEnvironmentStrings(str.c_str(), expanded_path.get(),
                                               expanded_size);
    if (expanded_size != 0)
      output = expanded_path.get();
  }
  return output;
}
