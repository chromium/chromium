// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>

#include <combaseapi.h>
#include <exdisp.h>
#include <exdispid.h>
#include <guiddef.h>
#include <objbase.h>
#include <oleauto.h>
#include <processthreadsapi.h>
#include <shellapi.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <strsafe.h>
#include <wininet.h>

#include "chrome/browser/browser_switcher/bho/ie_bho_idl.h"
#include "chrome/browser/browser_switcher/bho/mini_bho_util.h"

// memset() and memcmp() needed for debug builds.
void* memset(void* ptr, int value, size_t num) {
  // Naive, but only used in debug builds.
  char* bytes = (char*)ptr;
  for (size_t i = 0; i < num; i++)
    bytes[i] = value;
  return ptr;
}

int memcmp(const void* p1, const void* p2, size_t num) {
  // Naive, but only used in debug builds.
  char* bytes1 = (char*)p1;
  char* bytes2 = (char*)p2;
  for (size_t i = 0; i < num; i++) {
    if (bytes1[i] != bytes2[i])
      return bytes1[i] - bytes2[i];
  }
  return 0;
}

void* operator new(size_t num) {
  return ::HeapAlloc(::GetProcessHeap(), 0, num);
}

void* operator new[](size_t num) {
  return operator new(num);
}

void operator delete(void* ptr) {
  ::HeapFree(::GetProcessHeap(), 0, ptr);
}

void operator delete[](void* ptr) {
  return operator delete(ptr);
}

const wchar_t kHttpPrefix[] = L"http://";
const wchar_t kHttpsPrefix[] = L"https://";
const wchar_t kFilePrefix[] = L"file://";

const wchar_t kChromeKey[] =
    L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\App Paths\\chrome.exe";

// TODO(crbug/950039): Add other varnames (${firefox}, ${opera}, ...)
const wchar_t kChromeVarName[] = L"${chrome}";
const wchar_t kUrlVarName[] = L"${url}";

const char kWildcardUrl[] = "*";

const int kMinSupportedFileVersion = 1;
const int kCurrentFileVersion = 1;

// Puts "AppData\Local\Google\BrowserSwitcher" in |out|. Returns false on
// failure.
//
// Make sure |out| is at least |MAX_PATH| long.
bool GetConfigPath(wchar_t* out) {
  if (!::SHGetSpecialFolderPath(0, out, CSIDL_LOCAL_APPDATA, false)) {
    util::puts("Error locating %LOCAL_APPDATA%!");
    return false;
  }
  return S_OK == ::StringCchCatW(out, MAX_PATH, L"\\Google\\BrowserSwitcher");
}

// Puts "<config_path>\cache.dat" in |out|. Returns false on failure.
//
// Make sure |out| is at least |MAX_PATH| long.
bool GetConfigFileLocation(wchar_t* out) {
  if (!GetConfigPath(out))
    return false;
  return S_OK == ::StringCchCatW(out, MAX_PATH, L"\\cache.dat");
}

// Puts "<config_path>\sitelistcache.dat" in |out|. Returns false on failure.
//
// Make sure |out| is at least |MAX_PATH| long.
bool GetIESitelistCacheLocation(wchar_t* out) {
  if (!GetConfigPath(out))
    return false;
  return S_OK == ::StringCchCatW(out, MAX_PATH, L"\\sitelistcache.dat");
}

