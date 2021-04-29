// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/win/sid.h"

#include <windows.h>

#include <sddl.h>
#include <stdlib.h>

#include "base/check.h"
#include "base/notreached.h"
#include "base/rand_util.h"
#include "base/stl_util.h"
#include "base/win/scoped_handle.h"
#include "base/win/scoped_localalloc.h"
#include "base/win/windows_version.h"

namespace base {
namespace win {

namespace {

Optional<DWORD> WellKnownCapabilityToRid(WellKnownCapability capability) {
  switch (capability) {
    case WellKnownCapability::kInternetClient:
      return SECURITY_CAPABILITY_INTERNET_CLIENT;
    case WellKnownCapability::kInternetClientServer:
      return SECURITY_CAPABILITY_INTERNET_CLIENT_SERVER;
    case WellKnownCapability::kPrivateNetworkClientServer:
      return SECURITY_CAPABILITY_PRIVATE_NETWORK_CLIENT_SERVER;
    case WellKnownCapability::kPicturesLibrary:
      return SECURITY_CAPABILITY_PICTURES_LIBRARY;
    case WellKnownCapability::kVideosLibrary:
      return SECURITY_CAPABILITY_VIDEOS_LIBRARY;
    case WellKnownCapability::kMusicLibrary:
      return SECURITY_CAPABILITY_MUSIC_LIBRARY;
    case WellKnownCapability::kDocumentsLibrary:
      return SECURITY_CAPABILITY_DOCUMENTS_LIBRARY;
    case WellKnownCapability::kEnterpriseAuthentication:
      return SECURITY_CAPABILITY_ENTERPRISE_AUTHENTICATION;
    case WellKnownCapability::kSharedUserCertificates:
      return SECURITY_CAPABILITY_SHARED_USER_CERTIFICATES;
    case WellKnownCapability::kRemovableStorage:
      return SECURITY_CAPABILITY_REMOVABLE_STORAGE;
    case WellKnownCapability::kAppointments:
      return SECURITY_CAPABILITY_APPOINTMENTS;
    case WellKnownCapability::kContacts:
      return SECURITY_CAPABILITY_CONTACTS;
  }
  return nullopt;
}

Optional<WELL_KNOWN_SID_TYPE> WellKnownSidToEnum(WellKnownSid sid) {
  switch (sid) {
    case WellKnownSid::kNull:
      return WinNullSid;
    case WellKnownSid::kWorld:
      return WinWorldSid;
    case WellKnownSid::kCreatorOwner:
      return WinCreatorOwnerSid;
    case WellKnownSid::kNetwork:
      return WinNetworkSid;
    case WellKnownSid::kBatch:
      return WinBatchSid;
    case WellKnownSid::kInteractive:
      return WinInteractiveSid;
    case WellKnownSid::kService:
      return WinServiceSid;
    case WellKnownSid::kAnonymous:
      return WinAnonymousSid;
    case WellKnownSid::kSelf:
      return WinSelfSid;
    case WellKnownSid::kAuthenticatedUser:
      return WinAuthenticatedUserSid;
    case WellKnownSid::kRestricted:
      return WinRestrictedCodeSid;
    case WellKnownSid::kLocalSystem:
      return WinLocalSystemSid;
    case WellKnownSid::kLocalService:
      return WinLocalServiceSid;
    case WellKnownSid::kNetworkService:
      return WinNetworkServiceSid;
    case WellKnownSid::kBuiltinAdministrators:
      return WinBuiltinAdministratorsSid;
    case WellKnownSid::kBuiltinUsers:
      return WinBuiltinUsersSid;
    case WellKnownSid::kBuiltinGuests:
      return WinBuiltinGuestsSid;
    case WellKnownSid::kUntrustedLabel:
      return WinUntrustedLabelSid;
    case WellKnownSid::kLowLabel:
      return WinLowLabelSid;
    case WellKnownSid::kMediumLabel:
      return WinMediumLabelSid;
    case WellKnownSid::kHighLabel:
      return WinHighLabelSid;
    case WellKnownSid::kSystemLabel:
      return WinSystemLabelSid;
    case WellKnownSid::kWriteRestricted:
      return WinWriteRestrictedCodeSid;
    case WellKnownSid::kCreatorOwnerRights:
      return WinCreatorOwnerRightsSid;
    case WellKnownSid::kAllApplicationPackages:
      return WinBuiltinAnyPackageSid;
    case WellKnownSid::kAllRestrictedApplicationPackages:
      // This should be handled by FromKnownSid.
      NOTREACHED();
      break;
  }
  return nullopt;
}

Optional<Sid> FromSubAuthorities(PSID_IDENTIFIER_AUTHORITY identifier_authority,
                                 BYTE sub_authority_count,
                                 PDWORD sub_authorities) {
  BYTE sid[SECURITY_MAX_SID_SIZE];
  if (!::InitializeSid(sid, identifier_authority, sub_authority_count))
    return nullopt;

  for (DWORD index = 0; index < sub_authority_count; ++index) {
    PDWORD sub_authority = ::GetSidSubAuthority(sid, index);
    *sub_authority = sub_authorities[index];
  }
  return Sid::FromPSID(sid);
}

Optional<std::vector<Sid>> FromStringVector(
    const std::vector<const wchar_t*>& strs,
    decltype(Sid::FromSddlString)* create_sid) {
  std::vector<Sid> converted_sids;
  converted_sids.reserve(strs.size());
  for (const wchar_t* str : strs) {
    auto sid = create_sid(str);
    if (!sid)
      return nullopt;
    converted_sids.push_back(std::move(*sid));
  }
  return converted_sids;
}

}  // namespace

Sid::Sid(const void* sid, size_t length)
    : sid_(static_cast<const char*>(sid),
           static_cast<const char*>(sid) + length) {}

Optional<Sid> Sid::FromKnownCapability(WellKnownCapability capability) {
  Optional<DWORD> capability_rid = WellKnownCapabilityToRid(capability);
  if (!capability_rid)
    return nullopt;
  SID_IDENTIFIER_AUTHORITY capability_authority = {
      SECURITY_APP_PACKAGE_AUTHORITY};
  DWORD sub_authorities[] = {SECURITY_CAPABILITY_BASE_RID, *capability_rid};
  return FromSubAuthorities(&capability_authority, size(sub_authorities),
                            sub_authorities);
}

Optional<Sid> Sid::FromNamedCapability(const wchar_t* capability_name) {
  DCHECK_GE(GetVersion(), Version::WIN10);

  if (!capability_name || !*capability_name)
    return nullopt;

  typedef decltype(
      ::DeriveCapabilitySidsFromName)* DeriveCapabilitySidsFromNameFunc;
  static const DeriveCapabilitySidsFromNameFunc derive_capability_sids =
      []() -> DeriveCapabilitySidsFromNameFunc {
    HMODULE module = GetModuleHandle(L"api-ms-win-security-base-l1-2-2.dll");
    if (!module)
      return nullptr;

    return reinterpret_cast<DeriveCapabilitySidsFromNameFunc>(
        ::GetProcAddress(module, "DeriveCapabilitySidsFromName"));
  }();
  if (!derive_capability_sids)
    return nullopt;

  // Pre-reserve some space for SID deleters.
  std::vector<ScopedLocalAlloc> deleter_list;
  deleter_list.reserve(16);

  PSID* capability_groups = nullptr;
  DWORD capability_group_count = 0;
  PSID* capability_sids = nullptr;
  DWORD capability_sid_count = 0;

  if (!derive_capability_sids(capability_name, &capability_groups,
                              &capability_group_count, &capability_sids,
                              &capability_sid_count)) {
    return nullopt;
  }

  deleter_list.emplace_back(capability_groups);
  deleter_list.emplace_back(capability_sids);

  for (DWORD i = 0; i < capability_group_count; ++i) {
    deleter_list.emplace_back(capability_groups[i]);
  }
  for (DWORD i = 0; i < capability_sid_count; ++i) {
    deleter_list.emplace_back(capability_sids[i]);
  }

  if (capability_sid_count < 1)
    return nullopt;

  return FromPSID(capability_sids[0]);
}

Optional<Sid> Sid::FromKnownSid(WellKnownSid type) {
  if (type == WellKnownSid::kAllRestrictedApplicationPackages) {
    SID_IDENTIFIER_AUTHORITY package_authority = {
        SECURITY_APP_PACKAGE_AUTHORITY};
    DWORD sub_authorities[] = {SECURITY_APP_PACKAGE_BASE_RID,
                               SECURITY_BUILTIN_PACKAGE_ANY_RESTRICTED_PACKAGE};
    return FromSubAuthorities(&package_authority, 2, sub_authorities);
  }

  BYTE sid[SECURITY_MAX_SID_SIZE];
  DWORD size_sid = SECURITY_MAX_SID_SIZE;
  Optional<WELL_KNOWN_SID_TYPE> known_sid = WellKnownSidToEnum(type);
  if (!known_sid)
    return nullopt;
  if (!::CreateWellKnownSid(*known_sid, nullptr, sid, &size_sid))
    return nullopt;

  return Sid(sid, size_sid);
}

Optional<Sid> Sid::FromSddlString(const wchar_t* sddl_sid) {
  PSID psid = nullptr;
  if (!::ConvertStringSidToSid(sddl_sid, &psid))
    return nullopt;
  return FromPSID(TakeLocalAlloc(psid).get());
}

Optional<Sid> Sid::FromPSID(PSID sid) {
  DCHECK(sid);
  if (!sid || !::IsValidSid(sid))
    return nullopt;
  return Sid(sid, ::GetLengthSid(sid));
}

Optional<Sid> Sid::GenerateRandomSid() {
  SID_IDENTIFIER_AUTHORITY package_authority = {SECURITY_NULL_SID_AUTHORITY};
  DWORD sub_authorities[4] = {};
  RandBytes(&sub_authorities, sizeof(sub_authorities));
  return FromSubAuthorities(&package_authority, _countof(sub_authorities),
                            sub_authorities);
}

Optional<Sid> Sid::CurrentUser() {
  // Get the current token.
  HANDLE token = nullptr;
  if (!::OpenProcessToken(::GetCurrentProcess(), TOKEN_QUERY, &token))
    return nullopt;
  ScopedHandle token_scoped(token);

  char user_buffer[sizeof(TOKEN_USER) + SECURITY_MAX_SID_SIZE];
  DWORD size = sizeof(user_buffer);

  if (!::GetTokenInformation(token, TokenUser, user_buffer, size, &size))
    return nullopt;

  TOKEN_USER* user = reinterpret_cast<TOKEN_USER*>(user_buffer);
  if (!user->User.Sid)
    return nullopt;
  return Sid::FromPSID(user->User.Sid);
}

Optional<Sid> Sid::FromIntegrityLevel(DWORD integrity_level) {
  SID_IDENTIFIER_AUTHORITY package_authority = {
      SECURITY_MANDATORY_LABEL_AUTHORITY};
  return FromSubAuthorities(&package_authority, 1, &integrity_level);
}

Optional<std::vector<Sid>> Sid::FromSddlStringVector(
    const std::vector<const wchar_t*>& sddl_sids) {
  return FromStringVector(sddl_sids, Sid::FromSddlString);
}

Optional<std::vector<Sid>> Sid::FromNamedCapabilityVector(
    const std::vector<const wchar_t*>& capability_names) {
  return FromStringVector(capability_names, Sid::FromNamedCapability);
}

Sid::Sid(Sid&& sid) = default;
Sid::~Sid() = default;

PSID Sid::GetPSID() const {
  return const_cast<char*>(sid_.data());
}

// Converts the SID to an SDDL format string.
Optional<std::wstring> Sid::ToSddlString() const {
  LPWSTR sid = nullptr;
  if (!::ConvertSidToStringSid(GetPSID(), &sid))
    return nullopt;
  return TakeLocalAlloc(sid).get();
}

}  // namespace win
}  // namespace base
