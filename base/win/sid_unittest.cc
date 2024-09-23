// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

// This file contains unit tests for the sid class.

#include "base/win/sid.h"

#include <windows.h>

#include <sddl.h>

#include <optional>

#include "base/ranges/algorithm.h"
#include "base/win/atl.h"
#include "base/win/scoped_handle.h"
#include "base/win/scoped_localalloc.h"
#include "base/win/win_util.h"
#include "build/branding_buildflags.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base::win {

namespace {

bool EqualSid(const std::optional<Sid>& sid, const ATL::CSid& compare_sid) {
  if (!sid)
    return false;
  return sid->Equal(const_cast<SID*>(compare_sid.GetPSID()));
}

bool EqualSid(const Sid& sid, const std::wstring& sddl_sid) {
  PSID compare_sid;
  if (!::ConvertStringSidToSid(sddl_sid.c_str(), &compare_sid)) {
    return false;
  }
  auto sid_ptr = TakeLocalAlloc(compare_sid);
  return sid.Equal(sid_ptr.get());
}

bool EqualSid(const std::optional<Sid>& sid, WELL_KNOWN_SID_TYPE known_sid) {
  if (!sid)
    return false;
  char known_sid_buffer[SECURITY_MAX_SID_SIZE] = {};
  DWORD size = SECURITY_MAX_SID_SIZE;
  if (!::CreateWellKnownSid(known_sid, nullptr, known_sid_buffer, &size))
    return false;

  return sid->Equal(known_sid_buffer);
}

bool TestSidVector(std::optional<std::vector<Sid>> sids,
                   const std::vector<std::wstring>& sddl) {
  return sids && ranges::equal(*sids, sddl,
                               [](const Sid& sid, const std::wstring& sddl) {
                                 return EqualSid(sid, sddl);
                               });
}

bool TestFromSddlStringVector(const std::vector<std::wstring> sddl) {
  return TestSidVector(Sid::FromSddlStringVector(sddl), sddl);
}

typedef decltype(::DeriveCapabilitySidsFromName)*
    DeriveCapabilitySidsFromNameFunc;

// Get the DeriveCapabilitySidsFromName API dynamically. Versions of Windows 10
// older than 1809 do not implement this method. By loading dynamically we can
// skip tests when running on these older versions. Online documentation for
// this API claims it's supported back to Windows 2003, however this is entirely
// incorrect.
DeriveCapabilitySidsFromNameFunc GetDeriveCapabilitySidsFromName() {
  static const DeriveCapabilitySidsFromNameFunc derive_capability_sids =
      []() -> DeriveCapabilitySidsFromNameFunc {
    HMODULE module = GetModuleHandle(L"api-ms-win-security-base-l1-2-2.dll");
    if (!module) {
      return nullptr;
    }
    return reinterpret_cast<DeriveCapabilitySidsFromNameFunc>(
        ::GetProcAddress(module, "DeriveCapabilitySidsFromName"));
  }();

  return derive_capability_sids;
}

bool EqualNamedCapSid(const Sid& sid, const std::wstring& capability_name) {
  DeriveCapabilitySidsFromNameFunc derive_capability_sids =
      GetDeriveCapabilitySidsFromName();
  CHECK(derive_capability_sids);

  // Pre-reserve some space for SID deleters.
  std::vector<base::win::ScopedLocalAlloc> deleter_list;
  deleter_list.reserve(16);

  PSID* capability_groups = nullptr;
  DWORD capability_group_count = 0;
  PSID* capability_sids = nullptr;
  DWORD capability_sid_count = 0;

  CHECK(derive_capability_sids(capability_name.c_str(), &capability_groups,
                               &capability_group_count, &capability_sids,
                               &capability_sid_count));
  deleter_list.emplace_back(capability_groups);
  deleter_list.emplace_back(capability_sids);

  for (DWORD i = 0; i < capability_group_count; ++i) {
    deleter_list.emplace_back(capability_groups[i]);
  }
  for (DWORD i = 0; i < capability_sid_count; ++i) {
    deleter_list.emplace_back(capability_sids[i]);
  }

  CHECK_GE(capability_sid_count, 1U);
  return sid.Equal(capability_sids[0]);
}

struct KnownCapabilityTestEntry {
  WellKnownCapability capability;
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
  std::optional<Sid> sid_sid_star = Sid::FromPSID(sid_world_pointer);
  ASSERT_TRUE(EqualSid(sid_sid_star, sid_world));

