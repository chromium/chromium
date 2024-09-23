// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/win/access_token.h"

#include <windows.h>

#include <algorithm>
#include <cstdint>
#include <map>
#include <optional>
#include <utility>

#include "base/win/atl.h"
#include "base/win/scoped_handle.h"
#include "base/win/security_util.h"
#include "base/win/windows_version.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base::win {

namespace {

// All token access minus TOKEN_QUERY
constexpr ACCESS_MASK kTokenAllNoQuery = (TOKEN_ALL_ACCESS & ~TOKEN_QUERY);

bool CompareLuid(const CHROME_LUID& left, const LUID& right) {
  return left.HighPart == right.HighPart && left.LowPart == right.LowPart;
}

int64_t ConvertLuid(const AccessToken::Privilege& priv) {
  CHROME_LUID luid = priv.GetLuid();
  return (static_cast<int64_t>(luid.HighPart) << 32) | luid.LowPart;
}

int64_t ConvertLuid(const LUID& luid) {
  return (static_cast<int64_t>(luid.HighPart) << 32) | luid.LowPart;
}

bool EqualSid(const Sid& left, const ATL::CSid& right) {
  return left.Equal(const_cast<SID*>(right.GetPSID()));
}

void CompareGroups(const std::vector<AccessToken::Group>& groups,
                   const ATL::CSid::CSidArray& sids,
                   const ATL::CAtlArray<DWORD>& attrs) {
  ASSERT_EQ(groups.size(), sids.GetCount());
  ASSERT_EQ(sids.GetCount(), attrs.GetCount());
  std::map<std::wstring, DWORD> group_map;
  for (const AccessToken::Group& group : groups) {
    std::optional<std::wstring> sddl = group.GetSid().ToSddlString();
    ASSERT_TRUE(sddl);
    group_map.insert({*sddl, group.GetAttributes()});
  }
  for (size_t index = 0; index < sids.GetCount(); ++index) {
    auto found_group = group_map.find(sids[index].Sid());
    ASSERT_NE(found_group, group_map.end());
    EXPECT_EQ(found_group->second, attrs[index]);
  }
}

void CompareGroups(const std::vector<AccessToken::Group>& groups,
                   const CTokenGroups& atl_groups) {
  ATL::CSid::CSidArray sids;
  ATL::CAtlArray<DWORD> attrs;
  atl_groups.GetSidsAndAttributes(&sids, &attrs);
  CompareGroups(groups, sids, attrs);
}

void ComparePrivileges(const std::vector<AccessToken::Privilege>& privs,
                       const ATL::CTokenPrivileges& atl_privs) {
  CLUIDArray luids;
  ATL::CTokenPrivileges::CAttributes attrs;
  atl_privs.GetLuidsAndAttributes(&luids, &attrs);
  ATL::CTokenPrivileges::CNames names;
  atl_privs.GetNamesAndAttributes(&names);
  ASSERT_EQ(privs.size(), luids.GetCount());
  ASSERT_EQ(privs.size(), attrs.GetCount());
  ASSERT_EQ(privs.size(), names.GetCount());

  std::map<int64_t, const AccessToken::Privilege&> priv_map;
  for (const AccessToken::Privilege& priv : privs) {
    priv_map.insert({ConvertLuid(priv), priv});
  }

  for (size_t index = 0; index < luids.GetCount(); ++index) {
    auto found_priv = priv_map.find(ConvertLuid(luids[index]));
    ASSERT_NE(found_priv, priv_map.end());
    EXPECT_TRUE(CompareLuid(found_priv->second.GetLuid(), luids[index]));
    EXPECT_EQ(found_priv->second.GetAttributes(), attrs[index]);
    EXPECT_EQ(found_priv->second.GetName().c_str(), names[index]);
  }
}

void CompareIntegrityLevel(const AccessToken& token,
                           const ATL::CAccessToken& atl_token) {
  char buffer[sizeof(TOKEN_MANDATORY_LABEL) + SECURITY_MAX_SID_SIZE];
  DWORD size = sizeof(buffer);
  ASSERT_TRUE(::GetTokenInformation(atl_token.GetHandle(), TokenIntegrityLevel,
                                    buffer, size, &size));
  TOKEN_MANDATORY_LABEL* label =
      reinterpret_cast<TOKEN_MANDATORY_LABEL*>(buffer);
  ASSERT_TRUE(label->Label.Sid);
  std::optional<Sid> il_sid = Sid::FromIntegrityLevel(token.IntegrityLevel());
  ASSERT_TRUE(il_sid);
  EXPECT_TRUE(il_sid->Equal(label->Label.Sid));
}

void CompareElevated(const AccessToken& token,
                     const ATL::CAccessToken& atl_token) {
  TOKEN_ELEVATION elevation;
  DWORD size = sizeof(elevation);
  ASSERT_TRUE(::GetTokenInformation(atl_token.GetHandle(), TokenElevation,
                                    &elevation, size, &size));
  EXPECT_EQ(token.IsElevated(), !!elevation.TokenIsElevated);
}

bool GetLinkedToken(const ATL::CAccessToken& token,
                    ATL::CAccessToken* linked_token) {
  TOKEN_LINKED_TOKEN value;
  DWORD size = sizeof(value);
  if (!::GetTokenInformation(token.GetHandle(), TokenLinkedToken, &value, size,
                             &size)) {
    return false;
  }
  linked_token->Attach(value.LinkedToken);
  return true;
}

void CompareDefaultDacl(const AccessToken& token,
                        const ATL::CAccessToken& atl_token) {
  CDacl atl_dacl;
  ASSERT_TRUE(atl_token.GetDefaultDacl(&atl_dacl));
  ATL::CSid::CSidArray sids;
  CAcl::CAccessMaskArray access;
  CAcl::CAceTypeArray types;
  CAcl::CAceFlagArray flags;
  atl_dacl.GetAclEntries(&sids, &access, &types, &flags);
  std::optional<AccessControlList> dacl = token.DefaultDacl();
  ASSERT_TRUE(dacl);
  ACL* acl_ptr = dacl->get();
  ASSERT_TRUE(acl_ptr);
  DWORD ace_count = acl_ptr->AceCount;
  ASSERT_EQ(ace_count, sids.GetCount());
  ASSERT_EQ(ace_count, access.GetCount());
  ASSERT_EQ(ace_count, types.GetCount());
  ASSERT_EQ(ace_count, flags.GetCount());
  for (DWORD index = 0; index < ace_count; ++index) {
    ACE_HEADER* ace_header;
    ASSERT_TRUE(GetAce(acl_ptr, index, reinterpret_cast<LPVOID*>(&ace_header)));
    ASSERT_EQ(ace_header->AceType, types[index]);
    ASSERT_EQ(ace_header->AceFlags, flags[index]);
    // We only do a full comparison for these types of ACE.
    if (ace_header->AceType == ACCESS_ALLOWED_ACE_TYPE ||
        ace_header->AceType == ACCESS_DENIED_ACE_TYPE) {
      // ACCESS_ALLOWED_ACE and ACCESS_DENIED_ACE have the same layout.
      ACCESS_ALLOWED_ACE* ace =
          reinterpret_cast<ACCESS_ALLOWED_ACE*>(ace_header);
      EXPECT_EQ(ace->Mask, access[index]);
      std::optional<Sid> sid = Sid::FromPSID(&ace->SidStart);
      ASSERT_TRUE(sid);
      EXPECT_TRUE(EqualSid(*sid, sids[index]));
    }
  }
}

void CompareTokens(const AccessToken& token,
                   const ATL::CAccessToken& atl_token,
                   bool compare_linked_token = true) {
  LUID luid;
  // Only compare IDs if we're not comparing a token's linked token as the ID
  // will be different.
  if (compare_linked_token) {
    ASSERT_TRUE(atl_token.GetTokenId(&luid));
    EXPECT_TRUE(CompareLuid(token.Id(), luid));
  }
  ASSERT_TRUE(atl_token.GetLogonSessionId(&luid));
  EXPECT_TRUE(CompareLuid(token.AuthenticationId(), luid));
  ATL::CSid user_sid;
  ASSERT_TRUE(atl_token.GetUser(&user_sid));
  EXPECT_TRUE(EqualSid(token.User(), user_sid));
  AccessToken::Group user_group = token.UserGroup();
  EXPECT_TRUE(EqualSid(user_group.GetSid(), user_sid));
  EXPECT_EQ(0U, user_group.GetAttributes());
  ATL::CSid owner_sid;
  ASSERT_TRUE(atl_token.GetOwner(&owner_sid));
  EXPECT_TRUE(EqualSid(token.Owner(), owner_sid));
  ATL::CSid primary_group;
  ASSERT_TRUE(atl_token.GetPrimaryGroup(&primary_group));
  EXPECT_TRUE(EqualSid(token.PrimaryGroup(), primary_group));
  std::optional<Sid> logon_sid = token.LogonId();
  if (!logon_sid) {
    EXPECT_EQ(DWORD{ERROR_NOT_FOUND}, ::GetLastError());
  }
  ATL::CSid atl_logon_sid;
  if (!atl_token.GetLogonSid(&atl_logon_sid)) {
    EXPECT_FALSE(logon_sid);
  } else {
    ASSERT_TRUE(logon_sid);
    EXPECT_TRUE(EqualSid(*logon_sid, atl_logon_sid));
  }
  DWORD session_id;
  ASSERT_TRUE(atl_token.GetTerminalServicesSessionId(&session_id));
  EXPECT_EQ(token.SessionId(), session_id);
  CompareIntegrityLevel(token, atl_token);
  CompareElevated(token, atl_token);
  EXPECT_EQ(token.IsRestricted(), atl_token.IsTokenRestricted());
  TOKEN_TYPE token_type;
  ASSERT_TRUE(atl_token.GetType(&token_type));
  EXPECT_EQ(token.IsImpersonation(), token_type == TokenImpersonation);
  if (token_type == TokenImpersonation) {
    SECURITY_IMPERSONATION_LEVEL imp_level;
    ASSERT_TRUE(atl_token.GetImpersonationLevel(&imp_level));
    EXPECT_EQ(static_cast<int>(token.ImpersonationLevel()), imp_level);
  }
  CTokenGroups atl_groups;
  ASSERT_TRUE(atl_token.GetGroups(&atl_groups));
  CompareGroups(token.Groups(), atl_groups);
  ATL::CTokenPrivileges atl_privs;
  ASSERT_TRUE(atl_token.GetPrivileges(&atl_privs));
  ComparePrivileges(token.Privileges(), atl_privs);
  CompareDefaultDacl(token, atl_token);
  std::optional<AccessToken> linked_token = token.LinkedToken();
  ATL::CAccessToken atl_linked_token;
  bool result = GetLinkedToken(atl_token, &atl_linked_token);
  if (!linked_token) {
    EXPECT_FALSE(result);
  } else {
    ASSERT_TRUE(result);
    if (compare_linked_token)
      CompareTokens(*linked_token, atl_linked_token, false);
  }
}

bool DuplicateTokenWithSecurityDescriptor(const ATL::CAccessToken& token,
                                          DWORD desired_access,
                                          LPCWSTR security_descriptor,
                                          ATL::CAccessToken* new_token) {
  ATL::CSecurityDesc sd;
  if (!sd.FromString(security_descriptor))
    return false;
  ATL::CSecurityAttributes sa;
  sa.Set(sd);
  return token.CreatePrimaryToken(new_token, desired_access, &sa);
}

bool CreateImpersonationToken(SECURITY_IMPERSONATION_LEVEL impersonation_level,
                              ATL::CAccessToken* imp_token) {
  ATL::CAccessToken token;
  if (!token.GetProcessToken(MAXIMUM_ALLOWED))
    return false;
  return token.CreateImpersonationToken(imp_token, impersonation_level);
}

void CheckTokenError(const std::optional<AccessToken>& token,
                     DWORD expected_error) {
  DWORD error = ::GetLastError();
  EXPECT_FALSE(token);
  EXPECT_EQ(expected_error, error);
}

void CheckError(bool result, DWORD expected_error) {
  DWORD error = ::GetLastError();
  EXPECT_FALSE(result);
  EXPECT_EQ(expected_error, error);
}

void CheckAccessTokenGroup(DWORD attributes,
                           bool integrity,
                           bool enabled,
                           bool deny_only,
                           bool logon_id) {
  AccessToken::Group group(Sid(WellKnownSid::kWorld), attributes);
  EXPECT_EQ(L"S-1-1-0", *group.GetSid().ToSddlString());
  EXPECT_EQ(integrity, group.IsIntegrity());
  EXPECT_EQ(enabled, group.IsEnabled());
  EXPECT_EQ(deny_only, group.IsDenyOnly());
  EXPECT_EQ(logon_id, group.IsLogonId());
}

void CheckAccessTokenPrivilege(LPCWSTR name, DWORD attributes, bool enabled) {
  LUID luid;
  ASSERT_TRUE(::LookupPrivilegeValue(nullptr, name, &luid));
  CHROME_LUID chrome_luid;
  chrome_luid.LowPart = luid.LowPart;
  chrome_luid.HighPart = luid.HighPart;
  AccessToken::Privilege priv(chrome_luid, attributes);
  EXPECT_TRUE(CompareLuid(priv.GetLuid(), luid));
  EXPECT_EQ(name, priv.GetName());
  EXPECT_EQ(attributes, priv.GetAttributes());
  EXPECT_EQ(enabled, priv.IsEnabled());
}

typedef NTSTATUS(WINAPI* NtCreateLowBoxToken)(OUT PHANDLE token,
                                              IN HANDLE original_handle,
                                              IN ACCESS_MASK access,
                                              IN PVOID object_attribute,
                                              IN PSID appcontainer_sid,
                                              IN DWORD capabilityCount,
                                              IN PSID_AND_ATTRIBUTES
                                                  capabilities,
                                              IN DWORD handle_count,
                                              IN PHANDLE handles);

void CompareAppContainer(const Sid& package_sid, const std::vector<Sid>& caps) {
  static NtCreateLowBoxToken pNtCreateLowBoxToken =
      reinterpret_cast<NtCreateLowBoxToken>(::GetProcAddress(
          ::GetModuleHandle(L"ntdll.dll"), "NtCreateLowBoxToken"));
  ASSERT_TRUE(pNtCreateLowBoxToken);
  ATL::CTokenGroups capabilities;
  for (const Sid& cap : caps) {
    capabilities.Add(ATL::CSid(static_cast<SID*>(cap.GetPSID())),
                     SE_GROUP_ENABLED);
  }
  ATL::CAccessToken process_token;
  ASSERT_TRUE(process_token.GetProcessToken(TOKEN_ALL_ACCESS));
  UINT cap_count = capabilities.GetCount();
  PTOKEN_GROUPS cap_groups =
      const_cast<PTOKEN_GROUPS>(capabilities.GetPTOKEN_GROUPS());
  HANDLE tmp_token;
  NTSTATUS status = pNtCreateLowBoxToken(
      &tmp_token, process_token.GetHandle(), TOKEN_ALL_ACCESS, nullptr,
      package_sid.GetPSID(), cap_count,
      cap_count > 0 ? cap_groups->Groups : nullptr, 0, nullptr);
  ASSERT_EQ(0, status);
  ScopedHandle scoped_tmp_token(tmp_token);
  std::optional<AccessToken> ac_token =
      AccessToken::FromToken(scoped_tmp_token.get());
  ASSERT_TRUE(ac_token);
  EXPECT_TRUE(ac_token->IsAppContainer());
  EXPECT_EQ(ac_token->AppContainerSid(), package_sid);
  CompareGroups(ac_token->Capabilities(), capabilities);
}

ACCESS_MASK GetTokenAccess(const AccessToken& token) {
  std::optional<ACCESS_MASK> granted_access = GetGrantedAccess(token.get());
  CHECK(granted_access);
  return *granted_access;
}

std::vector<Sid> GroupsToSids(const std::vector<AccessToken::Group>& groups) {
  std::vector<Sid> sids;
  for (const AccessToken::Group& group : groups) {
    sids.push_back(group.GetSid().Clone());
  }
  return sids;
}

std::vector<std::wstring> PrivilegesToNames(
    const std::vector<AccessToken::Privilege>& privs) {
  std::vector<std::wstring> sids;
  for (const AccessToken::Privilege& priv : privs) {
    sids.push_back(priv.GetName());
  }
  return sids;
}
}  // namespace