// Reads the contents of the text file and returns them as a null-terminated
// string.
//
// Returns an empty string (with capacity=1 and str[0]='\0') if the file is
// empty or doesn't exist.
util::string ReadFileToString(const wchar_t* path) {
  HANDLE file = ::CreateFile(path, GENERIC_READ, FILE_SHARE_READ, nullptr,
                             OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
  size_t file_size = 0;
  if (file != INVALID_HANDLE_VALUE)
    file_size = GetFileSize(file, nullptr);
  util::string contents(file_size + 1);
  contents[file_size] = '\0';
  if (file_size != 0)
    ::ReadFile(file, contents.data(), file_size, nullptr, nullptr);
  return contents;
}

// Checks if the omitted prefix for a non-fully specified prefix rule is one of
// the expected parts that are allowed to be omitted.
bool IsValidPrefix(const char* prefix, size_t prefix_len) {
  static const char* kValidPrefixes[] = {
      "https://", "https:", "http://", "http:", "file://", "file:",
  };
  for (const char* candidate : kValidPrefixes) {
    if (!::StrCmpNA(prefix, candidate, prefix_len))
      return true;
  }
  return false;
}

bool StringContainsInsensitiveASCII(const char* str, const char* target) {
  size_t len = ::lstrlenA(target);
  while (*str) {
    if (!::StrCmpNA(str, target, len))
      return true;
    str++;
  }
  return false;
}

bool UrlMatchesPattern(const char* url,
                       const char* hostname,
                       const char* pattern) {
  if (!::StrCmpA(pattern, kWildcardUrl)) {
    // Wildcard, always match.
    return true;
  }
  if (::StrChrA(pattern, '/') != nullptr) {
    // Check prefix using the normalized URL. Case sensitive, but with
    // case-insensitive scheme/hostname.
    const char* pos = ::StrStrA(url, pattern);
    if (pos == nullptr)
      return false;
    return IsValidPrefix(url, (pos - url));
  }
  // Compare hosts, case-insensitive.
  return StringContainsInsensitiveASCII(hostname, pattern);
}

bool IsInverted(const char* rule) {
  return *rule == '!';
}

util::wstring ReadRegValue(HKEY key, const wchar_t* name) {
  DWORD length = 0;
  if (ERROR_SUCCESS !=
      ::RegQueryValueEx(key, name, nullptr, nullptr, nullptr, &length)) {
    util::printf(ERR, "Could not get the size of the value! %d\n",
                 ::GetLastError());
    return util::empty_wstring();
  }
  util::wstring value(length);
  if (ERROR_SUCCESS != ::RegQueryValueEx(key, name, nullptr, nullptr,
                                         reinterpret_cast<LPBYTE>(value.data()),
                                         &length)) {
    util::printf(ERR, "Could not get the value! %d\n", ::GetLastError());
    return util::empty_wstring();
  }
  return value;
}

util::wstring GetChromeLocation() {
  HKEY key;
  if (ERROR_SUCCESS != ::RegOpenKey(HKEY_LOCAL_MACHINE, kChromeKey, &key) &&
      ERROR_SUCCESS != ::RegOpenKey(HKEY_CURRENT_USER, kChromeKey, &key)) {
    util::printf(ERR, "Could not open registry key %S! Error Code: %d\n",
                 kChromeKey, ::GetLastError());
    return util::empty_wstring();
  }
  return ReadRegValue(key, nullptr);
}

util::wstring SanitizeUrl(const wchar_t* url) {
  // In almost every case should this be enough for the sanitization because
  // any ASCII char will expand to at most 3 chars - %[0-9A-F][0-9A-F].
  const wchar_t untranslated_chars[] = L".:/\\_-@~();";
  util::wstring sanitized_url(::lstrlenW(url) * 3 + 1);

  const wchar_t* it = url;
  wchar_t* output = sanitized_url.data();
  while (*it) {
    if (::IsCharAlphaNumeric(*it) ||
        ::StrChrW(untranslated_chars, *it) != nullptr) {
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
  return sanitized_url;
}

util::wstring ExpandEnvironmentVariables(const wchar_t* str) {
  util::wstring output;
  DWORD expanded_size = 0;
  expanded_size = ::ExpandEnvironmentStrings(str, nullptr, expanded_size);
  if (expanded_size != 0) {
    // The expected buffer length as defined in MSDN is chars + null + 1.
    output = util::wstring(util::max(expanded_size, ::lstrlenW(str)) + 2);
    expanded_size =
        ::ExpandEnvironmentStrings(str, output.data(), expanded_size);
  }
  return output;
}

util::wstring CanonicalizeUrl(const wchar_t* url) {
  // In almost every case should this be enough for the sanitization because
  // any ASCII char will expand to at most 3 chars - %[0-9A-F][0-9A-F].
  DWORD capacity = static_cast<DWORD>(::lstrlenW(url) * 3 + 1);
  util::wstring sanitized_url(capacity);
  if (!::InternetCanonicalizeUrl(url, sanitized_url.data(), &capacity, 0)) {
    DWORD error = ::GetLastError();
    if (error == ERROR_INSUFFICIENT_BUFFER) {
      // If we get this error it means that the buffer is too small to hold
      // the canoncial url. In that case resize the buffer to what the
      // requested size is (returned in |capacity|) and try again.
      sanitized_url = util::wstring(capacity);
      if (!::InternetCanonicalizeUrl(url, sanitized_url.data(), &capacity, 0))
        sanitized_url = util::empty_wstring();
    }
  }
  // If the API failed, do some poor man's sanitizing at least.
  if (sanitized_url[0] == '\0') {
    util::printf(WARNING, "::InternetCanonicalizeUrl failed : %d\n",
                 ::GetLastError());
    sanitized_url = SanitizeUrl(url);
  }
  return sanitized_url;
}

util::wstring CompileCommandLine(const wchar_t* raw_command_line,
                                 const wchar_t* url) {
  util::wstring sanitized_url = CanonicalizeUrl(url);

  // +2 for the extra space we might insert, plus the terminating null char.
  const size_t max_possible_size =
      ::lstrlenW(sanitized_url.data()) + ::lstrlenW(raw_command_line) + 2;
  util::wstring command_line(max_possible_size);
  ::StringCchCopyW(command_line.data(), max_possible_size, raw_command_line);

  bool had_url_placeholder = util::wcs_replace_s(
      command_line.data(), max_possible_size, kUrlVarName, url);
  if (!had_url_placeholder) {
    if (*raw_command_line == '\0') {
      ::StringCchCatW(command_line.data(), max_possible_size,
                      sanitized_url.data());
    } else {
      ::StringCchCatW(command_line.data(), max_possible_size, L" ");
      ::StringCchCatW(command_line.data(), max_possible_size,
                      sanitized_url.data());
    }
  }
  return ExpandEnvironmentVariables(command_line.data());
}

util::wstring ExpandChromePath(const char* orig) {
  util::wstring path = util::utf8_to_utf16(orig);
  if (path[0] == '\0' || !::StrCmpW(path.data(), kChromeVarName))
    path = GetChromeLocation();
  path = ExpandEnvironmentVariables(path.data());
  return path;
}

enum TransitionDecision {
  NONE,
  CHROME,
  ALT_BROWSER,
};

class BrowserSwitcherCore {
 public:
  // Initializes configuration by parsing the file contents.
  explicit BrowserSwitcherCore(util::string&& cache_file_contents,
                               util::string&& sitelistcache_file_contents) {
    cache_file_contents_ = std::move(cache_file_contents);
    sitelistcache_file_contents_ = std::move(sitelistcache_file_contents);

    ParseCacheFile();
    ParseSitelistCacheFile();

    util::printf(DEBUG, "configuration_valid = %s\n",
                 (configuration_valid_ ? "true" : "false"));
  }

  bool ShouldOpenInChrome(const wchar_t* url) {
    return (GetDecision(url) == CHROME);
  }

  bool InvokeChrome(const wchar_t* url) {
    util::wstring path = ExpandChromePath(chrome_path_);
    util::wstring command_line = util::utf8_to_utf16(chrome_parameters_);
    command_line = CompileCommandLine(command_line.data(), url);
    HINSTANCE browser_instance =
        ::ShellExecute(nullptr, nullptr, path.data(), command_line.data(),
                       nullptr, SW_SHOWNORMAL);
    if (reinterpret_cast<int>(browser_instance) <= 32) {
      util::printf(ERR, "Could not start Chrome! Handle: %d %d\n",
                   reinterpret_cast<int>(browser_instance), ::GetLastError());
      return false;
    }
    return true;
  }

 private:
  // Initiates the parsing of |str| and returns the first line of the file, as a
  // null-terminated string (without LF/CRLF).
  //
  // If there is no such line, returns nullptr and flips |configuration_valid_|
  // to false.
  char* GetFirstLine(char* str) {
    char* line = util::strtok(str, "\n");
    if (!line)
      configuration_valid_ = false;
    size_t len = ::lstrlenA(line);
    // Remove '\r' if it was '\r\n'.
    if (line[len - 1] == '\r')
      line[len - 1] = '\0';
    return line;
  }

  // Like GetNextLine(char*), but gets the next line instead of the first one.
  char* GetNextLine() { return GetFirstLine(nullptr); }

  // Read a line that contains a number, and then read that number of lines into
  // a vector (one element for each line).
  util::vector<const char*> ReadList() {
    char* line = GetNextLine();
    if (!line)
      return util::vector<const char*>();
    int list_size;
    bool success = ::StrToIntExA(line, 0, &list_size);
    if (!success) {
      configuration_valid_ = false;
      return util::vector<const char*>();
    }
    util::printf(INFO, "list size : '%d'\n", list_size);
    util::vector<const char*> list = util::vector<const char*>(list_size);
    for (int i = 0; i < list_size; i++) {
      list[i] = GetNextLine();
      util::printf(INFO, "url : '%s'\n", list[i]);
    }
    return list;
  }

  // Parses cache.dat. Sets |configuration_valid_| to false if it fails.
  void ParseCacheFile() {
    util::puts(INFO, "Loading cache.dat");
    char* line = GetFirstLine(cache_file_contents_.data());
    if (!line)
      return;
    int version;
    bool success = ::StrToIntExA(line, 0, &version);
    if (!success || version < kMinSupportedFileVersion ||
        version > kCurrentFileVersion) {
      configuration_valid_ = false;
      return;
    }

    GetNextLine();  // Skip IE path.
    GetNextLine();  // Skip IE parameters.

    chrome_path_ = GetNextLine();
    util::printf(INFO, "chrome_path : '%s'\n", chrome_path_);
    chrome_parameters_ = GetNextLine();
    util::printf(INFO, "chrome_parameters : '%s'\n", chrome_parameters_);
    util::puts(INFO, "Reading url list...");
    cache_.sitelist = ReadList();
    util::puts(INFO, "Reading grey list...");
    cache_.greylist = ReadList();
  }

  // Parses |sitelistcache.dat|. Sets |configuration_valid_| to false if it
  // fails.
  void ParseSitelistCacheFile() {
    util::puts(INFO, "Loading sitelistcache.dat");
    // Special case: don't set |configuration_valid_| to false if the file is
    // straight-up empty or doesn't exist.
    if (sitelistcache_file_contents_[0] == '\0')
      return;

    char* line = GetFirstLine(sitelistcache_file_contents_.data());
    if (!line)
      return;
    int version;
    bool success = StrToIntExA(line, 0, &version);
    if (!success || version < kMinSupportedFileVersion ||
        version > kCurrentFileVersion) {
      configuration_valid_ = false;
      return;
    }
    sitelistcache_.sitelist = ReadList();
    sitelistcache_.greylist = util::vector<const char*>();
  }

  TransitionDecision GetDecision(const wchar_t* url) {
    // Switch to Chrome by default.
    TransitionDecision decision = CHROME;

    // Since we cannot decide in this case we should assume it is ok to use
    // Chrome.
    //
    // XXX: this might be incorrect, and cause redirect loops...
    if (!configuration_valid_)
      return ALT_BROWSER;

    // Only verify the url if is http[s] or file link.
    if (::StrCmpNW(url, kHttpPrefix, ::lstrlenW(kHttpPrefix)) &&
        ::StrCmpNW(url, kHttpsPrefix, ::lstrlenW(kHttpsPrefix)) &&
        ::StrCmpNW(url, kFilePrefix, ::lstrlenW(kFilePrefix))) {
      return NONE;
    }

    util::string utf8_url = util::utf16_to_utf8(url);
    util::string hostname(utf8_url.capacity());
    ::StringCchCopyA(hostname.data(), hostname.capacity(), utf8_url.data());

    URL_COMPONENTS parsed_url = {0};
    parsed_url.dwStructSize = sizeof(parsed_url);
    parsed_url.dwHostNameLength = static_cast<DWORD>(-1);
    parsed_url.dwSchemeLength = static_cast<DWORD>(-1);
    parsed_url.dwUrlPathLength = static_cast<DWORD>(-1);
    parsed_url.dwExtraInfoLength = static_cast<DWORD>(-1);
    if (::InternetCrackUrl(url, 0, 0, &parsed_url)) {
      hostname = util::utf16_to_utf8(parsed_url.lpszHostName);
      hostname[parsed_url.dwHostNameLength] = '\0';
    } else {
      util::puts(ERR, "URL Parsing failed!");
    }

    const RuleSet* rulesets[] = {&cache_, &sitelistcache_};

    size_t decision_rule_length = 0;
    for (const auto* ruleset : rulesets) {
      for (const char* pattern : ruleset->sitelist) {
        size_t pattern_length = ::lstrlenA(pattern);
        if (pattern_length <= decision_rule_length)
          continue;
        bool inverted = IsInverted(pattern);
        // Skip "!" in the pattern.
        if (inverted)
          pattern++;
        if (UrlMatchesPattern(utf8_url.data(), hostname.data(), pattern)) {
          decision = (inverted ? CHROME : ALT_BROWSER);
          decision_rule_length = pattern_length;
        }
      }
    }

    // If the sitelist matches, no need to check the greylist.
    if (decision == ALT_BROWSER)
      return decision;

    for (const auto* ruleset : rulesets) {
      for (const char* pattern : ruleset->greylist) {
        size_t pattern_length = ::lstrlenA(pattern);
        if (pattern_length <= decision_rule_length)
          continue;
        if (UrlMatchesPattern(utf8_url.data(), hostname.data(), pattern)) {
          decision = NONE;
          decision_rule_length = pattern_length;
        }
      }
    }

    return decision;
  }

  // Buffers containing the contents of the config files. Other fields are
  // pointers inside these strings, so keep them in memory.
  util::string cache_file_contents_;
  util::string sitelistcache_file_contents_;

  const char* chrome_path_;
  const char* chrome_parameters_;

  // Each rule is a pointer to a null-terminated string inside
  // |[sitelist_]cache_file_contents_|.
  struct RuleSet {
    util::vector<const char*> sitelist;
    util::vector<const char*> greylist;
  };

  RuleSet cache_;
  RuleSet sitelistcache_;

  // Tracks the validity of the cached configuration. Gets flipped to false if
  // parsing fails during construction.
  bool configuration_valid_ = true;
};

class CBrowserSwitcherBHO final : public IBrowserSwitcherBHO,
                                  public IObjectWithSite {
 public:
  explicit CBrowserSwitcherBHO(BrowserSwitcherCore* core) : core_(core) {
    util::puts(DEBUG, "CBrowserSwitcherBHO()");
  }
  ~CBrowserSwitcherBHO() {
    // Useful for finding leaks.
    util::puts(DEBUG, "~CBrowserSwitcherBHO()");
  }

  //////////////////////////////
  // IDispatch:
  //////////////////////////////
  HRESULT STDMETHODCALLTYPE GetTypeInfoCount(UINT* pctinfo) override {
    // TypeInfo is for chumps.
    *pctinfo = 0;
    return S_OK;
  }

  HRESULT STDMETHODCALLTYPE GetTypeInfo(UINT iTInfo,
                                        LCID lcid,
                                        ITypeInfo** ppTInfo) override {
    if (iTInfo != 0)
      return DISP_E_BADINDEX;
    *ppTInfo = nullptr;
    return S_OK;
  }

  HRESULT STDMETHODCALLTYPE GetIDsOfNames(REFIID riid,
                                          LPOLESTR* rgszNames,
                                          UINT cNames,
                                          LCID lcid,
                                          DISPID* rgDispId) override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE Invoke(DISPID dispIdMember,
                                   REFIID riid,
                                   LCID lcid,
                                   WORD wFlags,
                                   DISPPARAMS* pDispParams,
                                   VARIANT* pVarResult,
                                   EXCEPINFO* pExcepInfo,
                                   UINT* puArgErr) override {
    if (dispIdMember != DISPID_BEFORENAVIGATE2)
      return S_OK;
    BeforeNavigate(
        pDispParams->rgvarg[6].pdispVal, pDispParams->rgvarg[5].pvarVal,
        pDispParams->rgvarg[4].pvarVal, pDispParams->rgvarg[3].pvarVal,
        pDispParams->rgvarg[2].pvarVal, pDispParams->rgvarg[1].pvarVal,
        pDispParams->rgvarg[0].pboolVal);
    return S_OK;
  }

  //////////////////////////////
  // IObjectWithSite:
  //////////////////////////////
  STDMETHODIMP(SetSite(IUnknown* site)) override {
    HRESULT hr = S_OK;
    if (site != nullptr) {
      hr = site->QueryInterface(IID_IWebBrowser2,
                                reinterpret_cast<void**>(&web_browser_));
      if (SUCCEEDED(hr))
        hr = Advise();
    } else {
      Unadvise();
      web_browser_->Release();
    }
    if (site_ != nullptr)
      site_->Release();
    site_ = site;
    if (site_ != nullptr)
      site_->AddRef();
    return hr;
  }

  STDMETHODIMP(GetSite(REFIID riid, void** ppvSite)) override {
    if (site_ == nullptr) {
      *ppvSite = nullptr;
      return E_FAIL;
    }
    return site_->QueryInterface(riid, ppvSite);
  }

  //////////////////////////////
  // IUnknown:
  //////////////////////////////
  STDMETHODIMP(QueryInterface)(REFIID riid, LPVOID* ppv) override {
    if (ppv == nullptr)
      return E_POINTER;
    if (IsEqualIID(riid, IID_IUnknown) ||
        IsEqualIID(riid, IID_IBrowserSwitcherBHO)) {
      *ppv = this;
      AddRef();
      return S_OK;
    }
    if (IsEqualIID(riid, IID_IDispatch)) {
      *ppv = static_cast<IDispatch*>(this);
      AddRef();
      return S_OK;
    }
    if (IsEqualIID(riid, IID_IObjectWithSite)) {
      *ppv = static_cast<IObjectWithSite*>(this);
      AddRef();
      return S_OK;
    }
    *ppv = nullptr;
    return E_NOINTERFACE;
  }

  ULONG STDMETHODCALLTYPE AddRef() override { return ++refcount_; }

  ULONG STDMETHODCALLTYPE Release() override {
    refcount_--;
    if (refcount_ <= 0)
      delete this;
    return refcount_;
  }

 private:
  void STDMETHODCALLTYPE BeforeNavigate(IDispatch* disp,
                                        VARIANT* url,
                                        VARIANT* flags,
                                        VARIANT* target_frame_name,
                                        VARIANT* post_data,
                                        VARIANT* headers,
                                        VARIANT_BOOL* cancel) {
    if (web_browser_ == nullptr || disp == nullptr)
      return;
    IUnknown* unknown1 = nullptr;
    IUnknown* unknown2 = nullptr;
    if (SUCCEEDED(web_browser_->QueryInterface(
            IID_IUnknown, reinterpret_cast<void**>(&unknown1))) &&
        SUCCEEDED(disp->QueryInterface(IID_IUnknown,
                                       reinterpret_cast<void**>(&unknown2)))) {
      // Check if this is the outer frame.
      if (unknown1 == unknown2) {
        bool result = core_->ShouldOpenInChrome(url->bstrVal);
        if (result) {
          if (!core_->InvokeChrome(url->bstrVal)) {
            util::puts(ERR,
                       "Could not invoke alternative browser! "
                       "Will resume loading in IE!");
          } else {
            *cancel = VARIANT_TRUE;
            web_browser_->Quit();
          }
        }
      }
    }
    if (unknown1)
      unknown1->Release();
    if (unknown2)
      unknown2->Release();
  }

  HRESULT GetConnectionPoint(IConnectionPoint** cp) {
    IConnectionPointContainer* cp_container;
    HRESULT hr = web_browser_->QueryInterface(
        IID_IConnectionPointContainer, reinterpret_cast<void**>(&cp_container));
    if (SUCCEEDED(hr))
      hr = cp_container->FindConnectionPoint(DIID_DWebBrowserEvents2, cp);
    return hr;
  }

  HRESULT Advise() {
    IConnectionPoint* cp;
    HRESULT hr = GetConnectionPoint(&cp);
    if (SUCCEEDED(hr))
      hr = cp->Advise(
          static_cast<IUnknown*>(static_cast<IObjectWithSite*>(this)),
          &event_cookie_);
    advised_ = true;
    return hr;
  }

  void Unadvise() {
    if (!advised_)
      return;
    IConnectionPoint* cp;
    HRESULT hr = GetConnectionPoint(&cp);
    if (SUCCEEDED(hr))
      hr = cp->Unadvise(event_cookie_);
    advised_ = false;
  }

  BrowserSwitcherCore* core_;

  DWORD event_cookie_;
  bool advised_ = false;
  IWebBrowser2* web_browser_ = nullptr;
  IUnknown* site_ = nullptr;
  size_t refcount_ = 0;
};

BrowserSwitcherCore* g_browser_switcher_core = nullptr;

class CBrowserSwitcherBHOClass final : public IClassFactory {
 public:
  static CBrowserSwitcherBHOClass* GetInstance() { return &instance_; }

  //////////////////////////////
  // IClassFactory:
  //////////////////////////////
  STDMETHODIMP(CreateInstance)
  (IUnknown* pUnkOuter, REFIID riid, LPVOID* ppvObject) override {
    if (ppvObject == nullptr)
      return E_POINTER;
    auto* bho = new CBrowserSwitcherBHO(g_browser_switcher_core);
    return bho->QueryInterface(riid, ppvObject);
  }

  STDMETHODIMP(LockServer)(BOOL fLock) override { return S_OK; }

  //////////////////////////////
  // IUnknown:
  //////////////////////////////
  STDMETHODIMP(QueryInterface)(REFIID riid, LPVOID* ppv) override {
    if (ppv == nullptr)
      return E_POINTER;
    if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_IClassFactory)) {
      *ppv = this;
      return S_OK;
    }
    *ppv = nullptr;
    return E_NOINTERFACE;
  }

  ULONG STDMETHODCALLTYPE AddRef() override {
    // Not actually refcounted, just a singleton.
    return 1;
  }

  ULONG STDMETHODCALLTYPE Release() override { return 1; }

 private:
  CBrowserSwitcherBHOClass() = default;

  static CBrowserSwitcherBHOClass instance_;
};

CBrowserSwitcherBHOClass CBrowserSwitcherBHOClass::instance_;

extern "C" BOOL WINAPI DllMain(HINSTANCE instance,
                               DWORD reason,
                               LPVOID reserved) {
  switch (reason) {
    case DLL_PROCESS_ATTACH:
      util::InitLog();
      util::puts(DEBUG, "DLL_PROCESS_ATTACH");
      {
        wchar_t config_file[MAX_PATH];
        GetConfigFileLocation(config_file);
        wchar_t sitelist_file[MAX_PATH];
        GetIESitelistCacheLocation(sitelist_file);
        if (g_browser_switcher_core != nullptr)
          delete g_browser_switcher_core;
        g_browser_switcher_core = new BrowserSwitcherCore(
            ReadFileToString(config_file), ReadFileToString(sitelist_file));
      }
      break;
    case DLL_PROCESS_DETACH:
      util::puts(DEBUG, "DLL_PROCESS_DETACH");
      util::CloseLog();
      delete g_browser_switcher_core;
      g_browser_switcher_core = nullptr;
      break;
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    default:
      break;
  }
  return true;
}

STDAPI DllGetClassObject(REFCLSID rclsid, REFIID riid, LPVOID* ppv) {
  if (ppv == nullptr)
    return E_POINTER;
  if (!IsEqualCLSID(rclsid, CLSID_BrowserSwitcherBHO))
    return E_INVALIDARG;
  *ppv = nullptr;
  return CBrowserSwitcherBHOClass::GetInstance()->QueryInterface(riid, ppv);
}
