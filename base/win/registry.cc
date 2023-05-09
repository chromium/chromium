// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/win/registry.h"

#include <stddef.h>

#include <algorithm>
#include <iterator>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/check_op.h"
#include "base/containers/fixed_flat_map.h"
#include "base/functional/callback.h"
#include "base/native_library.h"
#include "base/notreached.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/strings/string_util_win.h"
#include "base/threading/thread_restrictions.h"
#include "base/win/object_watcher.h"
#include "base/win/pe_image.h"
#include "base/win/scoped_handle.h"
#include "base/win/shlwapi.h"

namespace base::win {

namespace {

// RegEnumValue() reports the number of characters from the name that were
// written to the buffer, not how many there are. This constant is the maximum
// name size, such that a buffer with this size should read any name.
constexpr DWORD MAX_REGISTRY_NAME_SIZE = 16384;

// Registry values are read as BYTE* but can have wchar_t* data whose last
// wchar_t is truncated. This function converts the reported |byte_size| to
// a size in wchar_t that can store a truncated wchar_t if necessary.
inline DWORD to_wchar_size(DWORD byte_size) {
  return (byte_size + sizeof(wchar_t) - 1) / sizeof(wchar_t);
}

// Mask to pull WOW64 access flags out of REGSAM access.
constexpr REGSAM kWow64AccessMask = KEY_WOW64_32KEY | KEY_WOW64_64KEY;

constexpr DWORD kInvalidIterValue = static_cast<DWORD>(-1);

}  // namespace

namespace internal {

// A forwarder to the normal delayloaded Windows Registry API.
class Standard {
 public:
  static inline LSTATUS CreateKey(HKEY hKey,
                                  LPCWSTR lpSubKey,
                                  DWORD Reserved,
                                  LPWSTR lpClass,
                                  DWORD dwOptions,
                                  REGSAM samDesired,
                                  CONST LPSECURITY_ATTRIBUTES
                                      lpSecurityAttributes,
                                  PHKEY phkResult,
                                  LPDWORD lpdwDisposition) {
    return ::RegCreateKeyExW(hKey, lpSubKey, Reserved, lpClass, dwOptions,
                             samDesired, lpSecurityAttributes, phkResult,
                             lpdwDisposition);
  }

  static inline LSTATUS OpenKey(HKEY hKey,
                                LPCWSTR lpSubKey,
                                DWORD ulOptions,
                                REGSAM samDesired,
                                PHKEY phkResult) {
    return ::RegOpenKeyExW(hKey, lpSubKey, ulOptions, samDesired, phkResult);
  }

  static inline LSTATUS DeleteKey(HKEY hKey,
                                  LPCWSTR lpSubKey,
                                  REGSAM samDesired,
                                  DWORD Reserved) {
    return ::RegDeleteKeyExW(hKey, lpSubKey, samDesired, Reserved);
  }

  static inline LSTATUS QueryInfoKey(HKEY hKey,
                                     LPWSTR lpClass,
                                     LPDWORD lpcchClass,
                                     LPDWORD lpReserved,
                                     LPDWORD lpcSubKeys,
                                     LPDWORD lpcbMaxSubKeyLen,
                                     LPDWORD lpcbMaxClassLen,
                                     LPDWORD lpcValues,
                                     LPDWORD lpcbMaxValueNameLen,
                                     LPDWORD lpcbMaxValueLen,
                                     LPDWORD lpcbSecurityDescriptor,
                                     PFILETIME lpftLastWriteTime) {
    return ::RegQueryInfoKeyW(hKey, lpClass, lpcchClass, lpReserved, lpcSubKeys,
                              lpcbMaxSubKeyLen, lpcbMaxClassLen, lpcValues,
                              lpcbMaxValueNameLen, lpcbMaxValueLen,
                              lpcbSecurityDescriptor, lpftLastWriteTime);
  }

  static inline LSTATUS EnumKey(HKEY hKey,
                                DWORD dwIndex,
                                LPWSTR lpName,
                                LPDWORD lpcchName,
                                LPDWORD lpReserved,
                                LPWSTR lpClass,
                                LPDWORD lpcchClass,
                                PFILETIME lpftLastWriteTime) {
    return ::RegEnumKeyExW(hKey, dwIndex, lpName, lpcchName, lpReserved,
                           lpClass, lpcchClass, lpftLastWriteTime);
  }

  static inline LSTATUS CloseKey(HKEY hKey) { return ::RegCloseKey(hKey); }

  static inline LSTATUS QueryValue(HKEY hKey,
                                   LPCWSTR lpValueName,
                                   LPDWORD lpReserved,
                                   LPDWORD lpType,
                                   LPBYTE lpData,
                                   LPDWORD lpcbData) {
    return ::RegQueryValueExW(hKey, lpValueName, lpReserved, lpType, lpData,
                              lpcbData);
  }

  static inline LSTATUS SetValue(HKEY hKey,
                                 LPCWSTR lpValueName,
                                 DWORD Reserved,
                                 DWORD dwType,
                                 CONST BYTE* lpData,
                                 DWORD cbData) {
    return ::RegSetValueExW(hKey, lpValueName, Reserved, dwType, lpData,
                            cbData);
  }