TEST(AccessTokenTest, FromToken) {
  ATL::CAccessToken atl_token;
  ASSERT_TRUE(atl_token.GetProcessToken(TOKEN_QUERY));

  std::optional<AccessToken> token =
      AccessToken::FromToken(atl_token.GetHandle());
  ASSERT_TRUE(token);
  CompareTokens(*token, atl_token);
  EXPECT_EQ(GetTokenAccess(*token), DWORD{TOKEN_QUERY});

  std::optional<AccessToken> all_access_token =
      AccessToken::FromToken(atl_token.GetHandle(), kTokenAllNoQuery);
  ASSERT_TRUE(all_access_token);
  EXPECT_EQ(GetTokenAccess(*all_access_token), DWORD{TOKEN_ALL_ACCESS});

  // Check that we duplicated the handle.
  LUID luid;
  ASSERT_TRUE(atl_token.GetTokenId(&luid));
  ::CloseHandle(atl_token.Detach());
  LUID temp_luid;
  ASSERT_FALSE(atl_token.GetTokenId(&temp_luid));
  EXPECT_TRUE(CompareLuid(token->Id(), luid));

  // Check that we duplicate with the correct access rights.
  ASSERT_TRUE(atl_token.GetProcessToken(TOKEN_QUERY_SOURCE));
  ASSERT_FALSE(atl_token.GetTokenId(&temp_luid));
  std::optional<AccessToken> token2 =
      AccessToken::FromToken(atl_token.GetHandle());
  ASSERT_TRUE(token2);
  EXPECT_TRUE(CompareLuid(token2->Id(), luid));

  // Check that we fail if we don't have access to the token object.
  ASSERT_TRUE(atl_token.GetProcessToken(TOKEN_DUPLICATE));
  ATL::CAccessToken pri_token;
  ASSERT_TRUE(DuplicateTokenWithSecurityDescriptor(
      atl_token, TOKEN_QUERY_SOURCE, L"D:", &pri_token));
  CheckTokenError(AccessToken::FromToken(pri_token.GetHandle()),
                  ERROR_ACCESS_DENIED);

  CheckTokenError(AccessToken::FromToken(ScopedHandle(nullptr)),
                  ERROR_INVALID_HANDLE);
  ASSERT_TRUE(atl_token.GetProcessToken(TOKEN_QUERY));
  ScopedHandle token_handle(atl_token.Detach());
  token = AccessToken::FromToken(std::move(token_handle));
  ASSERT_TRUE(token);
  EXPECT_FALSE(token_handle.is_valid());
  EXPECT_TRUE(token->is_valid());
  ASSERT_TRUE(atl_token.GetProcessToken(TOKEN_DUPLICATE));
  token_handle.Set(atl_token.Detach());
  CheckTokenError(AccessToken::FromToken(std::move(token_handle)),
                  ERROR_ACCESS_DENIED);
}

