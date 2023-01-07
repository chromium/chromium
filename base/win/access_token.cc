// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/win/access_token.h"

#include <windows.h>

#include <utility>

#include "base/strings/stringprintf.h"

namespace base {
namespace win {

namespace {

Sid UnwrapSid(absl::optional<Sid>&& sid) {
  DCHECK(sid);
  return std::move(*sid);
}

absl::optional<std::vector<char>> GetTokenInfo(
    HANDLE token,
    TOKEN_INFORMATION_CLASS info_class) {
  // Get the buffer size. The call to GetTokenInformation should never succeed.
  DWORD size = 0;
  if (::GetTokenInformation(token, info_class, nullptr, 0, &size) || !size)
    return absl::nullopt;

  std::vector<char> temp_buffer(size);
  if (!::GetTokenInformation(token, info_class, temp_buffer.data(), size,
                             &size)) {
    return absl::nullopt;
  }

  return std::move(temp_buffer);
}

template <typename T>
absl::optional<T> GetTokenInfoFixed(HANDLE token,
                                    TOKEN_INFORMATION_CLASS info_class) {
  T result;
  DWORD size = sizeof(T);
  if (!::GetTokenInformation(token, info_class, &result, size, &size))
    return absl::nullopt;

  return result;
}

template <typename T>
T* GetType(absl::optional<std::vector<char>>& info) {
  DCHECK(info);
  DCHECK(info->size() >= sizeof(T));
  return reinterpret_cast<T*>(info->data());
}

std::vector<AccessToken::Group> GetGroupsFromToken(
    HANDLE token,
    TOKEN_INFORMATION_CLASS info_class) {
  absl::optional<std::vector<char>> groups = GetTokenInfo(token, info_class);
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
  absl::optional<TOKEN_STATISTICS> value =
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
  if (!::LookupPrivilegeName(nullptr, &luid, name, &size))
    return base::StringPrintf(L"%08X-%08X", luid.HighPart, luid.LowPart);
  return name;
}

bool AccessToken::Privilege::IsEnabled() const {
  return !!(attributes_ & SE_PRIVILEGE_ENABLED);
}

AccessToken::Privilege::Privilege(CHROME_LUID luid, DWORD attributes)
    : luid_(luid), attributes_(attributes) {}

AccessToken::Dacl::Dacl(const ACL* dacl)
    : acl_buffer_(reinterpret_cast<const char*>(dacl),
                  reinterpret_cast<const char*>(dacl) + dacl->AclSize) {}
AccessToken::Dacl::Dacl(Dacl&&) = default;
AccessToken::Dacl::~Dacl() {}

ACL* AccessToken::Dacl::GetAcl() const {
  return const_cast<ACL*>(reinterpret_cast<const ACL*>(acl_buffer_.data()));
}

absl::optional<AccessToken> AccessToken::FromToken(HANDLE token) {
  HANDLE new_token;
  if (!::DuplicateHandle(::GetCurrentProcess(), token, ::GetCurrentProcess(),
                         &new_token, TOKEN_QUERY, FALSE, 0)) {
    return absl::nullopt;
  }
  return AccessToken(new_token);
}

absl::optional<AccessToken> AccessToken::FromProcess(HANDLE process,
                                                     bool impersonation) {
  HANDLE token = nullptr;
  if (impersonation) {
    if (!::OpenProcessToken(process, TOKEN_DUPLICATE, &token))
      return absl::nullopt;
    ScopedHandle primary_token(token);
    if (!::DuplicateToken(primary_token.get(), SecurityIdentification, &token))
      return absl::nullopt;
  } else {
    if (!::OpenProcessToken(process, TOKEN_QUERY, &token))
      return absl::nullopt;
  }
  return AccessToken(token);
}

absl::optional<AccessToken> AccessToken::FromCurrentProcess(
    bool impersonation) {
  return FromProcess(::GetCurrentProcess(), impersonation);
}

absl::optional<AccessToken> AccessToken::FromThread(HANDLE thread,
                                                    bool open_as_self) {
  HANDLE token;
  if (!::OpenThreadToken(thread, TOKEN_QUERY, open_as_self, &token))
    return absl::nullopt;
  return AccessToken(token);
}

absl::optional<AccessToken> AccessToken::FromCurrentThread(bool open_as_self) {
  return FromThread(::GetCurrentThread(), open_as_self);
}

absl::optional<AccessToken> AccessToken::FromEffective() {
  absl::optional<AccessToken> token = FromCurrentThread();
  if (token)
    return token;
  if (::GetLastError() != ERROR_NO_TOKEN)
    return absl::nullopt;
  return FromCurrentProcess();
}

AccessToken::AccessToken(AccessToken&&) = default;
AccessToken& AccessToken::operator=(AccessToken&&) = default;
AccessToken::~AccessToken() = default;

Sid AccessToken::User() const {
  return UserGroup().GetSid().Clone();
}

AccessToken::Group AccessToken::UserGroup() const {
  absl::optional<std::vector<char>> buffer =
      GetTokenInfo(token_.get(), TokenUser);
  SID_AND_ATTRIBUTES& user = GetType<TOKEN_USER>(buffer)->User;
  return {UnwrapSid(Sid::FromPSID(user.Sid)), user.Attributes};
}

Sid AccessToken::Owner() const {
  absl::optional<std::vector<char>> buffer =
      GetTokenInfo(token_.get(), TokenOwner);
  return UnwrapSid(Sid::FromPSID(GetType<TOKEN_OWNER>(buffer)->Owner));
}

Sid AccessToken::PrimaryGroup() const {
  absl::optional<std::vector<char>> buffer =
      GetTokenInfo(token_.get(), TokenPrimaryGroup);
  return UnwrapSid(
      Sid::FromPSID(GetType<TOKEN_PRIMARY_GROUP>(buffer)->PrimaryGroup));
}

absl::optional<Sid> AccessToken::LogonId() const {
  std::vector<AccessToken::Group> groups =
      GetGroupsFromToken(token_.get(), TokenLogonSid);
  for (const AccessToken::Group& group : groups) {
    if (group.IsLogonId())
      return group.GetSid().Clone();
  }
  return absl::nullopt;
}

DWORD AccessToken::IntegrityLevel() const {
  absl::optional<std::vector<char>> buffer =
      GetTokenInfo(token_.get(), TokenIntegrityLevel);
  if (!buffer)
    return MAXDWORD;

  PSID il_sid = GetType<TOKEN_MANDATORY_LABEL>(buffer)->Label.Sid;
  return *::GetSidSubAuthority(
      il_sid, static_cast<DWORD>(*::GetSidSubAuthorityCount(il_sid) - 1));
}

DWORD AccessToken::SessionId() const {
  absl::optional<DWORD> value =
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
  absl::optional<DWORD> value =
      GetTokenInfoFixed<DWORD>(token_.get(), TokenIsAppContainer);
  if (!value)
    return false;
  return !!*value;
}

absl::optional<Sid> AccessToken::AppContainerSid() const {
  absl::optional<std::vector<char>> buffer =
      GetTokenInfo(token_.get(), TokenAppContainerSid);
  if (!buffer)
    return absl::nullopt;

  TOKEN_APPCONTAINER_INFORMATION* info =
      GetType<TOKEN_APPCONTAINER_INFORMATION>(buffer);
  if (!info->TokenAppContainer)
    return absl::nullopt;
  return Sid::FromPSID(info->TokenAppContainer);
}

std::vector<AccessToken::Group> AccessToken::Capabilities() const {
  return GetGroupsFromToken(token_.get(), TokenCapabilities);
}

absl::optional<AccessToken> AccessToken::LinkedToken() const {
  absl::optional<TOKEN_LINKED_TOKEN> value =
      GetTokenInfoFixed<TOKEN_LINKED_TOKEN>(token_.get(), TokenLinkedToken);
  if (!value)
    return absl::nullopt;
  return AccessToken(value->LinkedToken);
}

absl::optional<AccessToken::Dacl> AccessToken::DefaultDacl() const {
  absl::optional<std::vector<char>> dacl_buffer =
      GetTokenInfo(token_.get(), TokenDefaultDacl);
  if (!dacl_buffer)
    return absl::nullopt;
  TOKEN_DEFAULT_DACL* dacl_ptr = GetType<TOKEN_DEFAULT_DACL>(dacl_buffer);
  if (!dacl_ptr->DefaultDacl)
    return absl::nullopt;
  DCHECK(::IsValidAcl(dacl_ptr->DefaultDacl));
  DCHECK_GE(dacl_buffer->size(), dacl_ptr->DefaultDacl->AclSize);
  return AccessToken::Dacl{dacl_ptr->DefaultDacl};
}

CHROME_LUID AccessToken::Id() const {
  return ConvertLuid(GetTokenStatistics(token_.get()).TokenId);
}

CHROME_LUID AccessToken::AuthenticationId() const {
  return ConvertLuid(GetTokenStatistics(token_.get()).AuthenticationId);
}

std::vector<AccessToken::Privilege> AccessToken::Privileges() const {
  absl::optional<std::vector<char>> privileges =
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
  absl::optional<TOKEN_ELEVATION> value =
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
  absl::optional<Sid> sid = Sid::FromKnownSid(known_sid);
  if (!sid)
    return false;
  return IsMember(*sid);
}

bool AccessToken::IsImpersonation() const {
  return GetTokenStatistics(token_.get()).TokenType == TokenImpersonation;
}

bool AccessToken::IsIdentification() const {
  TOKEN_STATISTICS stats = GetTokenStatistics(token_.get());
  if (stats.TokenType != TokenImpersonation)
    return false;

  return stats.ImpersonationLevel < SecurityImpersonation;
}

AccessToken::AccessToken(HANDLE token) : token_(token) {}

}  // namespace win
}  // namespace base
