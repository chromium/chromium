// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/win/sid.h"

#include <windows.h>

#include <sddl.h>
#include <stdlib.h>

#include "base/check.h"
#include "base/cxx17_backports.h"
#include "base/notreached.h"
#include "base/rand_util.h"
#include "base/win/scoped_handle.h"
#include "base/win/scoped_localalloc.h"
#include "base/win/windows_version.h"

namespace base {
namespace win {

namespace {

absl::optional<DWORD> WellKnownCapabilityToRid(WellKnownCapability capability) {
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
  return absl::nullopt;
}

absl::optional<WELL_KNOWN_SID_TYPE> WellKnownSidToEnum(WellKnownSid sid) {
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
  return absl::nullopt;
}

absl::optional<Sid> FromSubAuthorities(
    PSID_IDENTIFIER_AUTHORITY identifier_authority,
    BYTE sub_authority_count,
    PDWORD sub_authorities) {
  BYTE sid[SECURITY_MAX_SID_SIZE];
  if (!::InitializeSid(sid, identifier_authority, sub_authority_count))
    return absl::nullopt;

  for (DWORD index = 0; index < sub_authority_count; ++index) {
    PDWORD sub_authority = ::GetSidSubAuthority(sid, index);
    *sub_authority = sub_authorities[index];
  }
  return Sid::FromPSID(sid);
}

absl::optional<std::vector<Sid>> FromStringVector(
    const std::vector<const wchar_t*>& strs,
    decltype(Sid::FromSddlString)* create_sid) {
  std::vector<Sid> converted_sids;
  converted_sids.reserve(strs.size());
  for (const wchar_t* str : strs) {
    auto sid = create_sid(str);
    if (!sid)
      return absl::nullopt;
    converted_sids.push_back(std::move(*sid));
  }
  return converted_sids;
}

}  // namespace

Sid::Sid(const void* sid, size_t length)
    : sid_(static_cast<const char*>(sid),
           static_cast<const char*>(sid) + length) {}

absl::optional<Sid> Sid::FromKnownCapability(WellKnownCapability capability) {
  absl::optional<DWORD> capability_rid = WellKnownCapabilityToRid(capability);
  if (!capability_rid)
    return absl::nullopt;
  SID_IDENTIFIER_AUTHORITY capability_authority = {
      SECURITY_APP_PACKAGE_AUTHORITY};
  DWORD sub_authorities[] = {SECURITY_CAPABILITY_BASE_RID, *capability_rid};
  return FromSubAuthorities(&capability_authority, size(sub_authorities),
                            sub_authorities);
}

absl::optional<Sid> Sid::FromNamedCapability(const wchar_t* capability_name) {
  DCHECK_GE(GetVersion(), Version::WIN10);

  if (!capability_name || !*capability_name)
    return absl::nullopt;

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
    return absl::nullopt;

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
    return absl::nullopt;
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
    return absl::nullopt;

  return FromPSID(capability_sids[0]);
}

absl::optional<Sid> Sid::FromKnownSid(WellKnownSid type) {
  if (type == WellKnownSid::kAllRestrictedApplicationPackages) {
    SID_IDENTIFIER_AUTHORITY package_authority = {
        SECURITY_APP_PACKAGE_AUTHORITY};
    DWORD sub_authorities[] = {SECURITY_APP_PACKAGE_BASE_RID,
                               SECURITY_BUILTIN_PACKAGE_ANY_RESTRICTED_PACKAGE};
    return FromSubAuthorities(&package_authority, 2, sub_authorities);
  }

  BYTE sid[SECURITY_MAX_SID_SIZE];
  DWORD size_sid = SECURITY_MAX_SID_SIZE;
  absl::optional<WELL_KNOWN_SID_TYPE> known_sid = WellKnownSidToEnum(type);
  if (!known_sid)
    return absl::nullopt;
  if (!::CreateWellKnownSid(*known_sid, nullptr, sid, &size_sid))
    return absl::nullopt;

  return Sid(sid, size_sid);
}

absl::optional<Sid> Sid::FromSddlString(const wchar_t* sddl_sid) {
  PSID psid = nullptr;
  if (!::ConvertStringSidToSid(sddl_sid, &psid))
    return absl::nullopt;
  return FromPSID(TakeLocalAlloc(psid).get());
}

absl::optional<Sid> Sid::FromPSID(PSID sid) {
  DCHECK(sid);
  if (!sid || !::IsValidSid(sid))
    return absl::nullopt;
  return Sid(sid, ::GetLengthSid(sid));
}

absl::optional<Sid> Sid::GenerateRandomSid() {
  SID_IDENTIFIER_AUTHORITY package_authority = {SECURITY_NULL_SID_AUTHORITY};
  DWORD sub_authorities[4] = {};
  RandBytes(&sub_authorities, sizeof(sub_authorities));
  return FromSubAuthorities(&package_authority, _countof(sub_authorities),
                            sub_authorities);
}

absl::optional<Sid> Sid::CurrentUser() {
  // Get the current token.
  HANDLE token = nullptr;
  if (!::OpenProcessToken(::GetCurrentProcess(), TOKEN_QUERY, &token))
    return absl::nullopt;
  ScopedHandle token_scoped(token);

  char user_buffer[sizeof(TOKEN_USER) + SECURITY_MAX_SID_SIZE];
  DWORD size = sizeof(user_buffer);

  if (!::GetTokenInformation(token, TokenUser, user_buffer, size, &size))
    return absl::nullopt;

  TOKEN_USER* user = reinterpret_cast<TOKEN_USER*>(user_buffer);
  if (!user->User.Sid)
    return absl::nullopt;
  return Sid::FromPSID(user->User.Sid);
}

absl::optional<Sid> Sid::FromIntegrityLevel(DWORD integrity_level) {
  SID_IDENTIFIER_AUTHORITY package_authority = {
      SECURITY_MANDATORY_LABEL_AUTHORITY};
  return FromSubAuthorities(&package_authority, 1, &integrity_level);
}

absl::optional<std::vector<Sid>> Sid::FromSddlStringVector(
    const std::vector<const wchar_t*>& sddl_sids) {
  return FromStringVector(sddl_sids, Sid::FromSddlString);
}

absl::optional<std::vector<Sid>> Sid::FromNamedCapabilityVector(
    const std::vector<const wchar_t*>& capability_names) {
  return FromStringVector(capability_names, Sid::FromNamedCapability);
}

Sid::Sid(Sid&& sid) = default;
Sid::~Sid() = default;

PSID Sid::GetPSID() const {
  return const_cast<char*>(sid_.data());
}

// Converts the SID to an SDDL format string.
absl::optional<std::wstring> Sid::ToSddlString() const {
  LPWSTR sid = nullptr;
  if (!::ConvertSidToStringSid(GetPSID(), &sid))
    return absl::nullopt;
  return TakeLocalAlloc(sid).get();
}

}  // namespace win
}  // namespace base