TEST(AccessTokenTest, FromProcess) {
  ScopedHandle process(
      ::OpenProcess(PROCESS_TERMINATE, FALSE, ::GetCurrentProcessId()));
  ASSERT_TRUE(process.is_valid());
  CheckTokenError(AccessToken::FromProcess(process.get()), ERROR_ACCESS_DENIED);
  CheckTokenError(AccessToken::FromProcess(process.get(), true),
                  ERROR_ACCESS_DENIED);
  process.Set(::OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE,
                            ::GetCurrentProcessId()));
  ASSERT_TRUE(process.is_valid());
  std::optional<AccessToken> token = AccessToken::FromProcess(process.get());
  ASSERT_TRUE(token);
  EXPECT_EQ(GetTokenAccess(*token), DWORD{TOKEN_QUERY});
  ASSERT_FALSE(token->IsImpersonation());
  ATL::CAccessToken atl_token;
  ASSERT_TRUE(atl_token.GetProcessToken(TOKEN_QUERY, process.get()));
  CompareTokens(*token, atl_token);
  std::optional<AccessToken> imp_token =
      AccessToken::FromProcess(process.get(), true);
  ASSERT_TRUE(imp_token);
  ASSERT_TRUE(imp_token->IsImpersonation());
  ASSERT_TRUE(imp_token->IsIdentification());

  std::optional<AccessToken> all_access_token =
      AccessToken::FromProcess(process.get(), false, kTokenAllNoQuery);
  ASSERT_TRUE(all_access_token);
  EXPECT_EQ(GetTokenAccess(*all_access_token), DWORD{TOKEN_ALL_ACCESS});

  all_access_token =
      AccessToken::FromProcess(process.get(), true, kTokenAllNoQuery);
  ASSERT_TRUE(all_access_token);
  EXPECT_EQ(GetTokenAccess(*all_access_token), DWORD{TOKEN_ALL_ACCESS});
}

