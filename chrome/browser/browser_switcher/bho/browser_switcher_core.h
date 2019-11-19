// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BROWSER_SWITCHER_BHO_BROWSER_SWITCHER_CORE_H_
#define CHROME_BROWSER_BROWSER_SWITCHER_BHO_BROWSER_SWITCHER_CORE_H_

#include <Windows.h>

#include <string>
#include <vector>

// Implements the browser switching logic for both Chrome and the alternative
// browser.
class BrowserSwitcherCore {
 public:
  // Defines the type of the list of domains to open in the alternative browser.
  typedef std::vector<std::wstring> UrlList;

  BrowserSwitcherCore();
  virtual ~BrowserSwitcherCore();

  // Invokes Chrome and loads |url| in it.
  bool InvokeChrome(const std::wstring& url) const;

  // Setter and getter for Chrome's executable. |path| can be either a fully
  // qualified path to an executable or the following variable. If this variable
  // is used then it should be the only content of the parameter as it will
  // resolve to the fully qualified path of the browser.
  //     ${chrome}  - The default location of Chrome as defined in the registry.
  void SetChromePath(const std::wstring& path);
  const std::wstring& GetChromePath() const;

  // Setter and getter for the Chrome command line parameters.
  // |parameters| can contain the following variable in which case this will be
  // the position of the url to be opened, otherwise it will be appended at the
  // end:
  //     ${url} - The location of the url parameter in the command line.
  void SetChromeParameters(const std::wstring& parameters);
  const std::wstring& GetChromeParameters() const;

  // Setter and getter for the list of urls to be opened in the alternative
  // browser.
  const UrlList& GetUrlsToRedirect() const;
  void SetUrlsToRedirect(const UrlList& urls);

  // Setter and getter for the list of urls to be opened in both browsers. This
  // is the set of urls that should not trigger transition. Such set might be
  // required if for example there are third party authentication pages that
  // need to be accessible by both legacy and normal applications.
  const UrlList& GetUrlGreylist() const;
  void SetUrlGreylist(const UrlList& urls);

  // Setter and getter for the list of urls to be opened in both browsers. This
  // is the set of urls that should not trigger transition. Such set might be
  // required if for example there are third party authentication pages that
  // need to be accessible by both legacy and normal applications.
  bool GetIESiteList(UrlList* list) const;
  void SetIESiteList(const UrlList& urls);

  // Checks if an url should be opened in the alternative browser. Returns true
  // if the hostname (or part of it) of the url is contained in the url lists.
  // This function should be used by external browsers to verify if they should
  // bounce back to Chrome. Chrome itself uses different logic to decide if the
  // url should be opened in the external browser.
  bool ShouldOpenInAlternativeBrowser(const std::wstring& url);

  // Loader for the configuration file.
  bool LoadConfigFile();

  // Loader for the site list cache file. The cache is used to speed up the
  // start time of LBS since retrieving the original list might require network
  // access.
  bool LoadIESiteListCache();

  // Returns true if the configuration has been loaded or saved successfully.
  // Used mainly to verify the course of action in the alternative browser
  // which has no direct access to policy and relies on properly loaded confi-
  // guration.
  bool HasValidConfiguration() const;

 private:
  friend class BrowserSwitcherCoreTest;

  enum TransitionDecision { NONE, CHROME, ALT_BROWSER };

  enum UrlListEntryType {
    HOST,
    PREFIX,
    NEGATED_HOST,
    NEGATED_PREFIX,
    WILDCARD
  };
  typedef std::vector<UrlListEntryType> UrlListTypes;

  // Performs initialization of the class and loads the config file.
  void Initialize();

  // Retrieves the configuration files path based on %LOCALAPPDATA%.
  std::wstring GetConfigPath() const;
  std::wstring GetConfigFileLocation();
  std::wstring GetIESiteListCacheLocation();
  // Used for tests only to mock the config file.
  void SetConfigFileLocationForTest(const std::wstring& path);
  void SetIESiteListCacheLocationForTest(const std::wstring& path);
  void SetIESiteListLocationForTest(const std::wstring& path);

  // Compiles the final command line to start a browser. It will replace the
  // ${url} variable with the supplied url if it is present or append the url to
  // the end if the variable is not present in the command line.
  // The function will also attempt to canonicalize the url to make sure it is
  // not passing potentially dangerous argument to the browser.
  std::wstring CompileCommandLine(const std::wstring& raw_command_line,
                                  const std::wstring& url) const;

  // Poor man's implementation of URL sanitization, used if the call to the
  // WinInet API InternetCanonicalizeUrl fails for some reason.
  std::wstring SanitizeUrl(const std::wstring url) const;

  // Processes a list of url patterns creating a parallel list with the pattern
  // types for each entry. This is done to speed up searching for matches when
  // deciding for redirecting.
  void ProcessUrlList(UrlList* list, UrlListTypes* types) const;

  // Retrieves the location of various browsers. Using the values in the
  // registry under HKLM\SOFTWARE\Microsoft\Windows\CurrentVersion\App Paths\.
  // Returns either the executable location or an empty string if not found.
  std::wstring GetBrowserLocation(const wchar_t* key_name) const;

  // Retrieves a string value from the registry stored at |key| or an empty
  // string if missing.
  std::wstring ReadRegValue(HKEY key, const wchar_t* name) const;

  // Expands any environment variables the input string |str| might contain.
  std::wstring ExpandEnvironmentVariables(const std::wstring& str) const;

  static void IsRuleMatching(const std::wstring& url,
                             const std::wstring& hostname,
                             const UrlListEntryType& rule_type,
                             const std::wstring& rule_entry,
                             TransitionDecision* decision,
                             bool* all_in_alternative_browser);

  std::wstring chrome_path_;

  std::wstring chrome_parameters_;

  UrlList urls_to_redirect_;
  UrlListTypes urls_to_redirect_type_;
  UrlList url_greylist_;
  UrlListTypes url_greylist_type_;
  UrlList urls_from_site_list_;
  UrlListTypes urls_from_site_list_type_;

  HANDLE site_list_mutex_;

  std::wstring config_file_path_;
  std::wstring site_list_cache_file_path_;
  // Tracks the validity of the cached configuration. If a load has succeeded
  // once this status is flipped to true. As long as no load has been successful
  // and no configuration has been set and successfully saved this status stays
  // false.
  bool configuration_valid_;

  // Used to override default sitelist file location for tests.
  std::wstring site_list_location_for_test_;
};

#endif  // CHROME_BROWSER_BROWSER_SWITCHER_BHO_BROWSER_SWITCHER_CORE_H_
