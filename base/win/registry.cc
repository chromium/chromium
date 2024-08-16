// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/win/registry.h"

#include <ntstatus.h>
#include <stddef.h>

#include <algorithm>
#include <iterator>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/check_op.h"
#include "base/functional/callback.h"
#include "base/notreached.h"
#include "base/strings/string_util.h"
#include "base/strings/string_util_win.h"
#include "base/threading/thread_restrictions.h"
#include "base/win/object_watcher.h"
#include "base/win/scoped_handle.h"
#include "base/win/shlwapi.h"

extern "C" NTSTATUS WINAPI NtDeleteKey(IN HANDLE KeyHandle);

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

// Watches for modifications to a key.
class RegKey::Watcher : public ObjectWatcher::Delegate {
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

bool RegKey::Watcher::StartWatching(HKEY key, ChangeCallback callback) {
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

// RegKey ----------------------------------------------------------------------

RegKey::RegKey() = default;

RegKey::RegKey(HKEY key) : key_(key) {}

RegKey::RegKey(HKEY rootkey, const wchar_t* subkey, REGSAM access) {
  if (rootkey) {
    if (access & (KEY_SET_VALUE | KEY_CREATE_SUB_KEY | KEY_CREATE_LINK)) {
      (void)Create(rootkey, subkey, access);
    } else {
      (void)Open(rootkey, subkey, access);
    }
  } else {
    DCHECK(!subkey);
    wow64access_ = access & kWow64AccessMask;
  }
}

RegKey::RegKey(RegKey&& other) noexcept
    : key_(other.key_),
      wow64access_(other.wow64access_),
      key_watcher_(std::move(other.key_watcher_)) {
  other.key_ = nullptr;
  other.wow64access_ = 0;
}

RegKey& RegKey::operator=(RegKey&& other) {
  Close();
  std::swap(key_, other.key_);
  std::swap(wow64access_, other.wow64access_);
  key_watcher_ = std::move(other.key_watcher_);
  return *this;
}

RegKey::~RegKey() {
  Close();
}

LONG RegKey::Create(HKEY rootkey, const wchar_t* subkey, REGSAM access) {
  DWORD disposition_value;
  return CreateWithDisposition(rootkey, subkey, &disposition_value, access);
}

LONG RegKey::CreateWithDisposition(HKEY rootkey,
                                   const wchar_t* subkey,
                                   DWORD* disposition,
                                   REGSAM access) {
  DCHECK(rootkey && subkey && access && disposition);
  HKEY subhkey = nullptr;
  LONG result =
      RegCreateKeyEx(rootkey, subkey, 0, nullptr, REG_OPTION_NON_VOLATILE,
                     access, nullptr, &subhkey, disposition);
  if (result == ERROR_SUCCESS) {
    Close();
    key_ = subhkey;
    wow64access_ = access & kWow64AccessMask;
  }

  return result;
}

LONG RegKey::CreateKey(const wchar_t* name, REGSAM access) {
  DCHECK(name && access);

  if (!Valid()) {
    // The parent key has not been opened or created.
    return ERROR_INVALID_HANDLE;
  }

  // After the application has accessed an alternate registry view using one
  // of the [KEY_WOW64_32KEY / KEY_WOW64_64KEY] flags, all subsequent
  // operations (create, delete, or open) on child registry keys must
  // explicitly use the same flag. Otherwise, there can be unexpected
  // behavior.
  // http://msdn.microsoft.com/en-us/library/windows/desktop/aa384129.aspx.
  if ((access & kWow64AccessMask) != wow64access_) {
    NOTREACHED();
  }
  HKEY subkey = nullptr;
  LONG result = RegCreateKeyEx(key_, name, 0, nullptr, REG_OPTION_NON_VOLATILE,
                               access, nullptr, &subkey, nullptr);
  if (result == ERROR_SUCCESS) {
    Close();
    key_ = subkey;
    wow64access_ = access & kWow64AccessMask;
  }

  return result;
}

LONG RegKey::Open(HKEY rootkey, const wchar_t* subkey, REGSAM access) {
  return Open(rootkey, subkey, /*options=*/0, access);
}

LONG RegKey::OpenKey(const wchar_t* relative_key_name, REGSAM access) {
  DCHECK(relative_key_name && access);

  if (!Valid()) {
    // The parent key has not been opened or created.
    return ERROR_INVALID_HANDLE;
  }

  // After the application has accessed an alternate registry view using one
  // of the [KEY_WOW64_32KEY / KEY_WOW64_64KEY] flags, all subsequent
  // operations (create, delete, or open) on child registry keys must
  // explicitly use the same flag. Otherwise, there can be unexpected
  // behavior.
  // http://msdn.microsoft.com/en-us/library/windows/desktop/aa384129.aspx.
  if ((access & kWow64AccessMask) != wow64access_) {
    NOTREACHED();
  }
  HKEY subkey = nullptr;
  LONG result = RegOpenKeyEx(key_, relative_key_name, 0, access, &subkey);

  // We have to close the current opened key before replacing it with the new
  // one.
  if (result == ERROR_SUCCESS) {
    Close();
    key_ = subkey;
    wow64access_ = access & kWow64AccessMask;
  }
  return result;
}

void RegKey::Close() {
  if (key_) {
    ::RegCloseKey(key_);
    key_ = nullptr;
    wow64access_ = 0;
  }
}

// TODO(wfh): Remove this and other unsafe methods. See http://crbug.com/375400
void RegKey::Set(HKEY key) {
  if (key_ != key) {
    Close();
    key_ = key;
  }
}

HKEY RegKey::Take() {
  DCHECK_EQ(wow64access_, 0u);
  HKEY key = key_;
  key_ = nullptr;
  return key;
}

bool RegKey::HasValue(const wchar_t* name) const {
  return RegQueryValueEx(key_, name, nullptr, nullptr, nullptr, nullptr) ==
         ERROR_SUCCESS;
}

base::expected<DWORD, LONG> RegKey::GetValueCount() const {
  DWORD count = 0;
  LONG result =
      RegQueryInfoKey(key_, nullptr, nullptr, nullptr, nullptr, nullptr,
                      nullptr, &count, nullptr, nullptr, nullptr, nullptr);
  if (result == ERROR_SUCCESS) {
    return base::ok(count);
  }
  return base::unexpected(result);
}

LONG RegKey::GetValueNameAt(DWORD index, std::wstring* name) const {
  wchar_t buf[256];
  DWORD bufsize = std::size(buf);
  LONG r = ::RegEnumValue(key_, index, buf, &bufsize, nullptr, nullptr, nullptr,
                          nullptr);
  if (r == ERROR_SUCCESS) {
    name->assign(buf, bufsize);
  }

  return r;
}

LONG RegKey::DeleteKey(const wchar_t* name, RecursiveDelete recursive) {
  DCHECK(name);

  if (!Valid()) {
    return ERROR_INVALID_HANDLE;
  }

  // Verify the key exists before attempting delete to replicate previous
  // behavior.
  RegKey target_key;
  LONG result = target_key.Open(key_, name, REG_OPTION_OPEN_LINK,
                                wow64access_ | KEY_QUERY_VALUE | DELETE);
  if (result != ERROR_SUCCESS) {
    return result;
  }

  if (recursive.value()) {
    target_key.Close();
    return RegDelRecurse(key_, name, wow64access_);
  }

  // Next, try to delete the key if it is a symbolic link.
  if (auto deleted_link = target_key.DeleteIfLink(); deleted_link.has_value()) {
    return deleted_link.value();
  }

  // It's not a symbolic link, so try to delete it without recursing.
  return ::RegDeleteKeyEx(target_key.key_, L"", wow64access_, 0);
}

LONG RegKey::DeleteValue(const wchar_t* value_name) {
  // `RegDeleteValue()` will return an error if `key_` is invalid.
  LONG result = RegDeleteValue(key_, value_name);
  return result;
}

LONG RegKey::ReadValueDW(const wchar_t* name, DWORD* out_value) const {
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

LONG RegKey::ReadInt64(const wchar_t* name, int64_t* out_value) const {
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

LONG RegKey::ReadValue(const wchar_t* name, std::wstring* out_value) const {
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

LONG RegKey::ReadValue(const wchar_t* name,
                       void* data,
                       DWORD* dsize,
                       DWORD* dtype) const {
  LONG result = RegQueryValueEx(key_, name, nullptr, dtype,
                                reinterpret_cast<LPBYTE>(data), dsize);
  return result;
}

LONG RegKey::ReadValues(const wchar_t* name,
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
  // Note: This code is paranoid to not read outside of |buf|, in the case where
  // it may not be properly terminated.
  auto entry = buffer.cbegin();
  auto buffer_end = buffer.cend();
  while (entry < buffer_end && *entry != '\0') {
    auto entry_end = std::find(entry, buffer_end, '\0');
    values->emplace_back(entry, entry_end);
    entry = entry_end + 1;
  }
  return 0;
}

LONG RegKey::WriteValue(const wchar_t* name, DWORD in_value) {
  return WriteValue(name, &in_value, static_cast<DWORD>(sizeof(in_value)),
                    REG_DWORD);
}

LONG RegKey::WriteValue(const wchar_t* name, const wchar_t* in_value) {
  return WriteValue(
      name, in_value,
      static_cast<DWORD>(sizeof(*in_value) *
                         (std::char_traits<wchar_t>::length(in_value) + 1)),
      REG_SZ);
}

LONG RegKey::WriteValue(const wchar_t* name,
                        const void* data,
                        DWORD dsize,
                        DWORD dtype) {
  DCHECK(data || !dsize);

  LONG result =
      RegSetValueEx(key_, name, 0, dtype,
                    reinterpret_cast<LPBYTE>(const_cast<void*>(data)), dsize);
  return result;
}

bool RegKey::StartWatching(ChangeCallback callback) {
  if (!key_watcher_) {
    key_watcher_ = std::make_unique<Watcher>();
  }

  if (!key_watcher_->StartWatching(key_, std::move(callback))) {
    return false;
  }

  return true;
}

LONG RegKey::Open(HKEY rootkey,
                  const wchar_t* subkey,
                  DWORD options,
                  REGSAM access) {
  DCHECK(options == 0 || options == REG_OPTION_OPEN_LINK) << options;
  DCHECK(rootkey && subkey && access);
  HKEY subhkey = nullptr;

  LONG result = RegOpenKeyEx(rootkey, subkey, options, access, &subhkey);
  if (result == ERROR_SUCCESS) {
    Close();
    key_ = subhkey;
    wow64access_ = access & kWow64AccessMask;
  }

  return result;
}

expected<bool, LONG> RegKey::IsLink() const {
  DWORD value_type = 0;
  LONG result = ::RegQueryValueEx(key_, L"SymbolicLinkValue",
                                  /*lpReserved=*/nullptr, &value_type,
                                  /*lpData=*/nullptr, /*lpcbData=*/nullptr);
  if (result == ERROR_FILE_NOT_FOUND) {
    return ok(false);
  }
  if (result == ERROR_SUCCESS) {
    return ok(value_type == REG_LINK);
  }
  return unexpected(result);
}

std::optional<LONG> RegKey::DeleteIfLink() {
  if (auto is_link = IsLink(); !is_link.has_value()) {
    return is_link.error();  // Failed to determine if a link.
  } else if (is_link.value() == false) {
    return std::nullopt;  // Not a link.
  }

  const NTSTATUS delete_result = ::NtDeleteKey(key_);
  if (delete_result == STATUS_SUCCESS) {
    return ERROR_SUCCESS;
  }
  using RtlNtStatusToDosErrorFunction = ULONG(WINAPI*)(NTSTATUS);
  static const RtlNtStatusToDosErrorFunction rtl_nt_status_to_dos_error =
      reinterpret_cast<RtlNtStatusToDosErrorFunction>(::GetProcAddress(
          ::GetModuleHandle(L"ntdll.dll"), "RtlNtStatusToDosError"));
  // The most common cause of failure is the presence of subkeys, which is
  // reported as `STATUS_CANNOT_DELETE` and maps to `ERROR_ACCESS_DENIED`.
  return rtl_nt_status_to_dos_error
             ? static_cast<LONG>(rtl_nt_status_to_dos_error(delete_result))
             : ERROR_ACCESS_DENIED;
}

// static
LONG RegKey::RegDelRecurse(HKEY root_key, const wchar_t* name, REGSAM access) {
  // First, open the key; taking care not to traverse symbolic links.
  RegKey target_key;
  LONG result = target_key.Open(
      root_key, name, REG_OPTION_OPEN_LINK,
      access | KEY_ENUMERATE_SUB_KEYS | KEY_QUERY_VALUE | DELETE);
  if (result == ERROR_FILE_NOT_FOUND) {  // The key doesn't exist.
    return ERROR_SUCCESS;
  }
  if (result != ERROR_SUCCESS) {
    return result;
  }

  // Next, try to delete the key if it is a symbolic link.
  if (auto deleted_link = target_key.DeleteIfLink(); deleted_link.has_value()) {
    return deleted_link.value();
  }

  // It's not a symbolic link, so try to delete it without recursing.
  result = ::RegDeleteKeyEx(target_key.key_, L"", access, 0);
  if (result == ERROR_SUCCESS) {
    return result;
  }

  // Enumerate the keys.
  const DWORD kMaxKeyNameLength = 256;  // Includes string terminator.
  auto subkey_buffer = std::make_unique<wchar_t[]>(kMaxKeyNameLength);
  while (true) {
    DWORD key_size = kMaxKeyNameLength;
    if (::RegEnumKeyEx(target_key.key_, 0, &subkey_buffer[0], &key_size,
                       nullptr, nullptr, nullptr, nullptr) != ERROR_SUCCESS) {
      break;
    }
    CHECK_LT(key_size, kMaxKeyNameLength);
    CHECK_EQ(subkey_buffer[key_size], L'\0');
    if (RegDelRecurse(target_key.key_, &subkey_buffer[0], access) !=
        ERROR_SUCCESS) {
      break;
    }
  }

  // Try again to delete the key.
  return ::RegDeleteKeyEx(target_key.key_, L"", access, 0);
}

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
