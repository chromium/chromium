// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "base/win/sid.h"

// clang-format off
#include <windows.h>  // Must be in front of other Windows header files.
// clang-format on

#include <sddl.h>
#include <stdint.h>
#include <stdlib.h>

#include <iterator>
#include <map>
#include <utility>

#include "base/check.h"
#include "base/no_destructor.h"
#include "base/rand_util.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_util_win.h"
#include "base/win/scoped_handle.h"
#include "base/win/scoped_localalloc.h"
#include "base/win/windows_version.h"
#include "third_party/boringssl/src/include/openssl/sha.h"

namespace base::win {

namespace {

template <typename Iterator>
Sid FromSubAuthorities(const SID_IDENTIFIER_AUTHORITY& identifier_authority,
                       size_t sub_authority_count,
                       Iterator sub_authorities) {
  DCHECK(sub_authority_count <= SID_MAX_SUB_AUTHORITIES);
  BYTE sid_buffer[SECURITY_MAX_SID_SIZE];
  SID* sid = reinterpret_cast<SID*>(sid_buffer);
  sid->Revision = SID_REVISION;
  sid->SubAuthorityCount = static_cast<UCHAR>(sub_authority_count);
  sid->IdentifierAuthority = identifier_authority;
  for (size_t index = 0; index < sub_authority_count; ++index) {
    sid->SubAuthority[index] = static_cast<DWORD>(*sub_authorities++);
  }
  DCHECK(::IsValidSid(sid));
  return *Sid::FromPSID(sid);
}

Sid FromSubAuthorities(const SID_IDENTIFIER_AUTHORITY& identifier_authority,
                       std::initializer_list<int32_t> sub_authorities) {
  return FromSubAuthorities(identifier_authority, sub_authorities.size(),
                            sub_authorities.begin());
}

Sid FromNtAuthority(std::initializer_list<int32_t> sub_authorities) {
  return FromSubAuthorities(SECURITY_NT_AUTHORITY, sub_authorities);
}

int32_t WellKnownCapabilityToRid(WellKnownCapability capability) {
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
}

}  // namespace

Sid::Sid(const void* sid, size_t length)
    : sid_(static_cast<const char*>(sid),
           static_cast<const char*>(sid) + length) {
  DCHECK(::IsValidSid(GetPSID()));
}

Sid Sid::FromKnownCapability(WellKnownCapability capability) {
  int32_t capability_rid = WellKnownCapabilityToRid(capability);
  return FromSubAuthorities(SECURITY_APP_PACKAGE_AUTHORITY,
                            {SECURITY_CAPABILITY_BASE_RID, capability_rid});
}

Sid Sid::FromNamedCapability(const std::wstring& capability_name) {
  static const base::NoDestructor<std::map<std::wstring, WellKnownCapability>>
      known_capabilities(
          {{L"INTERNETCLIENT", WellKnownCapability::kInternetClient},
           {L"INTERNETCLIENTSERVER",
            WellKnownCapability::kInternetClientServer},
           {L"PRIVATENETWORKCLIENTSERVER",
            WellKnownCapability::kPrivateNetworkClientServer},
           {L"PICTURESLIBRARY", WellKnownCapability::kPicturesLibrary},
           {L"VIDEOSLIBRARY", WellKnownCapability::kVideosLibrary},
           {L"MUSICLIBRARY", WellKnownCapability::kMusicLibrary},
           {L"DOCUMENTSLIBRARY", WellKnownCapability::kDocumentsLibrary},
           {L"ENTERPRISEAUTHENTICATION",
            WellKnownCapability::kEnterpriseAuthentication},
           {L"SHAREDUSERCERTIFICATES",
            WellKnownCapability::kSharedUserCertificates},
           {L"REMOVABLESTORAGE", WellKnownCapability::kRemovableStorage},
           {L"APPOINTMENTS", WellKnownCapability::kAppointments},
           {L"CONTACTS", WellKnownCapability::kContacts}});

  std::wstring cap_upper = base::ToUpperASCII(capability_name);
  auto known_cap = known_capabilities->find(cap_upper);
  if (known_cap != known_capabilities->end()) {
    return FromKnownCapability(known_cap->second);
  }
  static_assert((SHA256_DIGEST_LENGTH / sizeof(DWORD)) ==
                SECURITY_APP_PACKAGE_RID_COUNT);
  DWORD rids[(SHA256_DIGEST_LENGTH / sizeof(DWORD)) + 2];
  rids[0] = SECURITY_CAPABILITY_BASE_RID;
  rids[1] = SECURITY_CAPABILITY_APP_RID;

  SHA256(reinterpret_cast<const uint8_t*>(cap_upper.c_str()),
         cap_upper.size() * sizeof(wchar_t),
         reinterpret_cast<uint8_t*>(&rids[2]));
  return FromSubAuthorities(SECURITY_APP_PACKAGE_AUTHORITY, std::size(rids),
                            rids);
}

Sid Sid::FromKnownSid(WellKnownSid type) {
  switch (type) {
    case WellKnownSid::kNull:
      return FromSubAuthorities(SECURITY_NULL_SID_AUTHORITY,
                                {SECURITY_NULL_RID});
    case WellKnownSid::kWorld:
      return FromSubAuthorities(SECURITY_WORLD_SID_AUTHORITY,
                                {SECURITY_WORLD_RID});
    case WellKnownSid::kCreatorOwner:
      return FromSubAuthorities(SECURITY_CREATOR_SID_AUTHORITY,
                                {SECURITY_CREATOR_OWNER_RID});
    case WellKnownSid::kCreatorOwnerRights:
      return FromSubAuthorities(SECURITY_CREATOR_SID_AUTHORITY,
                                {SECURITY_CREATOR_OWNER_RIGHTS_RID});
    case WellKnownSid::kNetwork:
      return FromNtAuthority({SECURITY_NETWORK_RID});
    case WellKnownSid::kBatch:
      return FromNtAuthority({SECURITY_BATCH_RID});
    case WellKnownSid::kInteractive:
      return FromNtAuthority({SECURITY_INTERACTIVE_RID});
    case WellKnownSid::kService:
      return FromNtAuthority({SECURITY_SERVICE_RID});
    case WellKnownSid::kAnonymous:
      return FromNtAuthority({SECURITY_ANONYMOUS_LOGON_RID});
    case WellKnownSid::kSelf:
      return FromNtAuthority({SECURITY_PRINCIPAL_SELF_RID});
    case WellKnownSid::kAuthenticatedUser:
      return FromNtAuthority({SECURITY_AUTHENTICATED_USER_RID});
    case WellKnownSid::kRestricted:
      return FromNtAuthority({SECURITY_RESTRICTED_CODE_RID});
    case WellKnownSid::kWriteRestricted:
      return FromNtAuthority({SECURITY_WRITE_RESTRICTED_CODE_RID});
    case WellKnownSid::kLocalSystem:
      return FromNtAuthority({SECURITY_LOCAL_SYSTEM_RID});
    case WellKnownSid::kLocalService:
      return FromNtAuthority({SECURITY_LOCAL_SERVICE_RID});
    case WellKnownSid::kNetworkService:
      return FromNtAuthority({SECURITY_NETWORK_SERVICE_RID});
    case WellKnownSid::kBuiltinAdministrators:
      return FromNtAuthority(
          {SECURITY_BUILTIN_DOMAIN_RID, DOMAIN_ALIAS_RID_ADMINS});
    case WellKnownSid::kBuiltinUsers:
      return FromNtAuthority(
          {SECURITY_BUILTIN_DOMAIN_RID, DOMAIN_ALIAS_RID_USERS});
    case WellKnownSid::kBuiltinGuests:
      return FromNtAuthority(
          {SECURITY_BUILTIN_DOMAIN_RID, DOMAIN_ALIAS_RID_GUESTS});
    case WellKnownSid::kUntrustedLabel:
      return FromIntegrityLevel(SECURITY_MANDATORY_UNTRUSTED_RID);
    case WellKnownSid::kLowLabel:
      return FromIntegrityLevel(SECURITY_MANDATORY_LOW_RID);
    case WellKnownSid::kMediumLabel:
      return FromIntegrityLevel(SECURITY_MANDATORY_MEDIUM_RID);
    case WellKnownSid::kHighLabel:
      return FromIntegrityLevel(SECURITY_MANDATORY_HIGH_RID);
    case WellKnownSid::kSystemLabel:
      return FromIntegrityLevel(SECURITY_MANDATORY_SYSTEM_RID);
    case WellKnownSid::kAllApplicationPackages:
      return FromSubAuthorities(SECURITY_APP_PACKAGE_AUTHORITY,
                                {SECURITY_APP_PACKAGE_BASE_RID,
                                 SECURITY_BUILTIN_PACKAGE_ANY_PACKAGE});
    case WellKnownSid::kAllRestrictedApplicationPackages:
      return FromSubAuthorities(
          SECURITY_APP_PACKAGE_AUTHORITY,
          {SECURITY_APP_PACKAGE_BASE_RID,
           SECURITY_BUILTIN_PACKAGE_ANY_RESTRICTED_PACKAGE});
  }
}

std::optional<Sid> Sid::FromSddlString(const std::wstring& sddl_sid) {
  PSID psid = nullptr;
  if (!::ConvertStringSidToSid(sddl_sid.c_str(), &psid)) {
    return std::nullopt;
  }
  auto psid_alloc = TakeLocalAlloc(psid);
  return FromPSID(psid_alloc.get());
}

std::optional<Sid> Sid::FromPSID(PSID sid) {
  DCHECK(sid);
  if (!sid || !::IsValidSid(sid))
    return std::nullopt;
  return Sid(sid, ::GetLengthSid(sid));
}

Sid Sid::GenerateRandomSid() {
  DWORD sub_authorities[4] = {};
  RandBytes(as_writable_byte_span(sub_authorities));
  return FromSubAuthorities(SECURITY_NULL_SID_AUTHORITY,
                            std::size(sub_authorities), sub_authorities);
}

Sid Sid::FromIntegrityLevel(DWORD integrity_level) {
  return FromSubAuthorities(SECURITY_MANDATORY_LABEL_AUTHORITY, 1,
                            &integrity_level);
}

std::optional<std::vector<Sid>> Sid::FromSddlStringVector(
    const std::vector<std::wstring>& sddl_sids) {
  std::vector<Sid> converted_sids;
  converted_sids.reserve(sddl_sids.size());
  for (const std::wstring& sddl_sid : sddl_sids) {
    std::optional<Sid> sid = FromSddlString(sddl_sid);
    if (!sid)
      return std::nullopt;
    converted_sids.push_back(std::move(*sid));
  }
  return converted_sids;
}

std::vector<Sid> Sid::FromNamedCapabilityVector(
    const std::vector<std::wstring>& capability_names) {
  std::vector<Sid> sids;
  ranges::transform(capability_names, std::back_inserter(sids),
                    FromNamedCapability);
  return sids;
}

std::vector<Sid> Sid::FromKnownCapabilityVector(
    const std::vector<WellKnownCapability>& capabilities) {
  std::vector<Sid> sids;
  ranges::transform(capabilities, std::back_inserter(sids),
                    FromKnownCapability);
  return sids;
}

std::vector<Sid> Sid::FromKnownSidVector(
    const std::vector<WellKnownSid>& known_sids) {
  std::vector<Sid> sids;
  ranges::transform(known_sids, std::back_inserter(sids), FromKnownSid);
  return sids;
}

Sid::Sid(WellKnownSid known_sid) : Sid(FromKnownSid(known_sid)) {}
Sid::Sid(WellKnownCapability known_capability)
    : Sid(FromKnownCapability(known_capability)) {}
Sid::Sid(Sid&& sid) = default;
Sid& Sid::operator=(Sid&&) = default;
Sid::~Sid() = default;

PSID Sid::GetPSID() const {
  DCHECK(!sid_.empty());
  return const_cast<char*>(sid_.data());
}

std::optional<std::wstring> Sid::ToSddlString() const {
  LPWSTR sid = nullptr;
  if (!::ConvertSidToStringSid(GetPSID(), &sid))
    return std::nullopt;
  auto sid_ptr = TakeLocalAlloc(sid);
  return sid_ptr.get();
}

Sid Sid::Clone() const {
  return Sid(sid_.data(), sid_.size());
}

bool Sid::Equal(PSID sid) const {
  return !!::EqualSid(GetPSID(), sid);
}

bool Sid::operator==(const Sid& sid) const {
  return Equal(sid.GetPSID());
}

bool Sid::operator!=(const Sid& sid) const {
  return !(operator==(sid));
}

}  // namespace base::win