  static inline LSTATUS DeleteValue(HKEY hKey, LPCWSTR lpValueName) {
    return ::RegDeleteValueW(hKey, lpValueName);
  }

  static inline LSTATUS EnumValue(HKEY hKey,
                                  DWORD dwIndex,
                                  LPWSTR lpValueName,
                                  LPDWORD lpcchValueName,
                                  LPDWORD lpReserved,
                                  LPDWORD lpType,
                                  LPBYTE lpData,
                                  LPDWORD lpcbData) {
    return ::RegEnumValueW(hKey, dwIndex, lpValueName, lpcchValueName,
                           lpReserved, lpType, lpData, lpcbData);
  }
};

// An implementation derived from the export table of advapi32.
class ExportDerived {
 public:
  static LSTATUS CreateKey(HKEY hKey,
                           LPCWSTR lpSubKey,
                           DWORD Reserved,
                           LPWSTR lpClass,
                           DWORD dwOptions,
                           REGSAM samDesired,
                           CONST LPSECURITY_ATTRIBUTES lpSecurityAttributes,
                           PHKEY phkResult,
                           LPDWORD lpdwDisposition) {
    if (!ResolveRegistryFunctions() || !reg_create_key_ex_) {
      return ERROR_ERRORS_ENCOUNTERED;
    }
    return reg_create_key_ex_(hKey, lpSubKey, Reserved, lpClass, dwOptions,
                              samDesired, lpSecurityAttributes, phkResult,
                              lpdwDisposition);
  }

  static LSTATUS OpenKey(HKEY hKey,
                         LPCWSTR lpSubKey,
                         DWORD ulOptions,
                         REGSAM samDesired,
                         PHKEY phkResult) {
    if (!ResolveRegistryFunctions() || !reg_open_key_ex_) {
      return ERROR_ERRORS_ENCOUNTERED;
    }
    return reg_open_key_ex_(hKey, lpSubKey, ulOptions, samDesired, phkResult);
  }

  static LSTATUS DeleteKey(HKEY hKey,
                           LPCWSTR lpSubKey,
                           REGSAM samDesired,
                           DWORD Reserved) {
    if (!ResolveRegistryFunctions() || !reg_delete_key_ex_) {
      return ERROR_ERRORS_ENCOUNTERED;
    }
    return reg_delete_key_ex_(hKey, lpSubKey, samDesired, Reserved);
  }

  static LSTATUS QueryInfoKey(HKEY hKey,
                              LPWSTR lpClass,
                              LPDWORD lpcchClass,
                              LPDWORD lpReserved,
                              LPDWORD lpcSubKeys,
                              LPDWORD lpcbMaxSubKeyLen,
                              LPDWORD lpcbMaxClassLen,
                              LPDWORD lpcValues,
                              LPDWORD lpcbMaxValueNameLen,
                              LPDWORD lpcbMaxValueLen,
                              LPDWORD lpcbSecurityDescriptor,
                              PFILETIME lpftLastWriteTime) {
    if (!ResolveRegistryFunctions() || !reg_query_info_key_) {
      return ERROR_ERRORS_ENCOUNTERED;
    }
    return reg_query_info_key_(hKey, lpClass, lpcchClass, lpReserved,
                               lpcSubKeys, lpcbMaxSubKeyLen, lpcbMaxClassLen,
                               lpcValues, lpcbMaxValueNameLen, lpcbMaxValueLen,
                               lpcbSecurityDescriptor, lpftLastWriteTime);
  }

  static LSTATUS EnumKey(HKEY hKey,
                         DWORD dwIndex,
                         LPWSTR lpName,
                         LPDWORD lpcchName,
                         LPDWORD lpReserved,
                         LPWSTR lpClass,
                         LPDWORD lpcchClass,
                         PFILETIME lpftLastWriteTime) {
    if (!ResolveRegistryFunctions() || !reg_enum_key_ex_) {
      return ERROR_ERRORS_ENCOUNTERED;
    }
    return reg_enum_key_ex_(hKey, dwIndex, lpName, lpcchName, lpReserved,
                            lpClass, lpcchClass, lpftLastWriteTime);
  }

  static LSTATUS CloseKey(HKEY hKey) {
    if (!ResolveRegistryFunctions() || !reg_close_key_) {
      return ERROR_ERRORS_ENCOUNTERED;
    }
    return reg_close_key_(hKey);
  }

  static LSTATUS QueryValue(HKEY hKey,
                            LPCWSTR lpValueName,
                            LPDWORD lpReserved,
                            LPDWORD lpType,
                            LPBYTE lpData,
                            LPDWORD lpcbData) {
    if (!ResolveRegistryFunctions() || !reg_query_value_ex_) {
      return ERROR_ERRORS_ENCOUNTERED;
    }
    return reg_query_value_ex_(hKey, lpValueName, lpReserved, lpType, lpData,
                               lpcbData);
  }

  static LSTATUS SetValue(HKEY hKey,
                          LPCWSTR lpValueName,
                          DWORD Reserved,
                          DWORD dwType,
                          CONST BYTE* lpData,
                          DWORD cbData) {
    if (!ResolveRegistryFunctions() || !reg_set_value_ex_) {
      return ERROR_ERRORS_ENCOUNTERED;
    }
    return reg_set_value_ex_(hKey, lpValueName, Reserved, dwType, lpData,
                             cbData);
  }