TEST(AccessTokenTest, FromCurrentProcess) {
  std::optional<AccessToken> token = AccessToken::FromCurrentProcess();
  ASSERT_TRUE(token);
  ASSERT_FALSE(token->IsImpersonation());
  ATL::CAccessToken atl_token;
  ASSERT_TRUE(atl_token.GetProcessToken(TOKEN_QUERY));
  CompareTokens(*token, atl_token);
  std::optional<AccessToken> imp_token = AccessToken::FromCurrentProcess(true);
  ASSERT_TRUE(imp_token);
  ASSERT_TRUE(imp_token->IsImpersonation());
  ASSERT_TRUE(imp_token->IsIdentification());

  std::optional<AccessToken> all_access_token =
      AccessToken::FromCurrentProcess(false, kTokenAllNoQuery);
  ASSERT_TRUE(all_access_token);
  EXPECT_EQ(GetTokenAccess(*all_access_token), DWORD{TOKEN_ALL_ACCESS});

  all_access_token = AccessToken::FromCurrentProcess(true, kTokenAllNoQuery);
  ASSERT_TRUE(all_access_token);
  EXPECT_EQ(GetTokenAccess(*all_access_token), DWORD{TOKEN_ALL_ACCESS});
}

TEST(AccessTokenTest, FromThread) {
  // Make sure we have no impersonation token before starting.
  ::RevertToSelf();
  // Check we
  CheckTokenError(AccessToken::FromThread(::GetCurrentThread()),
                  ERROR_NO_TOKEN);
  ScopedHandle thread(
      ::OpenThread(THREAD_TERMINATE, FALSE, ::GetCurrentThreadId()));
  ASSERT_TRUE(thread.is_valid());
  CheckTokenError(AccessToken::FromThread(thread.get()), ERROR_ACCESS_DENIED);

  ATL::CAccessToken atl_imp_token;
  ASSERT_TRUE(CreateImpersonationToken(SecurityImpersonation, &atl_imp_token));
  ASSERT_TRUE(atl_imp_token.Impersonate());
  CAutoRevertImpersonation scoped_imp(&atl_imp_token);

  thread.Set(::OpenThread(THREAD_QUERY_LIMITED_INFORMATION, FALSE,
                          ::GetCurrentThreadId()));
  ASSERT_TRUE(thread.is_valid());
  std::optional<AccessToken> imp_token = AccessToken::FromThread(thread.get());
  std::optional<AccessToken> all_access_token =
      AccessToken::FromThread(thread.get(), true, kTokenAllNoQuery);
  atl_imp_token.Revert();
  ASSERT_TRUE(imp_token);
  EXPECT_EQ(GetTokenAccess(*imp_token), DWORD{TOKEN_QUERY});
  ASSERT_TRUE(imp_token->IsImpersonation());
  EXPECT_FALSE(imp_token->IsIdentification());
  CompareTokens(*imp_token, atl_imp_token);
  ASSERT_TRUE(all_access_token);
  EXPECT_EQ(GetTokenAccess(*all_access_token), DWORD{TOKEN_ALL_ACCESS});

  ATL::CAccessToken atl_id_token;
  ASSERT_TRUE(CreateImpersonationToken(SecurityIdentification, &atl_id_token));
  ASSERT_TRUE(atl_id_token.Impersonate());
  CAutoRevertImpersonation scoped_imp2(&atl_id_token);
  CheckTokenError(AccessToken::FromThread(thread.get(), false),
                  ERROR_BAD_IMPERSONATION_LEVEL);
  std::optional<AccessToken> id_token =
      AccessToken::FromThread(thread.get(), true);
  atl_id_token.Revert();
  ASSERT_TRUE(id_token);
  EXPECT_TRUE(id_token->IsIdentification());
  CompareTokens(*id_token, atl_id_token);
}

