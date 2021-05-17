// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains unit tests for the sid class.

#include "base/win/sid.h"

#include <algorithm>

#include <windows.h>

#include <sddl.h>

#include "base/win/atl.h"
#include "base/win/scoped_localalloc.h"
#include "base/win/win_util.h"
#include "base/win/windows_version.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace base {
namespace win {

namespace {

bool EqualSid(const absl::optional<Sid>& sid, const ATL::CSid& compare_sid) {
  if (!sid)
    return false;
  return !!::EqualSid(sid->GetPSID(), const_cast<SID*>(compare_sid.GetPSID()));
}

bool EqualSid(const Sid& sid, const wchar_t* sddl_sid) {
  PSID compare_sid;
  if (!::ConvertStringSidToSid(sddl_sid, &compare_sid))
    return false;
  auto sid_ptr = TakeLocalAlloc(compare_sid);
  return !!::EqualSid(sid.GetPSID(), sid_ptr.get());
}

bool EqualSid(const absl::optional<Sid>& sid, const wchar_t* sddl_sid) {
  if (!sid)
    return false;
  return EqualSid(sid.value(), sddl_sid);
}

bool EqualSid(const absl::optional<Sid>& sid,
              const absl::optional<Sid>& compare_sid) {
  if (!sid || !compare_sid)
    return false;
  return !!::EqualSid(sid->GetPSID(), compare_sid->GetPSID());
}

bool EqualSid(const absl::optional<Sid>& sid, WELL_KNOWN_SID_TYPE known_sid) {
  if (!sid)
    return false;
  char known_sid_buffer[SECURITY_MAX_SID_SIZE] = {};
  DWORD size = SECURITY_MAX_SID_SIZE;
  if (!::CreateWellKnownSid(known_sid, nullptr, known_sid_buffer, &size))
    return false;

  return !!::EqualSid(sid->GetPSID(), known_sid_buffer);
}

bool TestSidVector(absl::optional<std::vector<Sid>> sids,
                   const std::vector<const wchar_t*> sddl) {
  if (!sids)
    return false;
  if (sids->size() != sddl.size())
    return false;
  return std::equal(
      sids->begin(), sids->end(), sddl.begin(),
      [](const Sid& sid, const wchar_t* sddl) { return EqualSid(sid, sddl); });
}

bool TestFromSddlStringVector(const std::vector<const wchar_t*> sddl) {
  return TestSidVector(Sid::FromSddlStringVector(sddl), sddl);
}

struct KnownCapabilityTestEntry {
  WellKnownCapability capability;
  const wchar_t* sddl_sid;
};

struct NamedCapabilityTestEntry {
  const wchar_t* capability_name;
  const wchar_t* sddl_sid;
};

struct KnownSidTestEntry {
  WellKnownSid sid;
  WELL_KNOWN_SID_TYPE well_known_sid;
};

}  // namespace

// Tests the creation of a Sid.
TEST(SidTest, Initializers) {
  ATL::CSid sid_world = ATL::Sids::World();
  PSID sid_world_pointer = const_cast<SID*>(sid_world.GetPSID());

  // Check the PSID constructor.
  absl::optional<Sid> sid_sid_star = Sid::FromPSID(sid_world_pointer);
  ASSERT_TRUE(EqualSid(sid_sid_star, sid_world));

  char invalid_sid[16] = {};
  ASSERT_FALSE(Sid::FromPSID(invalid_sid));

  absl::optional<Sid> sid_sddl = Sid::FromSddlString(L"S-1-1-0");
  ASSERT_TRUE(sid_sddl);
  ASSERT_TRUE(EqualSid(sid_sddl, sid_world));
}

TEST(SidTest, KnownCapability) {
  if (GetVersion() < Version::WIN8)
    return;

  const KnownCapabilityTestEntry capabilities[] = {
      {WellKnownCapability::kInternetClient, L"S-1-15-3-1"},
      {WellKnownCapability::kInternetClientServer, L"S-1-15-3-2"},
      {WellKnownCapability::kPrivateNetworkClientServer, L"S-1-15-3-3"},
      {WellKnownCapability::kPicturesLibrary, L"S-1-15-3-4"},
      {WellKnownCapability::kVideosLibrary, L"S-1-15-3-5"},
      {WellKnownCapability::kMusicLibrary, L"S-1-15-3-6"},
      {WellKnownCapability::kDocumentsLibrary, L"S-1-15-3-7"},
      {WellKnownCapability::kEnterpriseAuthentication, L"S-1-15-3-8"},
      {WellKnownCapability::kSharedUserCertificates, L"S-1-15-3-9"},
      {WellKnownCapability::kRemovableStorage, L"S-1-15-3-10"},
      {WellKnownCapability::kAppointments, L"S-1-15-3-11"},
      {WellKnownCapability::kContacts, L"S-1-15-3-12"},
  };

  for (auto capability : capabilities) {
    EXPECT_TRUE(EqualSid(Sid::FromKnownCapability(capability.capability),
                         capability.sddl_sid))
        << "Known Capability: " << capability.sddl_sid;
  }
}

TEST(SidTest, NamedCapability) {
  if (GetVersion() < Version::WIN10)
    return;

  EXPECT_FALSE(Sid::FromNamedCapability(nullptr));
  EXPECT_FALSE(Sid::FromNamedCapability(L""));

  const NamedCapabilityTestEntry capabilities[] = {
      {L"internetClient", L"S-1-15-3-1"},
      {L"internetClientServer", L"S-1-15-3-2"},
      {L"registryRead",
       L"S-1-15-3-1024-1065365936-1281604716-3511738428-"
       "1654721687-432734479-3232135806-4053264122-3456934681"},
      {L"lpacCryptoServices",
       L"S-1-15-3-1024-3203351429-2120443784-2872670797-"
       "1918958302-2829055647-4275794519-765664414-2751773334"},
      {L"enterpriseAuthentication", L"S-1-15-3-8"},
      {L"privateNetworkClientServer", L"S-1-15-3-3"}};

  for (auto capability : capabilities) {
    EXPECT_TRUE(EqualSid(Sid::FromNamedCapability(capability.capability_name),
                         capability.sddl_sid))
        << "Named Capability: " << capability.sddl_sid;
  }
}

TEST(SidTest, KnownSids) {
  const KnownSidTestEntry known_sids[] = {
      {WellKnownSid::kNull, ::WinNullSid},
      {WellKnownSid::kWorld, ::WinWorldSid},
      {WellKnownSid::kCreatorOwner, ::WinCreatorOwnerSid},
      {WellKnownSid::kNetwork, ::WinNetworkSid},
      {WellKnownSid::kBatch, ::WinBatchSid},
      {WellKnownSid::kInteractive, ::WinInteractiveSid},
      {WellKnownSid::kService, ::WinServiceSid},
      {WellKnownSid::kAnonymous, ::WinAnonymousSid},
      {WellKnownSid::kSelf, ::WinSelfSid},
      {WellKnownSid::kAuthenticatedUser, ::WinAuthenticatedUserSid},
      {WellKnownSid::kRestricted, ::WinRestrictedCodeSid},
      {WellKnownSid::kLocalSystem, ::WinLocalSystemSid},
      {WellKnownSid::kLocalService, ::WinLocalServiceSid},
      {WellKnownSid::kNetworkService, ::WinNetworkServiceSid},
      {WellKnownSid::kBuiltinAdministrators, ::WinBuiltinAdministratorsSid},
      {WellKnownSid::kBuiltinUsers, ::WinBuiltinUsersSid},
      {WellKnownSid::kBuiltinGuests, ::WinBuiltinGuestsSid},
      {WellKnownSid::kUntrustedLabel, ::WinUntrustedLabelSid},
      {WellKnownSid::kLowLabel, ::WinLowLabelSid},
      {WellKnownSid::kMediumLabel, ::WinMediumLabelSid},
      {WellKnownSid::kHighLabel, ::WinHighLabelSid},
      {WellKnownSid::kSystemLabel, ::WinSystemLabelSid},
      {WellKnownSid::kWriteRestricted, ::WinWriteRestrictedCodeSid},
      {WellKnownSid::kCreatorOwnerRights, ::WinCreatorOwnerRightsSid}};

  for (auto known_sid : known_sids) {
    EXPECT_TRUE(
        EqualSid(Sid::FromKnownSid(known_sid.sid), known_sid.well_known_sid))
        << "Known Sid: " << static_cast<int>(known_sid.sid);
  }

  if (GetVersion() < Version::WIN8)
    return;

  EXPECT_TRUE(EqualSid(Sid::FromKnownSid(WellKnownSid::kAllApplicationPackages),
                       ::WinBuiltinAnyPackageSid));

  if (GetVersion() < Version::WIN10)
    return;

  EXPECT_TRUE(EqualSid(
      Sid::FromKnownSid(WellKnownSid::kAllRestrictedApplicationPackages),
      L"S-1-15-2-2"));
}

TEST(SidTest, SddlString) {
  absl::optional<Sid> sid_sddl = Sid::FromSddlString(L"S-1-1-0");
  ASSERT_TRUE(sid_sddl);
  absl::optional<std::wstring> sddl_str = sid_sddl->ToSddlString();
  ASSERT_TRUE(sddl_str);
  ASSERT_EQ(L"S-1-1-0", *sddl_str);
  ASSERT_FALSE(Sid::FromSddlString(L"X-1-1-0"));
  ASSERT_FALSE(Sid::FromSddlString(L""));
}

TEST(SidTest, RandomSid) {
  absl::optional<Sid> sid1 = Sid::GenerateRandomSid();
  ASSERT_TRUE(sid1);
  absl::optional<Sid> sid2 = Sid::GenerateRandomSid();
  ASSERT_TRUE(sid2);
  ASSERT_FALSE(EqualSid(sid1, sid2));
}

TEST(SidTest, CurrentUser) {
  absl::optional<Sid> sid1 = Sid::CurrentUser();
  ASSERT_TRUE(sid1);
  std::wstring user_sid;
  ASSERT_TRUE(GetUserSidString(&user_sid));
  ASSERT_TRUE(EqualSid(sid1, user_sid.c_str()));
}

TEST(SidTest, FromIntegrityLevel) {
  ASSERT_TRUE(EqualSid(
      Sid::FromIntegrityLevel(SECURITY_MANDATORY_UNTRUSTED_RID), L"S-1-16-0"));
  ASSERT_TRUE(EqualSid(Sid::FromIntegrityLevel(SECURITY_MANDATORY_LOW_RID),
                       L"S-1-16-4096"));
  ASSERT_TRUE(EqualSid(Sid::FromIntegrityLevel(SECURITY_MANDATORY_MEDIUM_RID),
                       L"S-1-16-8192"));
  ASSERT_TRUE(
      EqualSid(Sid::FromIntegrityLevel(SECURITY_MANDATORY_MEDIUM_PLUS_RID),
               L"S-1-16-8448"));
  ASSERT_TRUE(EqualSid(Sid::FromIntegrityLevel(SECURITY_MANDATORY_HIGH_RID),
                       L"S-1-16-12288"));
  ASSERT_TRUE(EqualSid(Sid::FromIntegrityLevel(SECURITY_MANDATORY_SYSTEM_RID),
                       L"S-1-16-16384"));
  ASSERT_TRUE(EqualSid(Sid::FromIntegrityLevel(1234), L"S-1-16-1234"));
}

TEST(SidTest, FromSddlStringVector) {
  ASSERT_TRUE(
      TestFromSddlStringVector({L"S-1-1-0", L"S-1-15-2-2", L"S-1-15-3-2"}));
  ASSERT_FALSE(
      TestFromSddlStringVector({L"S-1-1-0", L"X-1-15-2-2", L"S-1-15-3-2"}));
  ASSERT_FALSE(TestFromSddlStringVector({L""}));
  ASSERT_FALSE(TestFromSddlStringVector({nullptr}));
  ASSERT_FALSE(TestFromSddlStringVector({L"S-1-1-0", nullptr}));
  ASSERT_TRUE(TestFromSddlStringVector({}));
}

TEST(SidTest, FromNamedCapabilityVector) {
  if (GetVersion() < Version::WIN10)
    return;
  std::vector<const wchar_t*> capabilities = {L"internetClient",
                                              L"internetClientServer",
                                              L"registryRead",
                                              L"lpacCryptoServices",
                                              L"enterpriseAuthentication",
                                              L"privateNetworkClientServer"};
  std::vector<const wchar_t*> sddl_caps = {
      L"S-1-15-3-1",
      L"S-1-15-3-2",
      L"S-1-15-3-1024-1065365936-1281604716-3511738428-1654721687-432734479-"
      L"3232135806-4053264122-3456934681",
      L"S-1-15-3-1024-3203351429-2120443784-2872670797-1918958302-2829055647-"
      L"4275794519-765664414-2751773334",
      L"S-1-15-3-8",
      L"S-1-15-3-3"};
  ASSERT_TRUE(
      TestSidVector(Sid::FromNamedCapabilityVector(capabilities), sddl_caps));
  ASSERT_FALSE(Sid::FromNamedCapabilityVector({L""}));
  ASSERT_FALSE(Sid::FromNamedCapabilityVector({L"abc", nullptr}));
  ASSERT_TRUE(Sid::FromNamedCapabilityVector({}));
}

}  // namespace win
}  // namespace base