  char invalid_sid[16] = {};
  ASSERT_FALSE(Sid::FromPSID(invalid_sid));

  std::optional<Sid> sid_sddl = Sid::FromSddlString(L"S-1-1-0");
  ASSERT_TRUE(sid_sddl);
  ASSERT_TRUE(EqualSid(sid_sddl, sid_world));
}

TEST(SidTest, KnownCapability) {
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
    EXPECT_TRUE(EqualSid(Sid(capability.capability), capability.sddl_sid))
        << "Known Capability: " << capability.sddl_sid;
  }
}

TEST(SidTest, NamedCapability) {
  if (!GetDeriveCapabilitySidsFromName()) {
    GTEST_SKIP()
        << "Platform doesn't support DeriveCapabilitySidsFromName function.";
  }
  const std::wstring capabilities[] = {L"",
                                       L"InternetClient",
                                       L"InternetClientServer",
                                       L"PrivateNetworkClientServer",
                                       L"PicturesLibrary",
                                       L"VideosLibrary",
                                       L"MusicLibrary",
                                       L"DocumentsLibrary",
                                       L"EnterpriseAuthentication",
                                       L"SharedUserCertificates",
                                       L"RemovableStorage",
                                       L"Appointments",
                                       L"Contacts",
                                       L"registryRead",
                                       L"lpacCryptoServices"};

  for (const std::wstring& capability : capabilities) {
    EXPECT_TRUE(
        EqualNamedCapSid(Sid::FromNamedCapability(capability), capability))
        << "Named Capability: " << capability;
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
      {WellKnownSid::kCreatorOwnerRights, ::WinCreatorOwnerRightsSid},
      {WellKnownSid::kAllApplicationPackages, ::WinBuiltinAnyPackageSid}};

  for (auto known_sid : known_sids) {
    EXPECT_TRUE(
        EqualSid(Sid::FromKnownSid(known_sid.sid), known_sid.well_known_sid))
        << "Known Sid: " << static_cast<int>(known_sid.sid);
    EXPECT_TRUE(EqualSid(Sid(known_sid.sid), known_sid.well_known_sid))
        << "Known Sid: " << static_cast<int>(known_sid.sid);
  }

  EXPECT_TRUE(EqualSid(
      Sid::FromKnownSid(WellKnownSid::kAllRestrictedApplicationPackages),
      L"S-1-15-2-2"));
}

TEST(SidTest, SddlString) {
  std::optional<Sid> sid_sddl = Sid::FromSddlString(L"S-1-1-0");
  ASSERT_TRUE(sid_sddl);
  std::optional<std::wstring> sddl_str = sid_sddl->ToSddlString();
  ASSERT_TRUE(sddl_str);
  ASSERT_EQ(L"S-1-1-0", *sddl_str);
  ASSERT_FALSE(Sid::FromSddlString(L"X-1-1-0"));
  ASSERT_FALSE(Sid::FromSddlString(L""));
}

TEST(SidTest, RandomSid) {
  Sid sid1 = Sid::GenerateRandomSid();
  Sid sid2 = Sid::GenerateRandomSid();
  EXPECT_NE(sid1, sid2);
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
  ASSERT_TRUE(TestFromSddlStringVector({}));
}