TEST(AccessTokenTest, FromCurrentThread) {
  // Make sure we have no impersonation token before starting.
  ::RevertToSelf();
  // Check we don't have a token.
  CheckTokenError(AccessToken::FromCurrentThread(), ERROR_NO_TOKEN);

  ATL::CAccessToken atl_imp_token;
  ASSERT_TRUE(CreateImpersonationToken(SecurityImpersonation, &atl_imp_token));
  ASSERT_TRUE(atl_imp_token.Impersonate());
  CAutoRevertImpersonation scoped_imp(&atl_imp_token);

  std::optional<AccessToken> imp_token = AccessToken::FromCurrentThread();
  std::optional<AccessToken> all_access_token =
      AccessToken::FromCurrentThread(true, kTokenAllNoQuery);
  atl_imp_token.Revert();
  ASSERT_TRUE(imp_token);
  EXPECT_EQ(GetTokenAccess(*imp_token), DWORD{TOKEN_QUERY});
  ASSERT_TRUE(imp_token->IsImpersonation());
  EXPECT_FALSE(imp_token->IsIdentification());
  CompareTokens(*imp_token, atl_imp_token);
  ASSERT_TRUE(all_access_token);
  EXPECT_EQ(GetTokenAccess(*all_access_token), DWORD{TOKEN_ALL_ACCESS});

  ATL::CAccessToken atl_id_token;
  ASSERT_TRUE(CreateImpersonationToken(SecurityIdentification, &atl_id_token));
  ASSERT_TRUE(atl_id_token.Impersonate());
  ATL::CAutoRevertImpersonation scoped_imp2(&atl_id_token);
  CheckTokenError(AccessToken::FromCurrentThread(false),
                  ERROR_BAD_IMPERSONATION_LEVEL);
  std::optional<AccessToken> id_token = AccessToken::FromCurrentThread(true);
  atl_id_token.Revert();
  ASSERT_TRUE(id_token);
  EXPECT_TRUE(id_token->IsIdentification());
  CompareTokens(*id_token, atl_id_token);
}

TEST(AccessTokenTest, FromEffective) {
  // Make sure we have no impersonation token before starting.
  ::RevertToSelf();
  std::optional<base::win::AccessToken> primary_token =
      AccessToken::FromEffective();
  std::optional<base::win::AccessToken> all_access_token =
      AccessToken::FromEffective(kTokenAllNoQuery);
  ASSERT_TRUE(primary_token);
  EXPECT_EQ(GetTokenAccess(*primary_token), DWORD{TOKEN_QUERY});
  EXPECT_FALSE(primary_token->IsImpersonation());
  ASSERT_TRUE(all_access_token);
  EXPECT_EQ(GetTokenAccess(*all_access_token), DWORD{TOKEN_ALL_ACCESS});

  ATL::CAccessToken atl_primary_token;
  ASSERT_TRUE(atl_primary_token.GetProcessToken(TOKEN_QUERY));
  CompareTokens(*primary_token, atl_primary_token);

  ATL::CAccessToken atl_imp_token;
  ASSERT_TRUE(CreateImpersonationToken(SecurityImpersonation, &atl_imp_token));
  ASSERT_TRUE(atl_imp_token.Impersonate());
  CAutoRevertImpersonation scoped_imp(&atl_imp_token);

  std::optional<AccessToken> imp_token = AccessToken::FromEffective();
  all_access_token = AccessToken::FromEffective(kTokenAllNoQuery);
  atl_imp_token.Revert();
  ASSERT_TRUE(imp_token);
  ASSERT_TRUE(imp_token->IsImpersonation());
  EXPECT_FALSE(imp_token->IsIdentification());
  CompareTokens(*imp_token, atl_imp_token);
  ASSERT_TRUE(all_access_token);
  EXPECT_EQ(GetTokenAccess(*all_access_token), DWORD{TOKEN_ALL_ACCESS});
}

TEST(AccessTokenTest, AccessTokenGroup) {
  CheckAccessTokenGroup(0, false, false, false, false);
  CheckAccessTokenGroup(SE_GROUP_INTEGRITY, true, false, false, false);
  CheckAccessTokenGroup(SE_GROUP_ENABLED, false, true, false, false);
  CheckAccessTokenGroup(SE_GROUP_USE_FOR_DENY_ONLY, false, false, true, false);
  CheckAccessTokenGroup(SE_GROUP_LOGON_ID, false, false, false, true);
  CheckAccessTokenGroup(0xFFFFFFFF, true, true, true, true);
}

TEST(AccessTokenTest, AccessTokenPrivilege) {
  CheckAccessTokenPrivilege(SE_DEBUG_NAME, 0, false);
  CheckAccessTokenPrivilege(SE_DEBUG_NAME, SE_PRIVILEGE_ENABLED_BY_DEFAULT,
                            false);
  CheckAccessTokenPrivilege(SE_DEBUG_NAME, SE_PRIVILEGE_ENABLED, true);
  CheckAccessTokenPrivilege(
      SE_DEBUG_NAME, SE_PRIVILEGE_ENABLED | SE_PRIVILEGE_ENABLED_BY_DEFAULT,
      true);
  CheckAccessTokenPrivilege(SE_IMPERSONATE_NAME, 0, false);
  CHROME_LUID luid{0x8181, 0x5656};
  AccessToken::Privilege priv(luid, 0);
  EXPECT_EQ(L"00005656-00008181", priv.GetName());
}