  static LSTATUS DeleteValue(HKEY hKey, LPCWSTR lpValueName) {
    if (!ResolveRegistryFunctions() || !reg_delete_value_) {
      return ERROR_ERRORS_ENCOUNTERED;
    }
    return reg_delete_value_(hKey, lpValueName);
  }
  static LSTATUS EnumValue(HKEY hKey,
                           DWORD dwIndex,
                           LPWSTR lpValueName,
                           LPDWORD lpcchValueName,
                           LPDWORD lpReserved,
                           LPDWORD lpType,
                           LPBYTE lpData,
                           LPDWORD lpcbData) {
    if (!ResolveRegistryFunctions() || !reg_enum_value_) {
      return ERROR_ERRORS_ENCOUNTERED;
    }

    return reg_enum_value_(hKey, dwIndex, lpValueName, lpcchValueName,
                           lpReserved, lpType, lpData, lpcbData);
  }

 private:
  static bool ProcessOneExport(const base::win::PEImage& image,
                               DWORD ordinal,
                               DWORD hint,
                               LPCSTR name,
                               PVOID function_addr,
                               LPCSTR forward,
                               PVOID cookie) {
    if (!name || !function_addr) {
      return true;
    }

    static const auto kMap =
        base::MakeFixedFlatMapSorted<base::StringPiece, void**>({
            {"RegCloseKey", reinterpret_cast<void**>(&reg_close_key_)},
            {"RegCreateKeyExW", reinterpret_cast<void**>(&reg_create_key_ex_)},
            {"RegDeleteKeyExW", reinterpret_cast<void**>(&reg_delete_key_ex_)},
            {"RegDeleteValueW", reinterpret_cast<void**>(&reg_delete_value_)},
            {"RegEnumKeyExW", reinterpret_cast<void**>(&reg_enum_key_ex_)},
            {"RegEnumValueW", reinterpret_cast<void**>(&reg_enum_value_)},
            {"RegOpenKeyExW", reinterpret_cast<void**>(&reg_open_key_ex_)},
            {"RegQueryInfoKeyW",
             reinterpret_cast<void**>(&reg_query_info_key_)},
            {"RegQueryValueExW",
             reinterpret_cast<void**>(&reg_query_value_ex_)},
            {"RegSetValueExW", reinterpret_cast<void**>(&reg_set_value_ex_)},
        });

    auto* entry = kMap.find(name);
    if (entry == kMap.end()) {
      return true;
    }

    static size_t num_init_functions = 0;
    if (!std::exchange(*(entry->second), function_addr)) {
      ++num_init_functions;
    }

    bool& fully_resolved = *static_cast<bool*>(cookie);
    fully_resolved = num_init_functions == kMap.size();
    return !fully_resolved;
  }

  static bool ResolveRegistryFunctions() {
    static bool initialized = []() {
      base::NativeLibraryLoadError error;
      HMODULE advapi32 = base::PinSystemLibrary(L"advapi32.dll", &error);
      if (!advapi32 || error.code) {
        return false;
      }
      bool fully_resolved = false;
      base::win::PEImage(advapi32).EnumExports(&ProcessOneExport,
                                               &fully_resolved);
      return fully_resolved;
    }();
    return initialized;
  }

  static decltype(::RegCreateKeyExW)* reg_create_key_ex_;
  static decltype(::RegOpenKeyExW)* reg_open_key_ex_;
  static decltype(::RegDeleteKeyExW)* reg_delete_key_ex_;
  static decltype(::RegQueryInfoKeyW)* reg_query_info_key_;
  static decltype(::RegEnumKeyExW)* reg_enum_key_ex_;
  static decltype(::RegCloseKey)* reg_close_key_;
  static decltype(::RegQueryValueExW)* reg_query_value_ex_;
  static decltype(::RegSetValueExW)* reg_set_value_ex_;
  static decltype(::RegDeleteValueW)* reg_delete_value_;
  static decltype(::RegEnumValueW)* reg_enum_value_;
};

decltype(::RegCreateKeyEx)* ExportDerived::reg_create_key_ex_ = nullptr;
decltype(::RegOpenKeyExW)* ExportDerived::reg_open_key_ex_ = nullptr;
decltype(::RegDeleteKeyExW)* ExportDerived::reg_delete_key_ex_ = nullptr;
decltype(::RegQueryInfoKeyW)* ExportDerived::reg_query_info_key_ = nullptr;
decltype(::RegEnumKeyExW)* ExportDerived::reg_enum_key_ex_ = nullptr;
decltype(::RegCloseKey)* ExportDerived::reg_close_key_ = nullptr;
decltype(::RegQueryValueEx)* ExportDerived::reg_query_value_ex_ = nullptr;
decltype(::RegSetValueExW)* ExportDerived::reg_set_value_ex_ = nullptr;
decltype(::RegDeleteValueW)* ExportDerived::reg_delete_value_ = nullptr;
decltype(::RegEnumValueW)* ExportDerived::reg_enum_value_ = nullptr;

// Watches for modifications to a key.
template <typename Reg>
class GenericRegKey<Reg>::Watcher : public ObjectWatcher::Delegate {
 public:
  Watcher() = default;

