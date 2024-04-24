// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "base/win/access_token.h"

#include <windows.h>

#include <memory>
#include <utility>

#include "base/containers/span.h"
#include "base/numerics/checked_math.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"

namespace base::win {

namespace {

// The SECURITY_IMPERSONATION_LEVEL type is an enum and therefore can't be
// forward declared in windows_types.h. Ensure our separate definition matches
// the existing values for simplicity.
static_assert(static_cast<int>(SecurityImpersonationLevel::kAnonymous) ==
              SecurityAnonymous);
static_assert(static_cast<int>(SecurityImpersonationLevel::kIdentification) ==
              SecurityIdentification);
static_assert(static_cast<int>(SecurityImpersonationLevel::kImpersonation) ==
              SecurityImpersonation);
static_assert(static_cast<int>(SecurityImpersonationLevel::kDelegation) ==
              SecurityDelegation);

typedef BOOL(WINAPI* CreateAppContainerTokenFunction)(
    HANDLE TokenHandle,
    PSECURITY_CAPABILITIES SecurityCapabilities,
    PHANDLE OutToken);

Sid UnwrapSid(std::optional<Sid>&& sid) {
  DCHECK(sid);
  return std::move(*sid);
}

std::optional<std::vector<char>> GetTokenInfo(
    HANDLE token,
    TOKEN_INFORMATION_CLASS info_class) {
  // Get the buffer size. The call to GetTokenInformation should never succeed.
  DWORD size = 0;
  if (::GetTokenInformation(token, info_class, nullptr, 0, &size) || !size)
    return std::nullopt;

  std::vector<char> temp_buffer(size);
  if (!::GetTokenInformation(token, info_class, temp_buffer.data(), size,
                             &size)) {
    return std::nullopt;
  }

  return std::move(temp_buffer);
}

template <typename T>
std::optional<T> GetTokenInfoFixed(HANDLE token,
                                   TOKEN_INFORMATION_CLASS info_class) {
  T result;
  DWORD size = sizeof(T);
  if (!::GetTokenInformation(token, info_class, &result, size, &size))
    return std::nullopt;

  return result;
}

template <typename T>
T* GetType(std::optional<std::vector<char>>& info) {
  DCHECK(info);
  DCHECK(info->size() >= sizeof(T));
  return reinterpret_cast<T*>(info->data());
}

std::vector<AccessToken::Group> GetGroupsFromToken(
    HANDLE token,
    TOKEN_INFORMATION_CLASS info_class) {
  std::optional<std::vector<char>> groups = GetTokenInfo(token, info_class);
  // Sometimes only the GroupCount field is returned which indicates an empty
  // group set. If the buffer is smaller than the TOKEN_GROUPS structure then
  // just return an empty vector.
  if (!groups || (groups->size() < sizeof(TOKEN_GROUPS)))
    return {};

  TOKEN_GROUPS* groups_ptr = GetType<TOKEN_GROUPS>(groups);
  std::vector<AccessToken::Group> ret;
  ret.reserve(groups_ptr->GroupCount);
  for (DWORD index = 0; index < groups_ptr->GroupCount; ++index) {
    ret.emplace_back(UnwrapSid(Sid::FromPSID(groups_ptr->Groups[index].Sid)),
                     groups_ptr->Groups[index].Attributes);
  }
  return ret;
}

TOKEN_STATISTICS GetTokenStatistics(HANDLE token) {
  std::optional<TOKEN_STATISTICS> value =
      GetTokenInfoFixed<TOKEN_STATISTICS>(token, TokenStatistics);
  if (!value)
    return {};
  return *value;
}

CHROME_LUID ConvertLuid(const LUID& luid) {
  CHROME_LUID ret;
  ret.LowPart = luid.LowPart;
  ret.HighPart = luid.HighPart;
  return ret;
}

HANDLE DuplicateToken(HANDLE token,
                      ACCESS_MASK desired_access,
                      SECURITY_IMPERSONATION_LEVEL imp_level,
                      TOKEN_TYPE type) {
  HANDLE new_token;
  if (!::DuplicateTokenEx(token, TOKEN_QUERY | desired_access, nullptr,
                          imp_level, type, &new_token)) {
    return nullptr;
  }
  return new_token;
}

std::vector<SID_AND_ATTRIBUTES> ConvertSids(const std::vector<Sid>& sids,
                                            DWORD attributes) {
  std::vector<SID_AND_ATTRIBUTES> ret;
  ret.reserve(sids.size());
  for (const Sid& sid : sids) {
    SID_AND_ATTRIBUTES entry = {};
    entry.Sid = sid.GetPSID();
    entry.Attributes = attributes;
    ret.push_back(entry);
  }
  return ret;
}

std::optional<LUID> LookupPrivilege(const std::wstring& name) {
  LUID luid;
  if (!::LookupPrivilegeValue(nullptr, name.c_str(), &luid)) {
    return std::nullopt;
  }
  return luid;
}

std::vector<LUID_AND_ATTRIBUTES> ConvertPrivileges(
    const std::vector<std::wstring>& privs,
    DWORD attributes) {
  std::vector<LUID_AND_ATTRIBUTES> ret;
  ret.reserve(privs.size());
  for (const std::wstring& priv : privs) {
    std::optional<LUID> luid = LookupPrivilege(priv);
    if (!luid) {
      return {};
    }
    LUID_AND_ATTRIBUTES entry = {};
    entry.Luid = *luid;
    entry.Attributes = attributes;
    ret.push_back(entry);
  }
  return ret;
}

template <typename T>
T* GetPointer(std::vector<T>& values) {
  if (values.empty()) {
    return nullptr;
  }
  return values.data();
}

template <typename T>
bool Set(const ScopedHandle& token,
         TOKEN_INFORMATION_CLASS info_class,
         T& value) {
  return !!::SetTokenInformation(token.get(), info_class, &value,
                                 sizeof(value));
}

std::optional<DWORD> AdjustPrivilege(const ScopedHandle& token,
                                     const std::wstring& priv,
                                     DWORD attributes) {
  TOKEN_PRIVILEGES token_privs = {};
  token_privs.PrivilegeCount = 1;
  std::optional<LUID> luid = LookupPrivilege(priv);
  if (!luid) {
    return std::nullopt;
  }
  token_privs.Privileges[0].Luid = *luid;
  token_privs.Privileges[0].Attributes = attributes;

  TOKEN_PRIVILEGES out_privs = {};
  DWORD out_length = 0;
  if (!::AdjustTokenPrivileges(token.get(), FALSE, &token_privs,
                               sizeof(out_privs), &out_privs, &out_length)) {
    return std::nullopt;
  }
  if (::GetLastError() == ERROR_NOT_ALL_ASSIGNED) {
    return std::nullopt;
  }
  if (out_privs.PrivilegeCount == 1) {
    return out_privs.Privileges[0].Attributes;
  }
  return attributes;
}
}  // namespace

bool AccessToken::Group::IsIntegrity() const {
  return !!(attributes_ & SE_GROUP_INTEGRITY);
}

bool AccessToken::Group::IsEnabled() const {
  return !!(attributes_ & SE_GROUP_ENABLED);
}

bool AccessToken::Group::IsDenyOnly() const {
  return !!(attributes_ & SE_GROUP_USE_FOR_DENY_ONLY);
}

bool AccessToken::Group::IsLogonId() const {
  return (attributes_ & SE_GROUP_LOGON_ID) == SE_GROUP_LOGON_ID;
}

AccessToken::Group::Group(Sid&& sid, DWORD attributes)
    : sid_(std::move(sid)), attributes_(attributes) {}
AccessToken::Group::Group(Group&&) = default;
AccessToken::Group::Group::~Group() = default;

std::wstring AccessToken::Privilege::GetName() const {
  WCHAR name[128];
  LUID luid;
  luid.LowPart = luid_.LowPart;
  luid.HighPart = luid_.HighPart;
  DWORD size = std::size(name);
  return ::LookupPrivilegeName(nullptr, &luid, name, &size)
             ? name
             : ASCIIToWide(
                   StringPrintf("%08lX-%08lX", luid.HighPart, luid.LowPart));
}

bool AccessToken::Privilege::IsEnabled() const {
  return !!(attributes_ & SE_PRIVILEGE_ENABLED);
}

AccessToken::Privilege::Privilege(CHROME_LUID luid, DWORD attributes)
    : luid_(luid), attributes_(attributes) {}

std::optional<AccessToken> AccessToken::FromToken(HANDLE token,
                                                  ACCESS_MASK desired_access) {
  HANDLE new_token;
  if (!::DuplicateHandle(::GetCurrentProcess(), token, ::GetCurrentProcess(),
                         &new_token, TOKEN_QUERY | desired_access, FALSE, 0)) {
    return std::nullopt;
  }
  return AccessToken(new_token);
}

std::optional<AccessToken> AccessToken::FromToken(ScopedHandle&& token) {
  if (!token.is_valid()) {
    ::SetLastError(ERROR_INVALID_HANDLE);
    return std::nullopt;
  }
  if (!GetTokenInfoFixed<TOKEN_STATISTICS>(token.get(), TokenStatistics)) {
    return std::nullopt;
  }
  return AccessToken(token.release());
}

std::optional<AccessToken> AccessToken::FromProcess(
    HANDLE process,
    bool impersonation,
    ACCESS_MASK desired_access) {
  HANDLE token = nullptr;
  if (impersonation) {
    if (!::OpenProcessToken(process, TOKEN_DUPLICATE, &token))
      return std::nullopt;
    ScopedHandle primary_token(token);
    token = DuplicateToken(primary_token.get(), desired_access,
                           SecurityIdentification, TokenImpersonation);
    if (!token) {
      return std::nullopt;
    }
  } else {
    if (!::OpenProcessToken(process, TOKEN_QUERY | desired_access, &token))
      return std::nullopt;
  }
  return AccessToken(token);
}

std::optional<AccessToken> AccessToken::FromCurrentProcess(
    bool impersonation,
    ACCESS_MASK desired_access) {
  return FromProcess(::GetCurrentProcess(), impersonation, desired_access);
}

std::optional<AccessToken> AccessToken::FromThread(HANDLE thread,
                                                   bool open_as_self,
                                                   ACCESS_MASK desired_access) {
  HANDLE token;
  if (!::OpenThreadToken(thread, TOKEN_QUERY | desired_access, open_as_self,
                         &token))
    return std::nullopt;
  return AccessToken(token);
}

std::optional<AccessToken> AccessToken::FromCurrentThread(
    bool open_as_self,
    ACCESS_MASK desired_access) {
  return FromThread(::GetCurrentThread(), open_as_self, desired_access);
}

std::optional<AccessToken> AccessToken::FromEffective(
    ACCESS_MASK desired_access) {
  std::optional<AccessToken> token = FromCurrentThread(true, desired_access);
  if (token)
    return token;
  if (::GetLastError() != ERROR_NO_TOKEN)
    return std::nullopt;
  return FromCurrentProcess(false, desired_access);
}

AccessToken::AccessToken(AccessToken&&) = default;
AccessToken& AccessToken::operator=(AccessToken&&) = default;
AccessToken::~AccessToken() = default;

Sid AccessToken::User() const {
  return UserGroup().GetSid().Clone();
}

AccessToken::Group AccessToken::UserGroup() const {
  std::optional<std::vector<char>> buffer =
      GetTokenInfo(token_.get(), TokenUser);
  SID_AND_ATTRIBUTES& user = GetType<TOKEN_USER>(buffer)->User;
  return {UnwrapSid(Sid::FromPSID(user.Sid)), user.Attributes};
}

Sid AccessToken::Owner() const {
  std::optional<std::vector<char>> buffer =
      GetTokenInfo(token_.get(), TokenOwner);
  return UnwrapSid(Sid::FromPSID(GetType<TOKEN_OWNER>(buffer)->Owner));
}

Sid AccessToken::PrimaryGroup() const {
  std::optional<std::vector<char>> buffer =
      GetTokenInfo(token_.get(), TokenPrimaryGroup);
  return UnwrapSid(
      Sid::FromPSID(GetType<TOKEN_PRIMARY_GROUP>(buffer)->PrimaryGroup));
}

std::optional<Sid> AccessToken::LogonId() const {
  std::vector<AccessToken::Group> groups =
      GetGroupsFromToken(token_.get(), TokenLogonSid);
  for (const AccessToken::Group& group : groups) {
    if (group.IsLogonId())
      return group.GetSid().Clone();
  }
  return std::nullopt;
}

DWORD AccessToken::IntegrityLevel() const {
  std::optional<std::vector<char>> buffer =
      GetTokenInfo(token_.get(), TokenIntegrityLevel);
  if (!buffer)
    return MAXDWORD;

  PSID il_sid = GetType<TOKEN_MANDATORY_LABEL>(buffer)->Label.Sid;
  return *::GetSidSubAuthority(
      il_sid, static_cast<DWORD>(*::GetSidSubAuthorityCount(il_sid) - 1));
}

bool AccessToken::SetIntegrityLevel(DWORD integrity_level) {
  std::optional<base::win::Sid> sid = Sid::FromIntegrityLevel(integrity_level);
  if (!sid) {
    ::SetLastError(ERROR_INVALID_SID);
    return false;
  }

  TOKEN_MANDATORY_LABEL label = {};
  label.Label.Attributes = SE_GROUP_INTEGRITY;
  label.Label.Sid = sid->GetPSID();
  return Set(token_, TokenIntegrityLevel, label);
}

DWORD AccessToken::SessionId() const {
  std::optional<DWORD> value =
      GetTokenInfoFixed<DWORD>(token_.get(), TokenSessionId);
  if (!value)
    return MAXDWORD;
  return *value;
}

std::vector<AccessToken::Group> AccessToken::Groups() const {
  return GetGroupsFromToken(token_.get(), TokenGroups);
}

bool AccessToken::IsRestricted() const {
  return !!::IsTokenRestricted(token_.get());
}

std::vector<AccessToken::Group> AccessToken::RestrictedSids() const {
  return GetGroupsFromToken(token_.get(), TokenRestrictedSids);
}

bool AccessToken::IsAppContainer() const {
  std::optional<DWORD> value =
      GetTokenInfoFixed<DWORD>(token_.get(), TokenIsAppContainer);
  if (!value)
    return false;
  return !!*value;
}

std::optional<Sid> AccessToken::AppContainerSid() const {
  std::optional<std::vector<char>> buffer =
      GetTokenInfo(token_.get(), TokenAppContainerSid);
  if (!buffer)
    return std::nullopt;

  TOKEN_APPCONTAINER_INFORMATION* info =
      GetType<TOKEN_APPCONTAINER_INFORMATION>(buffer);
  if (!info->TokenAppContainer)
    return std::nullopt;
  return Sid::FromPSID(info->TokenAppContainer);
}

std::vector<AccessToken::Group> AccessToken::Capabilities() const {
  return GetGroupsFromToken(token_.get(), TokenCapabilities);
}

std::optional<AccessToken> AccessToken::LinkedToken() const {
  std::optional<TOKEN_LINKED_TOKEN> value =
      GetTokenInfoFixed<TOKEN_LINKED_TOKEN>(token_.get(), TokenLinkedToken);
  if (!value)
    return std::nullopt;
  return AccessToken(value->LinkedToken);
}

std::optional<AccessControlList> AccessToken::DefaultDacl() const {
  std::optional<std::vector<char>> dacl_buffer =
      GetTokenInfo(token_.get(), TokenDefaultDacl);
  if (!dacl_buffer)
    return std::nullopt;
  TOKEN_DEFAULT_DACL* dacl_ptr = GetType<TOKEN_DEFAULT_DACL>(dacl_buffer);
  return AccessControlList::FromPACL(dacl_ptr->DefaultDacl);
}

bool AccessToken::SetDefaultDacl(const AccessControlList& default_dacl) {
  TOKEN_DEFAULT_DACL set_default_dacl = {};
  set_default_dacl.DefaultDacl = default_dacl.get();
  return Set(token_, TokenDefaultDacl, set_default_dacl);
}

CHROME_LUID AccessToken::Id() const {
  return ConvertLuid(GetTokenStatistics(token_.get()).TokenId);
}

CHROME_LUID AccessToken::AuthenticationId() const {
  return ConvertLuid(GetTokenStatistics(token_.get()).AuthenticationId);
}

std::vector<AccessToken::Privilege> AccessToken::Privileges() const {
  std::optional<std::vector<char>> privileges =
      GetTokenInfo(token_.get(), TokenPrivileges);
  if (!privileges)
    return {};
  TOKEN_PRIVILEGES* privileges_ptr = GetType<TOKEN_PRIVILEGES>(privileges);
  std::vector<AccessToken::Privilege> ret;
  ret.reserve(privileges_ptr->PrivilegeCount);
  for (DWORD index = 0; index < privileges_ptr->PrivilegeCount; ++index) {
    ret.emplace_back(ConvertLuid(privileges_ptr->Privileges[index].Luid),
                     privileges_ptr->Privileges[index].Attributes);
  }
  return ret;
}

bool AccessToken::IsElevated() const {
  std::optional<TOKEN_ELEVATION> value =
      GetTokenInfoFixed<TOKEN_ELEVATION>(token_.get(), TokenElevation);
  if (!value)
    return false;
  return !!value->TokenIsElevated;
}

bool AccessToken::IsMember(const Sid& sid) const {
  BOOL is_member = FALSE;
  return ::CheckTokenMembership(token_.get(), sid.GetPSID(), &is_member) &&
         !!is_member;
}

bool AccessToken::IsMember(WellKnownSid known_sid) const {
  return IsMember(Sid(known_sid));
}

bool AccessToken::IsImpersonation() const {
  return GetTokenStatistics(token_.get()).TokenType == TokenImpersonation;
}

bool AccessToken::IsIdentification() const {
  return ImpersonationLevel() < SecurityImpersonationLevel::kImpersonation;
}

SecurityImpersonationLevel AccessToken::ImpersonationLevel() const {
  TOKEN_STATISTICS stats = GetTokenStatistics(token_.get());
  if (stats.TokenType != TokenImpersonation) {
    return SecurityImpersonationLevel::kImpersonation;
  }

  return static_cast<SecurityImpersonationLevel>(
      GetTokenStatistics(token_.get()).ImpersonationLevel);
}

std::optional<AccessToken> AccessToken::DuplicatePrimary(
    ACCESS_MASK desired_access) const {
  HANDLE token = DuplicateToken(token_.get(), desired_access, SecurityAnonymous,
                                TokenPrimary);
  if (!token) {
    return std::nullopt;
  }
  return AccessToken{token};
}

std::optional<AccessToken> AccessToken::DuplicateImpersonation(
    SecurityImpersonationLevel impersonation_level,
    ACCESS_MASK desired_access) const {
  HANDLE token = DuplicateToken(
      token_.get(), desired_access,
      static_cast<SECURITY_IMPERSONATION_LEVEL>(impersonation_level),
      TokenImpersonation);
  if (!token) {
    return std::nullopt;
  }
  return AccessToken(token);
}

std::optional<AccessToken> AccessToken::CreateRestricted(
    DWORD flags,
    const std::vector<Sid>& sids_to_disable,
    const std::vector<std::wstring>& privileges_to_delete,
    const std::vector<Sid>& sids_to_restrict,
    ACCESS_MASK desired_access) const {
  std::vector<SID_AND_ATTRIBUTES> sids_to_disable_buf =
      ConvertSids(sids_to_disable, 0);
  std::vector<SID_AND_ATTRIBUTES> sids_to_restrict_buf =
      ConvertSids(sids_to_restrict, 0);
  std::vector<LUID_AND_ATTRIBUTES> privileges_to_delete_buf =
      ConvertPrivileges(privileges_to_delete, 0);
  if (privileges_to_delete_buf.size() != privileges_to_delete.size()) {
    return std::nullopt;
  }

  HANDLE token;
  if (!::CreateRestrictedToken(
          token_.get(), flags, checked_cast<DWORD>(sids_to_disable_buf.size()),
          GetPointer(sids_to_disable_buf),
          checked_cast<DWORD>(privileges_to_delete_buf.size()),
          GetPointer(privileges_to_delete_buf),
          checked_cast<DWORD>(sids_to_restrict_buf.size()),
          GetPointer(sids_to_restrict_buf), &token)) {
    return std::nullopt;
  }

  ScopedHandle token_handle(token);
  return FromToken(token_handle.get(), desired_access);
}

std::optional<AccessToken> AccessToken::CreateAppContainer(
    const Sid& appcontainer_sid,
    const std::vector<Sid>& capabilities,
    ACCESS_MASK desired_access) const {
  static const CreateAppContainerTokenFunction CreateAppContainerToken =
      reinterpret_cast<CreateAppContainerTokenFunction>(::GetProcAddress(
          ::GetModuleHandle(L"kernelbase.dll"), "CreateAppContainerToken"));
  if (!CreateAppContainerToken) {
    ::SetLastError(ERROR_PROC_NOT_FOUND);
    return std::nullopt;
  }

  std::vector<SID_AND_ATTRIBUTES> capabilities_buf =
      ConvertSids(capabilities, SE_GROUP_ENABLED);
  SECURITY_CAPABILITIES security_capabilities = {};
  security_capabilities.AppContainerSid = appcontainer_sid.GetPSID();
  security_capabilities.Capabilities = GetPointer(capabilities_buf);
  security_capabilities.CapabilityCount =
      checked_cast<DWORD>(capabilities_buf.size());

  HANDLE token = nullptr;
  if (!CreateAppContainerToken(token_.get(), &security_capabilities, &token)) {
    return std::nullopt;
  }

  ScopedHandle token_handle(token);
  return FromToken(token_handle.get(), desired_access);
}

std::optional<bool> AccessToken::SetPrivilege(const std::wstring& name,
                                              bool enable) {
  std::optional<DWORD> attrs =
      AdjustPrivilege(token_, name.c_str(), enable ? SE_PRIVILEGE_ENABLED : 0);
  if (!attrs) {
    return std::nullopt;
  }
  return !!(*attrs & SE_PRIVILEGE_ENABLED);
}

bool AccessToken::RemovePrivilege(const std::wstring& name) {
  return AdjustPrivilege(token_, name.c_str(), SE_PRIVILEGE_REMOVED)
      .has_value();
}

bool AccessToken::RemoveAllPrivileges() {
  std::optional<std::vector<char>> privileges_buffer =
      GetTokenInfo(token_.get(), TokenPrivileges);
  if (!privileges_buffer ||
      (privileges_buffer->size() < sizeof(TOKEN_PRIVILEGES))) {
    return false;
  }
  auto* const token_privileges = GetType<TOKEN_PRIVILEGES>(privileges_buffer);
  if (privileges_buffer->size() <
      (offsetof(TOKEN_PRIVILEGES, Privileges) +
       sizeof(LUID_AND_ATTRIBUTES) * token_privileges->PrivilegeCount)) {
    return false;
  }

  for (auto privileges = base::make_span(&token_privileges->Privileges[0],
                                         token_privileges->PrivilegeCount);
       auto& privilege : privileges) {
    privilege.Attributes = SE_PRIVILEGE_REMOVED;
  }
  return ::AdjustTokenPrivileges(
      token_.get(), /*DisableAllPrivileges=*/FALSE, token_privileges,
      static_cast<DWORD>(privileges_buffer->size()),
      /*PreviousState=*/nullptr, /*ReturnLength=*/nullptr);
}

bool AccessToken::is_valid() const {
  return token_.is_valid();
}

HANDLE AccessToken::get() const {
  return token_.get();
}

ScopedHandle AccessToken::release() {
  return ScopedHandle(token_.release());
}

AccessToken::AccessToken(HANDLE token) : token_(token) {}

}  // namespace base::win