TEST(AccessTokenTest, IsMember) {
  std::optional<AccessToken> token = AccessToken::FromCurrentProcess();
  ASSERT_TRUE(token);
  ASSERT_FALSE(token->IsImpersonation());
  CheckError(token->IsMember(WellKnownSid::kWorld),
             ERROR_NO_IMPERSONATION_TOKEN);
  std::optional<Sid> sid = Sid::FromSddlString(L"S-1-1-2-3-4-5-6-7-8");
  ASSERT_TRUE(sid);
  CheckError(token->IsMember(*sid), ERROR_NO_IMPERSONATION_TOKEN);

  std::optional<AccessToken> imp_token = AccessToken::FromCurrentProcess(true);
  EXPECT_TRUE(imp_token->IsMember(WellKnownSid::kWorld));
  EXPECT_FALSE(imp_token->IsMember(WellKnownSid::kNull));
  EXPECT_TRUE(imp_token->IsMember(imp_token->User()));
  EXPECT_FALSE(imp_token->IsMember(*sid));
}

TEST(AccessTokenTest, Restricted) {
  ATL::CAccessToken atl_token;
  ASSERT_TRUE(atl_token.GetProcessToken(MAXIMUM_ALLOWED));
  ATL::CTokenGroups disable_groups;
  disable_groups.Add(ATL::Sids::World(), 0);
  ATL::CTokenGroups restrict_groups;
  restrict_groups.Add(ATL::Sids::RestrictedCode(), 0);
  restrict_groups.Add(ATL::Sids::Users(), 0);
  ATL::CAccessToken atl_restricted;
  ASSERT_TRUE(atl_token.CreateRestrictedToken(&atl_restricted, disable_groups,
                                              restrict_groups));
  std::optional<AccessToken> restricted =
      AccessToken::FromToken(atl_restricted.GetHandle());
  ASSERT_TRUE(restricted);
  EXPECT_TRUE(restricted->IsRestricted());
  CompareTokens(*restricted, atl_restricted, false);
  ATL::CSid::CSidArray sids;
  restrict_groups.GetSidsAndAttributes(&sids);
  ATL::CAtlArray<DWORD> attrs;
  for (UINT index = 0; index < sids.GetCount(); ++index) {
    attrs.Add(SE_GROUP_MANDATORY | SE_GROUP_ENABLED_BY_DEFAULT |
              SE_GROUP_ENABLED);
  }
  CompareGroups(restricted->RestrictedSids(), sids, attrs);
}

TEST(AccessTokenTest, AppContainer) {
  std::optional<Sid> package_sid =
      Sid::FromSddlString(L"S-1-15-2-1-2-3-4-5-6-7");
  ASSERT_TRUE(package_sid);
  std::optional<std::vector<Sid>> caps =
      Sid::FromKnownCapabilityVector({WellKnownCapability::kInternetClient,
                                      WellKnownCapability::kDocumentsLibrary});
  ASSERT_TRUE(caps);

  CompareAppContainer(*package_sid, *caps);
  CompareAppContainer(*package_sid, {});
}

TEST(AccessTokenTest, Anonymous) {
  ATL::CAccessToken atl_anon_token;
  ASSERT_TRUE(::ImpersonateAnonymousToken(::GetCurrentThread()));
  bool result = atl_anon_token.GetThreadToken(TOKEN_ALL_ACCESS);
  ::RevertToSelf();
  ASSERT_TRUE(result);
  std::optional<AccessToken> anon_token =
      AccessToken::FromToken(atl_anon_token.GetHandle());
  ASSERT_TRUE(anon_token);
  CompareTokens(*anon_token, atl_anon_token);
  EXPECT_EQ(Sid(WellKnownSid::kAnonymous), anon_token->User());
  std::optional<Sid> logon_sid = anon_token->LogonId();
  EXPECT_FALSE(anon_token->LogonId());
  EXPECT_EQ(DWORD{ERROR_NOT_FOUND}, ::GetLastError());
}

TEST(AccessTokenTest, SetDefaultDacl) {
  ATL::CAccessToken atl_token;
  ASSERT_TRUE(atl_token.GetProcessToken(MAXIMUM_ALLOWED));
  ATL::CAccessToken atl_dup_token;
  ASSERT_TRUE(atl_token.CreatePrimaryToken(&atl_dup_token));
  std::optional<AccessToken> read_only_token =
      AccessToken::FromToken(atl_dup_token.GetHandle());
  AccessControlList default_dacl;
  Sid world_sid(WellKnownSid::kWorld);
  ASSERT_TRUE(default_dacl.SetEntry(world_sid, SecurityAccessMode::kGrant,
                                    GENERIC_ALL, 0));
  EXPECT_FALSE(read_only_token->SetDefaultDacl(default_dacl));
  std::optional<AccessToken> token =
      AccessToken::FromToken(atl_dup_token.GetHandle(), TOKEN_ADJUST_DEFAULT);
  EXPECT_TRUE(token->SetDefaultDacl(default_dacl));

  CDacl atl_dacl;
  ASSERT_TRUE(atl_dup_token.GetDefaultDacl(&atl_dacl));
  ASSERT_EQ(atl_dacl.GetAceCount(), 1U);
  ATL::CSid::CSidArray sids;
  CAcl::CAccessMaskArray access;
  CAcl::CAceTypeArray types;
  CAcl::CAceFlagArray flags;
  atl_dacl.GetAclEntries(&sids, &access, &types, &flags);
  EXPECT_TRUE(EqualSid(world_sid, sids[0]));
  EXPECT_EQ(access[0], DWORD{GENERIC_ALL});
  EXPECT_EQ(types[0], ACCESS_ALLOWED_ACE_TYPE);
  EXPECT_EQ(flags[0], 0U);
}

TEST(AccessTokenTest, SetIntegrityLevel) {
  ATL::CAccessToken atl_token;
  ASSERT_TRUE(atl_token.GetProcessToken(MAXIMUM_ALLOWED));
  ATL::CAccessToken atl_dup_token;
  ASSERT_TRUE(atl_token.CreatePrimaryToken(&atl_dup_token));
  std::optional<AccessToken> read_only_token =
      AccessToken::FromToken(atl_dup_token.GetHandle());
  EXPECT_FALSE(
      read_only_token->SetIntegrityLevel(SECURITY_MANDATORY_UNTRUSTED_RID));
  std::optional<AccessToken> token =
      AccessToken::FromToken(atl_dup_token.GetHandle(), TOKEN_ADJUST_DEFAULT);
  EXPECT_TRUE(token->SetIntegrityLevel(SECURITY_MANDATORY_LOW_RID));
  EXPECT_EQ(token->IntegrityLevel(), DWORD{SECURITY_MANDATORY_LOW_RID});
  EXPECT_TRUE(token->SetIntegrityLevel(SECURITY_MANDATORY_UNTRUSTED_RID));
  EXPECT_EQ(token->IntegrityLevel(), DWORD{SECURITY_MANDATORY_UNTRUSTED_RID});
}