  Watcher(const Watcher&) = delete;
  Watcher& operator=(const Watcher&) = delete;

  ~Watcher() override = default;

  bool StartWatching(HKEY key, ChangeCallback callback);

  // ObjectWatcher::Delegate:
  void OnObjectSignaled(HANDLE object) override {
    DCHECK(watch_event_.is_valid());
    DCHECK_EQ(watch_event_.get(), object);
    std::move(callback_).Run();
  }

 private:
  ScopedHandle watch_event_;
  ObjectWatcher object_watcher_;
  ChangeCallback callback_;
};

template <typename Reg>
bool GenericRegKey<Reg>::Watcher::StartWatching(HKEY key,
                                                ChangeCallback callback) {
  DCHECK(key);
  DCHECK(callback_.is_null());

  if (!watch_event_.is_valid()) {
    watch_event_.Set(CreateEvent(nullptr, TRUE, FALSE, nullptr));
  }

  if (!watch_event_.is_valid()) {
    return false;
  }

  DWORD filter = REG_NOTIFY_CHANGE_NAME | REG_NOTIFY_CHANGE_ATTRIBUTES |
                 REG_NOTIFY_CHANGE_LAST_SET | REG_NOTIFY_CHANGE_SECURITY |
                 REG_NOTIFY_THREAD_AGNOSTIC;
  // Watch the registry key for a change of value.
  LONG result =
      RegNotifyChangeKeyValue(key, /*bWatchSubtree=*/TRUE, filter,
                              watch_event_.get(), /*fAsynchronous=*/TRUE);
  if (result != ERROR_SUCCESS) {
    watch_event_.Close();
    return false;
  }

  callback_ = std::move(callback);
  return object_watcher_.StartWatchingOnce(watch_event_.get(), this);
}

// GenericRegKey<Reg>
// ----------------------------------------------------------------------

template <typename Reg>
GenericRegKey<Reg>::GenericRegKey() = default;

template <typename Reg>
GenericRegKey<Reg>::GenericRegKey(HKEY key) : key_(key) {}

template <typename Reg>
GenericRegKey<Reg>::GenericRegKey(HKEY rootkey,
                                  const wchar_t* subkey,
                                  REGSAM access) {
  if (rootkey) {
    if (access & (KEY_SET_VALUE | KEY_CREATE_SUB_KEY | KEY_CREATE_LINK)) {
      Create(rootkey, subkey, access);
    } else {
      Open(rootkey, subkey, access);
    }
  } else {
    DCHECK(!subkey);
    wow64access_ = access & kWow64AccessMask;
  }
}

template <typename Reg>
GenericRegKey<Reg>::GenericRegKey(GenericRegKey<Reg>&& other) noexcept
    : key_(other.key_),
      wow64access_(other.wow64access_),
      key_watcher_(std::move(other.key_watcher_)) {
  other.key_ = nullptr;
  other.wow64access_ = 0;
}

template <typename Reg>
GenericRegKey<Reg>& GenericRegKey<Reg>::operator=(GenericRegKey<Reg>&& other) {
  Close();
  std::swap(key_, other.key_);
  std::swap(wow64access_, other.wow64access_);
  key_watcher_ = std::move(other.key_watcher_);
  return *this;
}

template <typename Reg>
GenericRegKey<Reg>::~GenericRegKey() {
  Close();
}

template <typename Reg>
LONG GenericRegKey<Reg>::Create(HKEY rootkey,
                                const wchar_t* subkey,
                                REGSAM access) {
  DWORD disposition_value;
  return CreateWithDisposition(rootkey, subkey, &disposition_value, access);
}

template <typename Reg>
LONG GenericRegKey<Reg>::CreateWithDisposition(HKEY rootkey,
                                               const wchar_t* subkey,
                                               DWORD* disposition,
                                               REGSAM access) {
  DCHECK(rootkey && subkey && access && disposition);
  HKEY subhkey = nullptr;
  LONG result =
      Reg::CreateKey(rootkey, subkey, 0, nullptr, REG_OPTION_NON_VOLATILE,
                     access, nullptr, &subhkey, disposition);
  if (result == ERROR_SUCCESS) {
    Close();
    key_ = subhkey;
    wow64access_ = access & kWow64AccessMask;
  }

  return result;
}

template <typename Reg>
LONG GenericRegKey<Reg>::CreateKey(const wchar_t* name, REGSAM access) {
  DCHECK(name && access);
  // After the application has accessed an alternate registry view using one
  // of the [KEY_WOW64_32KEY / KEY_WOW64_64KEY] flags, all subsequent
  // operations (create, delete, or open) on child registry keys must
  // explicitly use the same flag. Otherwise, there can be unexpected
  // behavior.
  // http://msdn.microsoft.com/en-us/library/windows/desktop/aa384129.aspx.
  if ((access & kWow64AccessMask) != wow64access_) {
    NOTREACHED();
    return ERROR_INVALID_PARAMETER;
  }
  HKEY subkey = nullptr;
  LONG result = Reg::CreateKey(key_, name, 0, nullptr, REG_OPTION_NON_VOLATILE,
                               access, nullptr, &subkey, nullptr);
  if (result == ERROR_SUCCESS) {
    Close();
    key_ = subkey;
    wow64access_ = access & kWow64AccessMask;
  }

  return result;
}

template <typename Reg>
LONG GenericRegKey<Reg>::Open(HKEY rootkey,
                              const wchar_t* subkey,
                              REGSAM access) {
  DCHECK(rootkey && subkey && access);
  HKEY subhkey = nullptr;

  LONG result = Reg::OpenKey(rootkey, subkey, 0, access, &subhkey);
  if (result == ERROR_SUCCESS) {
    Close();
    key_ = subhkey;
    wow64access_ = access & kWow64AccessMask;
  }

  return result;
}

template <typename Reg>
LONG GenericRegKey<Reg>::OpenKey(const wchar_t* relative_key_name,
                                 REGSAM access) {
  DCHECK(relative_key_name && access);
  // After the application has accessed an alternate registry view using one
  // of the [KEY_WOW64_32KEY / KEY_WOW64_64KEY] flags, all subsequent
  // operations (create, delete, or open) on child registry keys must
  // explicitly use the same flag. Otherwise, there can be unexpected
  // behavior.
  // http://msdn.microsoft.com/en-us/library/windows/desktop/aa384129.aspx.
  if ((access & kWow64AccessMask) != wow64access_) {
    NOTREACHED();
    return ERROR_INVALID_PARAMETER;
  }
  HKEY subkey = nullptr;
  LONG result = Reg::OpenKey(key_, relative_key_name, 0, access, &subkey);

  // We have to close the current opened key before replacing it with the new
  // one.
  if (result == ERROR_SUCCESS) {
    Close();
    key_ = subkey;
    wow64access_ = access & kWow64AccessMask;
  }
  return result;
}

template <typename Reg>
void GenericRegKey<Reg>::Close() {
  if (key_) {
    Reg::CloseKey(key_);
    key_ = nullptr;
    wow64access_ = 0;
  }
}

// TODO(wfh): Remove this and other unsafe methods. See
// http://crbug.com/375400
template <typename Reg>
void GenericRegKey<Reg>::Set(HKEY key) {
  if (key_ != key) {
    Close();
    key_ = key;
  }
}

template <typename Reg>
HKEY GenericRegKey<Reg>::Take() {
  DCHECK_EQ(wow64access_, 0u);
  HKEY key = key_;
  key_ = nullptr;
  return key;
}

template <typename Reg>
bool GenericRegKey<Reg>::HasValue(const wchar_t* name) const {
  return Reg::QueryValue(key_, name, nullptr, nullptr, nullptr, nullptr) ==
         ERROR_SUCCESS;
}

template <typename Reg>
DWORD GenericRegKey<Reg>::GetValueCount() const {
  DWORD count = 0;
  LONG result =
      Reg::QueryInfoKey(key_, nullptr, nullptr, nullptr, nullptr, nullptr,
                        nullptr, &count, nullptr, nullptr, nullptr, nullptr);
  return (result == ERROR_SUCCESS) ? count : 0;
}

template <typename Reg>
FILETIME GenericRegKey<Reg>::GetLastWriteTime() const {
  FILETIME last_write_time;
  LONG result = Reg::QueryInfoKey(key_, nullptr, nullptr, nullptr, nullptr,
                                  nullptr, nullptr, nullptr, nullptr, nullptr,
                                  nullptr, &last_write_time);
  return (result == ERROR_SUCCESS) ? last_write_time : FILETIME{};
}

template <typename Reg>
LONG GenericRegKey<Reg>::GetValueNameAt(DWORD index, std::wstring* name) const {
  wchar_t buf[256];
  DWORD bufsize = std::size(buf);
  LONG r = Reg::EnumValue(key_, index, buf, &bufsize, nullptr, nullptr, nullptr,
                          nullptr);
  if (r == ERROR_SUCCESS) {
    name->assign(buf, bufsize);
  }

  return r;
}

template <typename Reg>
LONG GenericRegKey<Reg>::DeleteKey(const wchar_t* name) {
  DCHECK(name);

  // Verify the key exists before attempting delete to replicate previous
  // behavior.
  // `RegOpenKeyEx()` will return an error if `key_` is invalid.
  HKEY subkey = nullptr;
  LONG result =
      Reg::OpenKey(key_, name, 0, READ_CONTROL | wow64access_, &subkey);
  if (result != ERROR_SUCCESS) {
    return result;
  }
  Reg::CloseKey(subkey);

  return RegDelRecurse(key_, name, wow64access_);
}

template <typename Reg>
LONG GenericRegKey<Reg>::DeleteEmptyKey(const wchar_t* name) {
  DCHECK(name);

  // `RegOpenKeyEx()` will return an error if `key_` is invalid.
  HKEY target_key = nullptr;
  LONG result =
      Reg::OpenKey(key_, name, 0, KEY_READ | wow64access_, &target_key);

  if (result != ERROR_SUCCESS) {
    return result;
  }

  DWORD count = 0;
  result =
      Reg::QueryInfoKey(target_key, nullptr, nullptr, nullptr, nullptr, nullptr,
                        nullptr, &count, nullptr, nullptr, nullptr, nullptr);

  Reg::CloseKey(target_key);

  if (result != ERROR_SUCCESS) {
    return result;
  }

  if (count == 0) {
    return RegDeleteKeyEx(key_, name, wow64access_, 0);
  }

  return ERROR_DIR_NOT_EMPTY;
}

template <typename Reg>
LONG GenericRegKey<Reg>::DeleteValue(const wchar_t* value_name) {
  // `RegDeleteValue()` will return an error if `key_` is invalid.
  LONG result = Reg::DeleteValue(key_, value_name);
  return result;
}

template <typename Reg>
LONG GenericRegKey<Reg>::ReadValueDW(const wchar_t* name,
                                     DWORD* out_value) const {
  DCHECK(out_value);
  DWORD type = REG_DWORD;
  DWORD size = sizeof(DWORD);
  DWORD local_value = 0;
  LONG result = ReadValue(name, &local_value, &size, &type);
  if (result == ERROR_SUCCESS) {
    if ((type == REG_DWORD || type == REG_BINARY) && size == sizeof(DWORD)) {
      *out_value = local_value;
    } else {
      result = ERROR_CANTREAD;
    }
  }

  return result;
}

template <typename Reg>
LONG GenericRegKey<Reg>::ReadInt64(const wchar_t* name,
                                   int64_t* out_value) const {
  DCHECK(out_value);
  DWORD type = REG_QWORD;
  int64_t local_value = 0;
  DWORD size = sizeof(local_value);
  LONG result = ReadValue(name, &local_value, &size, &type);
  if (result == ERROR_SUCCESS) {
    if ((type == REG_QWORD || type == REG_BINARY) &&
        size == sizeof(local_value)) {
      *out_value = local_value;
    } else {
      result = ERROR_CANTREAD;
    }
  }

  return result;
}

template <typename Reg>
LONG GenericRegKey<Reg>::ReadValue(const wchar_t* name,
                                   std::wstring* out_value) const {
  DCHECK(out_value);
  const size_t kMaxStringLength = 1024;  // This is after expansion.
  // Use the one of the other forms of ReadValue if 1024 is too small for you.
  wchar_t raw_value[kMaxStringLength];
  DWORD type = REG_SZ, size = sizeof(raw_value);
  LONG result = ReadValue(name, raw_value, &size, &type);
  if (result == ERROR_SUCCESS) {
    if (type == REG_SZ) {
      *out_value = raw_value;
    } else if (type == REG_EXPAND_SZ) {
      wchar_t expanded[kMaxStringLength];
      size = ExpandEnvironmentStrings(raw_value, expanded, kMaxStringLength);
      // Success: returns the number of wchar_t's copied
      // Fail: buffer too small, returns the size required
      // Fail: other, returns 0
      if (size == 0 || size > kMaxStringLength) {
        result = ERROR_MORE_DATA;
      } else {
        *out_value = expanded;
      }
    } else {
      // Not a string. Oops.
      result = ERROR_CANTREAD;
    }
  }

  return result;
}

template <typename Reg>
LONG GenericRegKey<Reg>::ReadValue(const wchar_t* name,
                                   void* data,
                                   DWORD* dsize,
                                   DWORD* dtype) const {
  LONG result = Reg::QueryValue(key_, name, nullptr, dtype,
                                reinterpret_cast<LPBYTE>(data), dsize);
  return result;
}

template <typename Reg>
LONG GenericRegKey<Reg>::ReadValues(const wchar_t* name,
                                    std::vector<std::wstring>* values) {
  values->clear();

  DWORD type = REG_MULTI_SZ;
  DWORD size = 0;
  LONG result = ReadValue(name, nullptr, &size, &type);
  if (result != ERROR_SUCCESS || size == 0) {
    return result;
  }

  if (type != REG_MULTI_SZ) {
    return ERROR_CANTREAD;
  }

  std::vector<wchar_t> buffer(size / sizeof(wchar_t));
  result = ReadValue(name, buffer.data(), &size, nullptr);
  if (result != ERROR_SUCCESS || size == 0) {
    return result;
  }

  // Parse the double-null-terminated list of strings.
  // Note: This code is paranoid to not read outside of |buf|, in the case
  // where it may not be properly terminated.
  auto entry = buffer.cbegin();
  auto buffer_end = buffer.cend();
  while (entry < buffer_end && *entry != '\0') {
    auto entry_end = std::find(entry, buffer_end, '\0');
    values->emplace_back(entry, entry_end);
    entry = entry_end + 1;
  }
  return 0;
}

template <typename Reg>
LONG GenericRegKey<Reg>::WriteValue(const wchar_t* name, DWORD in_value) {
  return WriteValue(name, &in_value, static_cast<DWORD>(sizeof(in_value)),
                    REG_DWORD);
}

template <typename Reg>
LONG GenericRegKey<Reg>::WriteValue(const wchar_t* name,
                                    const wchar_t* in_value) {
  return WriteValue(
      name, in_value,
      static_cast<DWORD>(sizeof(*in_value) *
                         (std::char_traits<wchar_t>::length(in_value) + 1)),
      REG_SZ);
}

template <typename Reg>
LONG GenericRegKey<Reg>::WriteValue(const wchar_t* name,
                                    const void* data,
                                    DWORD dsize,
                                    DWORD dtype) {
  DCHECK(data || !dsize);

  LONG result =
      Reg::SetValue(key_, name, 0, dtype,
                    reinterpret_cast<LPBYTE>(const_cast<void*>(data)), dsize);
  return result;
}

template <typename Reg>
bool GenericRegKey<Reg>::StartWatching(ChangeCallback callback) {
  if (!key_watcher_) {
    key_watcher_ = std::make_unique<Watcher>();
  }

  if (!key_watcher_->StartWatching(key_, std::move(callback))) {
    return false;
  }

  return true;
}

// static
template <typename Reg>
LONG GenericRegKey<Reg>::RegDelRecurse(HKEY root_key,
                                       const wchar_t* name,
                                       REGSAM access) {
  // First, see if the key can be deleted without having to recurse.
  LONG result = Reg::DeleteKey(root_key, name, access, 0);
  if (result == ERROR_SUCCESS) {
    return result;
  }

  HKEY target_key = nullptr;
  result = Reg::OpenKey(root_key, name, 0, KEY_ENUMERATE_SUB_KEYS | access,
                        &target_key);

  if (result == ERROR_FILE_NOT_FOUND) {
    return ERROR_SUCCESS;
  }
  if (result != ERROR_SUCCESS)
    return result;

  std::wstring subkey_name(name);

  // Check for an ending slash and add one if it is missing.
  if (!subkey_name.empty() && subkey_name.back() != '\\') {
    subkey_name.push_back('\\');
  }

  // Enumerate the keys
  result = ERROR_SUCCESS;
  const DWORD kMaxKeyNameLength = MAX_PATH;
  const size_t base_key_length = subkey_name.length();
  std::wstring key_name;
  while (result == ERROR_SUCCESS) {
    DWORD key_size = kMaxKeyNameLength;
    result =
        Reg::EnumKey(target_key, 0, WriteInto(&key_name, kMaxKeyNameLength),
                     &key_size, nullptr, nullptr, nullptr, nullptr);

    if (result != ERROR_SUCCESS) {
      break;
    }

    key_name.resize(key_size);
    subkey_name.resize(base_key_length);
    subkey_name += key_name;

    if (RegDelRecurse(root_key, subkey_name.c_str(), access) != ERROR_SUCCESS) {
      break;
    }
  }

  Reg::CloseKey(target_key);

  // Try again to delete the key.
  result = Reg::DeleteKey(root_key, name, access, 0);

  return result;
}

// Instantiate the only two allowed versions of GenericRegKey for use by the
// public base::win::RegKey and base::win::ExportDerivedRegKey.
template class GenericRegKey<internal::Standard>;
template class GenericRegKey<internal::ExportDerived>;

}  // namespace internal

RegKey::RegKey() : GenericRegKey<internal::Standard>() {}
RegKey::RegKey(HKEY key) : GenericRegKey<internal::Standard>(key) {}
RegKey::RegKey(HKEY rootkey, const wchar_t* subkey, REGSAM access)
    : GenericRegKey<internal::Standard>(rootkey, subkey, access) {}

RegKey::RegKey(RegKey&& other) noexcept
    : GenericRegKey<internal::Standard>(std::move(other)) {}
RegKey& RegKey::operator=(RegKey&& other) {
  GenericRegKey<internal::Standard>::operator=(std::move(other));
  return *this;
}

RegKey::~RegKey() = default;

ExportDerivedRegKey::ExportDerivedRegKey()
    : GenericRegKey<internal::ExportDerived>() {}
ExportDerivedRegKey::ExportDerivedRegKey(HKEY key)
    : GenericRegKey<internal::ExportDerived>(key) {}
ExportDerivedRegKey::ExportDerivedRegKey(HKEY rootkey,
                                         const wchar_t* subkey,
                                         REGSAM access)
    : GenericRegKey<internal::ExportDerived>(rootkey, subkey, access) {}

ExportDerivedRegKey::ExportDerivedRegKey(ExportDerivedRegKey&& other) noexcept
    : GenericRegKey<internal::ExportDerived>(std::move(other)) {}
ExportDerivedRegKey& ExportDerivedRegKey::operator=(
    ExportDerivedRegKey&& other) {
  GenericRegKey<internal::ExportDerived>::operator=(std::move(other));
  return *this;
}

ExportDerivedRegKey::~ExportDerivedRegKey() = default;

// RegistryValueIterator ------------------------------------------------------

RegistryValueIterator::RegistryValueIterator(HKEY root_key,
                                             const wchar_t* folder_key,
                                             REGSAM wow64access)
    : name_(MAX_PATH, '\0'), value_(MAX_PATH, '\0') {
  Initialize(root_key, folder_key, wow64access);
}

RegistryValueIterator::RegistryValueIterator(HKEY root_key,
                                             const wchar_t* folder_key)
    : name_(MAX_PATH, '\0'), value_(MAX_PATH, '\0') {
  Initialize(root_key, folder_key, 0);
}

void RegistryValueIterator::Initialize(HKEY root_key,
                                       const wchar_t* folder_key,
                                       REGSAM wow64access) {
  DCHECK_EQ(wow64access & ~kWow64AccessMask, static_cast<REGSAM>(0));
  LONG result =
      RegOpenKeyEx(root_key, folder_key, 0, KEY_READ | wow64access, &key_);
  if (result != ERROR_SUCCESS) {
    key_ = nullptr;
  } else {
    DWORD count = 0;
    result =
        ::RegQueryInfoKey(key_, nullptr, nullptr, nullptr, nullptr, nullptr,
                          nullptr, &count, nullptr, nullptr, nullptr, nullptr);

    if (result != ERROR_SUCCESS) {
      ::RegCloseKey(key_);
      key_ = nullptr;
    } else {
      index_ = count - 1;
    }
  }

  Read();
}

RegistryValueIterator::~RegistryValueIterator() {
  if (key_)
    ::RegCloseKey(key_);
}

DWORD RegistryValueIterator::ValueCount() const {
  DWORD count = 0;
  LONG result =
      ::RegQueryInfoKey(key_, nullptr, nullptr, nullptr, nullptr, nullptr,
                        nullptr, &count, nullptr, nullptr, nullptr, nullptr);
  if (result != ERROR_SUCCESS)
    return 0;

  return count;
}

bool RegistryValueIterator::Valid() const {
  return key_ != nullptr && index_ != kInvalidIterValue;
}

void RegistryValueIterator::operator++() {
  if (index_ != kInvalidIterValue)
    --index_;
  Read();
}

bool RegistryValueIterator::Read() {
  if (Valid()) {
    DWORD capacity = static_cast<DWORD>(name_.capacity());
    DWORD name_size = capacity;
    // |value_size_| is in bytes. Reserve the last character for a NUL.
    value_size_ = static_cast<DWORD>((value_.size() - 1) * sizeof(wchar_t));
    LONG result = ::RegEnumValue(
        key_, index_, WriteInto(&name_, name_size), &name_size, nullptr, &type_,
        reinterpret_cast<BYTE*>(value_.data()), &value_size_);

    if (result == ERROR_MORE_DATA) {
      // Registry key names are limited to 255 characters and fit within
      // MAX_PATH (which is 260) but registry value names can use up to 16,383
      // characters and the value itself is not limited
      // (from http://msdn.microsoft.com/en-us/library/windows/desktop/
      // ms724872(v=vs.85).aspx).
      // Resize the buffers and retry if their size caused the failure.
      DWORD value_size_in_wchars = to_wchar_size(value_size_);
      if (value_size_in_wchars + 1 > value_.size())
        value_.resize(value_size_in_wchars + 1, '\0');
      value_size_ = static_cast<DWORD>((value_.size() - 1) * sizeof(wchar_t));
      name_size = name_size == capacity ? MAX_REGISTRY_NAME_SIZE : capacity;
      result = ::RegEnumValue(
          key_, index_, WriteInto(&name_, name_size), &name_size, nullptr,
          &type_, reinterpret_cast<BYTE*>(value_.data()), &value_size_);
    }

    if (result == ERROR_SUCCESS) {
      DCHECK_LT(to_wchar_size(value_size_), value_.size());
      value_[to_wchar_size(value_size_)] = '\0';
      return true;
    }
  }

  name_[0] = '\0';
  value_[0] = '\0';
  value_size_ = 0;
  return false;
}

// RegistryKeyIterator --------------------------------------------------------

RegistryKeyIterator::RegistryKeyIterator(HKEY root_key,
                                         const wchar_t* folder_key) {
  Initialize(root_key, folder_key, 0);
}

RegistryKeyIterator::RegistryKeyIterator(HKEY root_key,
                                         const wchar_t* folder_key,
                                         REGSAM wow64access) {
  Initialize(root_key, folder_key, wow64access);
}

RegistryKeyIterator::~RegistryKeyIterator() {
  if (key_)
    ::RegCloseKey(key_);
}

DWORD RegistryKeyIterator::SubkeyCount() const {
  DWORD count = 0;
  LONG result =
      ::RegQueryInfoKey(key_, nullptr, nullptr, nullptr, &count, nullptr,
                        nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
  if (result != ERROR_SUCCESS)
    return 0;

  return count;
}

bool RegistryKeyIterator::Valid() const {
  return key_ != nullptr && index_ != kInvalidIterValue;
}

void RegistryKeyIterator::operator++() {
  if (index_ != kInvalidIterValue)
    --index_;
  Read();
}

bool RegistryKeyIterator::Read() {
  if (Valid()) {
    DWORD ncount = static_cast<DWORD>(std::size(name_));
    FILETIME written;
    LONG r = ::RegEnumKeyEx(key_, index_, name_, &ncount, nullptr, nullptr,
                            nullptr, &written);
    if (ERROR_SUCCESS == r)
      return true;
  }

  name_[0] = '\0';
  return false;
}

void RegistryKeyIterator::Initialize(HKEY root_key,
                                     const wchar_t* folder_key,
                                     REGSAM wow64access) {
  DCHECK_EQ(wow64access & ~kWow64AccessMask, static_cast<REGSAM>(0));
  LONG result =
      RegOpenKeyEx(root_key, folder_key, 0, KEY_READ | wow64access, &key_);
  if (result != ERROR_SUCCESS) {
    key_ = nullptr;
  } else {
    DWORD count = 0;
    result =
        ::RegQueryInfoKey(key_, nullptr, nullptr, nullptr, &count, nullptr,
                          nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);

    if (result != ERROR_SUCCESS) {
      ::RegCloseKey(key_);
      key_ = nullptr;
    } else {
      index_ = count - 1;
    }
  }

  Read();
}

}  // namespace base::win