TEST(SidTest, FromNamedCapabilityVector) {
  if (!GetDeriveCapabilitySidsFromName()) {
    GTEST_SKIP()
        << "Platform doesn't support DeriveCapabilitySidsFromName function.";
  }
  std::vector<std::wstring> capabilities = {L"",
                                            L"InternetClient",
                                            L"InternetClientServer",
                                            L"PrivateNetworkClientServer",
                                            L"PicturesLibrary",
                                            L"VideosLibrary",
                                            L"MusicLibrary",
                                            L"DocumentsLibrary",
                                            L"EnterpriseAuthentication",
                                            L"SharedUserCertificates",
                                            L"RemovableStorage",
                                            L"Appointments",
                                            L"Contacts",
                                            L"registryRead",
                                            L"lpacCryptoServices"};

  ASSERT_TRUE(ranges::equal(Sid::FromNamedCapabilityVector(capabilities),
                            capabilities, EqualNamedCapSid));
  EXPECT_EQ(Sid::FromNamedCapabilityVector({}).size(), 0U);
}

TEST(SidTest, FromKnownCapabilityVector) {
  ASSERT_TRUE(TestSidVector(
      Sid::FromKnownCapabilityVector(
          {WellKnownCapability::kInternetClient,
           WellKnownCapability::kInternetClientServer,
           WellKnownCapability::kPrivateNetworkClientServer,
           WellKnownCapability::kPicturesLibrary,
           WellKnownCapability::kVideosLibrary,
           WellKnownCapability::kMusicLibrary,
           WellKnownCapability::kDocumentsLibrary,
           WellKnownCapability::kEnterpriseAuthentication,
           WellKnownCapability::kSharedUserCertificates,
           WellKnownCapability::kRemovableStorage,
           WellKnownCapability::kAppointments, WellKnownCapability::kContacts}),
      {L"S-1-15-3-1", L"S-1-15-3-2", L"S-1-15-3-3", L"S-1-15-3-4",
       L"S-1-15-3-5", L"S-1-15-3-6", L"S-1-15-3-7", L"S-1-15-3-8",
       L"S-1-15-3-9", L"S-1-15-3-10", L"S-1-15-3-11", L"S-1-15-3-12"}));

  ASSERT_FALSE(TestSidVector(
      Sid::FromKnownCapabilityVector({WellKnownCapability::kInternetClient}),
      {L"S-1-1-0"}));
}

TEST(SidTest, FromKnownSidVector) {
  ASSERT_TRUE(TestSidVector(
      Sid::FromKnownSidVector({WellKnownSid::kNull, WellKnownSid::kWorld}),
      {L"S-1-0-0", L"S-1-1-0"}));

  ASSERT_FALSE(TestSidVector(Sid::FromKnownSidVector({WellKnownSid::kNull}),
                             {L"S-1-1-0"}));
}

TEST(SidTest, Equal) {
  Sid world_sid = Sid::FromKnownSid(WellKnownSid::kWorld);
  EXPECT_EQ(world_sid, world_sid);
  auto world_sid_sddl = Sid::FromSddlString(L"S-1-1-0");
  ASSERT_TRUE(world_sid_sddl);
  EXPECT_EQ(world_sid, world_sid_sddl);
  EXPECT_EQ(world_sid_sddl, world_sid);
  EXPECT_TRUE(world_sid.Equal(world_sid_sddl->GetPSID()));
  EXPECT_TRUE(world_sid_sddl->Equal(world_sid.GetPSID()));
  Sid null_sid = Sid::FromKnownSid(WellKnownSid::kNull);
  EXPECT_NE(world_sid, null_sid);
  EXPECT_NE(null_sid, world_sid);
  EXPECT_FALSE(world_sid.Equal(null_sid.GetPSID()));
  EXPECT_FALSE(null_sid.Equal(world_sid.GetPSID()));
}

TEST(SidTest, Clone) {
  Sid world_sid = Sid::FromKnownSid(WellKnownSid::kWorld);
  auto world_sid_clone = world_sid.Clone();
  EXPECT_NE(world_sid.GetPSID(), world_sid_clone.GetPSID());
  EXPECT_EQ(world_sid, world_sid_clone);
}

}  // namespace base::win