TEST(AccessTokenTest, DuplicatePrimary) {
  std::optional<AccessToken> token = AccessToken::FromCurrentProcess();
  ASSERT_TRUE(token);
  CheckTokenError(token->DuplicatePrimary(), ERROR_ACCESS_DENIED);
  token = AccessToken::FromCurrentProcess(false, TOKEN_DUPLICATE);
  ASSERT_TRUE(token);
  std::optional<AccessToken> dup_token = token->DuplicatePrimary();
  ASSERT_TRUE(dup_token);
  EXPECT_FALSE(dup_token->IsImpersonation());
  EXPECT_EQ(GetTokenAccess(*dup_token), DWORD{TOKEN_QUERY});
  dup_token = token->DuplicatePrimary(kTokenAllNoQuery);
  ASSERT_TRUE(dup_token);
  EXPECT_FALSE(dup_token->IsImpersonation());
  EXPECT_EQ(GetTokenAccess(*dup_token), DWORD{TOKEN_ALL_ACCESS});
}

TEST(AccessTokenTest, DuplicateImpersonation) {
  std::optional<AccessToken> token = AccessToken::FromCurrentProcess();
  ASSERT_TRUE(token);
  CheckTokenError(
      token->DuplicateImpersonation(SecurityImpersonationLevel::kImpersonation),
      ERROR_ACCESS_DENIED);
  token = AccessToken::FromCurrentProcess(false, TOKEN_DUPLICATE);
  ASSERT_TRUE(token);
  std::optional<AccessToken> dup_token = token->DuplicateImpersonation();
  ASSERT_TRUE(dup_token);
  EXPECT_TRUE(dup_token->IsImpersonation());
  EXPECT_FALSE(dup_token->IsIdentification());
  EXPECT_EQ(dup_token->ImpersonationLevel(),
            SecurityImpersonationLevel::kImpersonation);
  dup_token =
      token->DuplicateImpersonation(SecurityImpersonationLevel::kImpersonation);
  ASSERT_TRUE(dup_token);
  EXPECT_TRUE(dup_token->IsImpersonation());
  EXPECT_FALSE(dup_token->IsIdentification());
  EXPECT_EQ(dup_token->ImpersonationLevel(),
            SecurityImpersonationLevel::kImpersonation);
  dup_token =
      token->DuplicateImpersonation(SecurityImpersonationLevel::kAnonymous);
  ASSERT_TRUE(dup_token);
  EXPECT_EQ(dup_token->ImpersonationLevel(),
            SecurityImpersonationLevel::kAnonymous);
  dup_token = token->DuplicateImpersonation(
      SecurityImpersonationLevel::kIdentification);
  ASSERT_TRUE(dup_token);
  EXPECT_EQ(dup_token->ImpersonationLevel(),
            SecurityImpersonationLevel::kIdentification);
  dup_token =
      token->DuplicateImpersonation(SecurityImpersonationLevel::kDelegation);
  ASSERT_TRUE(dup_token);
  EXPECT_EQ(dup_token->ImpersonationLevel(),
            SecurityImpersonationLevel::kDelegation);
  EXPECT_EQ(GetTokenAccess(*dup_token), DWORD{TOKEN_QUERY});
  dup_token = token->DuplicateImpersonation(
      SecurityImpersonationLevel::kImpersonation, kTokenAllNoQuery);
  ASSERT_TRUE(dup_token);
  EXPECT_TRUE(dup_token->IsImpersonation());
  EXPECT_FALSE(dup_token->IsIdentification());
  EXPECT_EQ(GetTokenAccess(*dup_token), DWORD{TOKEN_ALL_ACCESS});
  dup_token = token->DuplicateImpersonation(
      SecurityImpersonationLevel::kIdentification, TOKEN_DUPLICATE);
  ASSERT_TRUE(dup_token);
  CheckTokenError(dup_token->DuplicateImpersonation(
                      SecurityImpersonationLevel::kImpersonation),
                  ERROR_BAD_IMPERSONATION_LEVEL);
}

TEST(AccessTokenTest, CreateRestricted) {
  std::optional<AccessToken> primary_token = AccessToken::FromCurrentProcess();
  ASSERT_TRUE(primary_token);
  CheckTokenError(primary_token->CreateRestricted(0, {}, {}, {}),
                  ERROR_ACCESS_DENIED);
  primary_token = AccessToken::FromCurrentProcess(false, TOKEN_ALL_ACCESS);
  std::optional<AccessToken> restricted_token =
      primary_token->CreateRestricted(DISABLE_MAX_PRIVILEGE, {}, {}, {});
  ASSERT_TRUE(restricted_token);
  EXPECT_FALSE(restricted_token->IsRestricted());
  EXPECT_EQ(GetTokenAccess(*restricted_token), DWORD{TOKEN_QUERY});
  restricted_token = primary_token->CreateRestricted(DISABLE_MAX_PRIVILEGE, {},
                                                     {}, {}, kTokenAllNoQuery);
  ASSERT_TRUE(restricted_token);
  EXPECT_EQ(GetTokenAccess(*restricted_token), DWORD{TOKEN_ALL_ACCESS});

  std::vector<AccessToken::Privilege> privs = restricted_token->Privileges();
  ASSERT_EQ(privs.size(), 1U);
  EXPECT_EQ(privs[0].GetName(), SE_CHANGE_NOTIFY_NAME);
  std::vector<std::wstring> priv_names = {L"ThisIsNotValid"};
  EXPECT_FALSE(primary_token->CreateRestricted(0, {}, priv_names, {}));
  restricted_token = primary_token->CreateRestricted(
      0, {}, PrivilegesToNames(primary_token->Privileges()), {});
  ASSERT_TRUE(restricted_token);
  EXPECT_FALSE(restricted_token->IsRestricted());
  EXPECT_TRUE(restricted_token->Privileges().empty());
  std::vector<AccessToken::Group> groups = primary_token->Groups();
  restricted_token =
      primary_token->CreateRestricted(0, GroupsToSids(groups), {}, {});
  ASSERT_TRUE(restricted_token);
  EXPECT_FALSE(restricted_token->IsRestricted());
  for (const AccessToken::Group& group : restricted_token->Groups()) {
    if (!group.IsIntegrity()) {
      EXPECT_TRUE(group.IsDenyOnly());
    }
  }
  restricted_token =
      primary_token->CreateRestricted(0, {}, {}, GroupsToSids(groups));
  ASSERT_TRUE(restricted_token);
  EXPECT_TRUE(restricted_token->IsRestricted());
  std::vector<AccessToken::Group> restricted_sids =
      restricted_token->RestrictedSids();
  ASSERT_EQ(groups.size(), restricted_sids.size());
  for (size_t i = 0; i < groups.size(); ++i) {
    EXPECT_EQ(groups[i].GetSid(), restricted_sids[i].GetSid());
  }
}

TEST(AccessTokenTest, CreateAppContainer) {
  std::optional<AccessToken> primary_token = AccessToken::FromCurrentProcess();
  ASSERT_TRUE(primary_token);
  std::optional<Sid> package_sid =
      Sid::FromSddlString(L"S-1-15-2-1-2-3-4-5-6-7");
  ASSERT_TRUE(package_sid);
  CheckTokenError(primary_token->CreateAppContainer(*package_sid, {}),
                  ERROR_ACCESS_DENIED);
  primary_token = AccessToken::FromCurrentProcess(false, TOKEN_ALL_ACCESS);
  std::optional<AccessToken> ac_token =
      primary_token->CreateAppContainer(*package_sid, {});
  ASSERT_TRUE(ac_token);
  EXPECT_EQ(GetTokenAccess(*ac_token), DWORD{TOKEN_QUERY});
  EXPECT_TRUE(ac_token->IsAppContainer());
  EXPECT_EQ(ac_token->AppContainerSid(), package_sid);
  EXPECT_EQ(ac_token->Capabilities().size(), 0U);
  ac_token =
      primary_token->CreateAppContainer(*package_sid, {}, kTokenAllNoQuery);
  ASSERT_TRUE(ac_token);
  EXPECT_TRUE(ac_token->IsAppContainer());
  EXPECT_EQ(GetTokenAccess(*ac_token), DWORD{TOKEN_ALL_ACCESS});

  std::optional<std::vector<Sid>> caps =
      Sid::FromKnownCapabilityVector({WellKnownCapability::kInternetClient,
                                      WellKnownCapability::kDocumentsLibrary});
  ASSERT_TRUE(caps);
  ac_token = primary_token->CreateAppContainer(*package_sid, *caps);
  ASSERT_TRUE(ac_token);
  EXPECT_EQ(GetTokenAccess(*ac_token), DWORD{TOKEN_QUERY});
  EXPECT_TRUE(ac_token->IsAppContainer());
  EXPECT_EQ(ac_token->AppContainerSid(), package_sid);
  std::vector<AccessToken::Group> cap_groups = ac_token->Capabilities();
  ASSERT_EQ(cap_groups.size(), caps->size());
  for (size_t index = 0; index < cap_groups.size(); ++index) {
    EXPECT_EQ(cap_groups[index].GetSid(), caps->at(index));
    EXPECT_EQ(cap_groups[index].GetAttributes(), DWORD{SE_GROUP_ENABLED});
  }
}

TEST(AccessTokenTest, SetPrivilege) {
  std::optional<AccessToken> token = AccessToken::FromCurrentProcess(true);
  EXPECT_FALSE(token->SetPrivilege(SE_CHANGE_NOTIFY_NAME, false));
  token = AccessToken::FromCurrentProcess(true, TOKEN_ADJUST_PRIVILEGES);
  EXPECT_FALSE(token->SetPrivilege(L"ThisIsNotValid", false));
  std::optional<bool> original_state =
      token->SetPrivilege(SE_CHANGE_NOTIFY_NAME, false);
  EXPECT_TRUE(original_state);
  std::optional<bool> curr_state =
      token->SetPrivilege(SE_CHANGE_NOTIFY_NAME, true);
  EXPECT_TRUE(curr_state);
  EXPECT_FALSE(*curr_state);
  curr_state = token->SetPrivilege(SE_CHANGE_NOTIFY_NAME, false);
  EXPECT_TRUE(curr_state);
  EXPECT_TRUE(*curr_state);
  EXPECT_TRUE(token->SetPrivilege(SE_CHANGE_NOTIFY_NAME, *original_state));
}

TEST(AccessTokenTest, RemovePrivilege) {
  std::optional<AccessToken> token = AccessToken::FromCurrentProcess(true);
  EXPECT_FALSE(token->RemovePrivilege(SE_CHANGE_NOTIFY_NAME));
  token = AccessToken::FromCurrentProcess(true, TOKEN_ADJUST_PRIVILEGES);
  EXPECT_FALSE(token->RemovePrivilege(L"ThisIsNotValid"));
  EXPECT_TRUE(token->RemovePrivilege(SE_CHANGE_NOTIFY_NAME));
  EXPECT_FALSE(token->RemovePrivilege(SE_CHANGE_NOTIFY_NAME));
}

TEST(AccessTokenTest, RemoveAllPrivileges) {
  std::optional<AccessToken> token = AccessToken::FromCurrentProcess(true);
  EXPECT_FALSE(token->RemoveAllPrivileges());
  token = AccessToken::FromCurrentProcess(true, TOKEN_ADJUST_PRIVILEGES);
  EXPECT_TRUE(token->RemoveAllPrivileges());
  EXPECT_EQ(token->Privileges().size(), 0U);
  EXPECT_TRUE(token->RemoveAllPrivileges());
}

TEST(AccessTokenTest, CheckRelease) {
  std::optional<AccessToken> token = AccessToken::FromCurrentProcess();
  ASSERT_TRUE(token);
  EXPECT_TRUE(token->is_valid());
  ScopedHandle handle(token->release());
  EXPECT_TRUE(handle.is_valid());
  EXPECT_FALSE(token->is_valid());
  EXPECT_EQ(token->get(), nullptr);
}

}  // namespace base::win
