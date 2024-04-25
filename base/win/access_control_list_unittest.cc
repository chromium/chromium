// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "base/win/access_control_list.h"

// clang-format off
#include <windows.h>  // Must be in front of other Windows header files.
// clang-format on

#include <sddl.h>

#include <string>
#include <vector>

#include "base/check.h"
#include "base/strings/string_number_conversions_win.h"
#include "base/win/scoped_localalloc.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base::win {

namespace {

constexpr wchar_t kFromPACLTest[] = L"(D;;FX;;;SY)(A;;GA;;;WD)";
constexpr wchar_t kNullAcl[] = L"NO_ACCESS_CONTROL";
constexpr wchar_t kEvent[] = L"(A;;0x1f0003;;;WD)";
constexpr wchar_t kEventWithSystemInherit[] =
    L"(D;OICI;DC;;;SY)(A;;0x1f0003;;;WD)";
constexpr wchar_t kEventSystemOnlyInherit[] = L"(D;OICI;DC;;;SY)";
constexpr wchar_t kEventReadControl[] = L"(A;;RC;;;WD)";
constexpr wchar_t kEventReadControlModify[] = L"(A;;DCRC;;;WD)";
constexpr wchar_t kUntrustedLabel[] = L"(ML;;;;;S-1-16-0)";
constexpr wchar_t kSystemLabel[] = L"(ML;;;;;SI)";
constexpr wchar_t kSystemLabelInherit[] = L"(ML;OICI;;;;SI)";
constexpr wchar_t kSystemLabelPolicy[] = L"(ML;;NWNRNX;;;SI)";
constexpr wchar_t kSystemLabelInheritPolicy[] = L"(ML;OICI;NWNRNX;;;SI)";
constexpr wchar_t kDaclPrefix[] = L"D:";
constexpr wchar_t kSaclPrefix[] = L"S:";

std::vector<char> ConvertSddlToAcl(const wchar_t* sddl) {
  std::wstring sddl_dacl = kDaclPrefix;
  sddl_dacl += sddl;
  PSECURITY_DESCRIPTOR sd = nullptr;
  CHECK(::ConvertStringSecurityDescriptorToSecurityDescriptor(
      sddl_dacl.c_str(), SDDL_REVISION_1, &sd, nullptr));
  auto sd_ptr = TakeLocalAlloc(sd);
  CHECK(sd_ptr);
  BOOL present = FALSE;
  BOOL defaulted = FALSE;
  PACL dacl = nullptr;
  CHECK(::GetSecurityDescriptorDacl(sd_ptr.get(), &present, &dacl, &defaulted));
  CHECK(present);
  char* dacl_ptr = reinterpret_cast<char*>(dacl);
  return std::vector<char>(dacl_ptr, dacl_ptr + dacl->AclSize);
}

std::wstring ConvertAclToSddl(const AccessControlList& acl,
                              bool label = false) {
  SECURITY_DESCRIPTOR sd = {};
  CHECK(::InitializeSecurityDescriptor(&sd, SECURITY_DESCRIPTOR_REVISION));
  if (label) {
    CHECK(::SetSecurityDescriptorSacl(&sd, TRUE, acl.get(), FALSE));
  } else {
    CHECK(::SetSecurityDescriptorDacl(&sd, TRUE, acl.get(), FALSE));
  }
  LPWSTR sddl_str = nullptr;
  CHECK(::ConvertSecurityDescriptorToStringSecurityDescriptor(
      &sd, SDDL_REVISION_1,
      label ? LABEL_SECURITY_INFORMATION : DACL_SECURITY_INFORMATION, &sddl_str,
      nullptr));
  auto sddl_str_ptr = TakeLocalAlloc(sddl_str);
  std::wstring ret = sddl_str_ptr.get();
  CHECK(ret.substr(0, 2) == (label ? kSaclPrefix : kDaclPrefix));
  return ret.substr(2);
}
}  // namespace

TEST(AccessControlListTest, FromPACL) {
  auto null_acl = AccessControlList::FromPACL(nullptr);
  ASSERT_TRUE(null_acl);
  EXPECT_TRUE(null_acl->is_null());
  EXPECT_EQ(null_acl->get(), nullptr);
  EXPECT_EQ(ConvertAclToSddl(*null_acl), kNullAcl);

  ACL dummy_acl = {};
  auto invalid_acl = AccessControlList::FromPACL(&dummy_acl);
  DWORD error = ::GetLastError();
  EXPECT_FALSE(invalid_acl);
  EXPECT_EQ(error, DWORD{ERROR_INVALID_ACL});

  std::vector<char> compare_acl = ConvertSddlToAcl(kFromPACLTest);
  ASSERT_FALSE(compare_acl.empty());
  auto test_acl =
      AccessControlList::FromPACL(reinterpret_cast<ACL*>(compare_acl.data()));
  ASSERT_TRUE(test_acl);
  EXPECT_EQ(ConvertAclToSddl(*test_acl), kFromPACLTest);
}

TEST(AccessControlListTest, FromMandatoryLabel) {
  auto acl = AccessControlList::FromMandatoryLabel(0, 0, 0);
  ASSERT_TRUE(acl);
  EXPECT_EQ(ConvertAclToSddl(*acl, true), kUntrustedLabel);
  acl = AccessControlList::FromMandatoryLabel(SECURITY_MANDATORY_SYSTEM_RID, 0,
                                              0);
  ASSERT_TRUE(acl);
  EXPECT_EQ(ConvertAclToSddl(*acl, true), kSystemLabel);
  acl = AccessControlList::FromMandatoryLabel(
      SECURITY_MANDATORY_SYSTEM_RID, OBJECT_INHERIT_ACE | CONTAINER_INHERIT_ACE,
      0);
  ASSERT_TRUE(acl);
  EXPECT_EQ(ConvertAclToSddl(*acl, true), kSystemLabelInherit);
  acl = AccessControlList::FromMandatoryLabel(
      SECURITY_MANDATORY_SYSTEM_RID, 0,
      SYSTEM_MANDATORY_LABEL_NO_WRITE_UP | SYSTEM_MANDATORY_LABEL_NO_READ_UP |
          SYSTEM_MANDATORY_LABEL_NO_EXECUTE_UP);
  ASSERT_TRUE(acl);
  EXPECT_EQ(ConvertAclToSddl(*acl, true), kSystemLabelPolicy);
  acl = AccessControlList::FromMandatoryLabel(
      SECURITY_MANDATORY_SYSTEM_RID, OBJECT_INHERIT_ACE | CONTAINER_INHERIT_ACE,
      SYSTEM_MANDATORY_LABEL_NO_WRITE_UP | SYSTEM_MANDATORY_LABEL_NO_READ_UP |
          SYSTEM_MANDATORY_LABEL_NO_EXECUTE_UP);
  ASSERT_TRUE(acl);
  EXPECT_EQ(ConvertAclToSddl(*acl, true), kSystemLabelInheritPolicy);
}

TEST(AccessControlListTest, Empty) {
  AccessControlList acl;
  EXPECT_FALSE(acl.is_null());
  ACL* acl_ptr = acl.get();
  ASSERT_NE(acl_ptr, nullptr);
  EXPECT_EQ(acl_ptr->AceCount, 0);
  EXPECT_EQ(acl_ptr->AclSize, sizeof(ACL));
  EXPECT_EQ(acl_ptr->AclRevision, ACL_REVISION);
  EXPECT_TRUE(ConvertAclToSddl(acl).empty());
  AccessControlList acl_clone = acl.Clone();
  EXPECT_NE(acl.get(), acl_clone.get());
  EXPECT_TRUE(ConvertAclToSddl(acl_clone).empty());
}

TEST(AccessControlListTest, ExplicitAccessEntry) {
  constexpr DWORD FakeAccess = 0x12345678U;
  constexpr DWORD FakeInherit = 0x87654321U;
  Sid sid(WellKnownSid::kWorld);
  ExplicitAccessEntry ace(sid, SecurityAccessMode::kGrant, FakeAccess,
                          FakeInherit);
  EXPECT_EQ(sid, ace.sid());
  EXPECT_EQ(SecurityAccessMode::kGrant, ace.mode());
  EXPECT_EQ(FakeAccess, ace.access_mask());
  EXPECT_EQ(FakeInherit, ace.inheritance());
  ExplicitAccessEntry ace_clone = ace.Clone();
  EXPECT_EQ(ace.sid(), ace_clone.sid());
  EXPECT_NE(ace.sid().GetPSID(), ace_clone.sid().GetPSID());
  EXPECT_EQ(ace.mode(), ace_clone.mode());
  EXPECT_EQ(ace.access_mask(), ace_clone.access_mask());
  EXPECT_EQ(ace.inheritance(), ace_clone.inheritance());
  ExplicitAccessEntry ace_known(WellKnownSid::kSelf,
                                SecurityAccessMode::kRevoke, ~FakeAccess,
                                ~FakeInherit);
  EXPECT_EQ(Sid(WellKnownSid::kSelf), ace_known.sid());
  EXPECT_EQ(SecurityAccessMode::kRevoke, ace_known.mode());
  EXPECT_EQ(~FakeAccess, ace_known.access_mask());
  EXPECT_EQ(~FakeInherit, ace_known.inheritance());
}

TEST(AccessControlListTest, SetEntries) {
  AccessControlList acl;
  std::vector<ExplicitAccessEntry> ace_list;
  EXPECT_TRUE(acl.SetEntries(ace_list));
  EXPECT_EQ(ConvertAclToSddl(acl), L"");
  ace_list.emplace_back(Sid(WellKnownSid::kWorld), SecurityAccessMode::kGrant,
                        EVENT_ALL_ACCESS, 0);
  EXPECT_TRUE(acl.SetEntries(ace_list));
  EXPECT_EQ(ConvertAclToSddl(acl), kEvent);
  ace_list.emplace_back(Sid(WellKnownSid::kLocalSystem),
                        SecurityAccessMode::kDeny, EVENT_MODIFY_STATE,
                        CONTAINER_INHERIT_ACE | OBJECT_INHERIT_ACE);
  EXPECT_TRUE(acl.SetEntries(ace_list));
  EXPECT_EQ(ConvertAclToSddl(acl), kEventWithSystemInherit);
  ace_list.emplace_back(Sid(WellKnownSid::kWorld), SecurityAccessMode::kRevoke,
                        EVENT_MODIFY_STATE, 0);
  EXPECT_TRUE(acl.SetEntries(ace_list));
  EXPECT_EQ(ConvertAclToSddl(acl), kEventSystemOnlyInherit);
  AccessControlList acl_clone = acl.Clone();
  EXPECT_NE(acl.get(), acl_clone.get());
  EXPECT_EQ(ConvertAclToSddl(acl_clone), kEventSystemOnlyInherit);
}

TEST(AccessControlListTest, SetEntry) {
  AccessControlList acl;
  EXPECT_TRUE(acl.SetEntry(Sid(WellKnownSid::kWorld),
                           SecurityAccessMode::kGrant, READ_CONTROL, 0));
  EXPECT_EQ(ConvertAclToSddl(acl), kEventReadControl);
  EXPECT_TRUE(acl.SetEntry(Sid(WellKnownSid::kWorld),
                           SecurityAccessMode::kGrant, EVENT_MODIFY_STATE, 0));
  EXPECT_EQ(ConvertAclToSddl(acl), kEventReadControlModify);
  EXPECT_TRUE(acl.SetEntry(Sid(WellKnownSid::kWorld), SecurityAccessMode::kSet,
                           EVENT_ALL_ACCESS, 0));
  EXPECT_EQ(ConvertAclToSddl(acl), kEvent);
  EXPECT_TRUE(acl.SetEntry(Sid(WellKnownSid::kLocalSystem),
                           SecurityAccessMode::kDeny, EVENT_MODIFY_STATE,
                           CONTAINER_INHERIT_ACE | OBJECT_INHERIT_ACE));
  EXPECT_EQ(ConvertAclToSddl(acl), kEventWithSystemInherit);
  EXPECT_TRUE(acl.SetEntry(Sid(WellKnownSid::kWorld),
                           SecurityAccessMode::kRevoke, EVENT_ALL_ACCESS, 0));
  EXPECT_EQ(ConvertAclToSddl(acl), kEventSystemOnlyInherit);
}

TEST(AccessControlListTest, SetEntriesError) {
  AccessControlList acl;
  std::vector<ExplicitAccessEntry> ace_list;
  ace_list.emplace_back(Sid(WellKnownSid::kWorld), SecurityAccessMode::kGrant,
                        EVENT_ALL_ACCESS, 0);
  EXPECT_TRUE(acl.SetEntries(ace_list));
  EXPECT_EQ(ConvertAclToSddl(acl), kEvent);
  // ACL has a maximum capacity of 2^16-1 bytes or 2^16-1 ACEs. Force a fail.
  while (ace_list.size() < 0x10000) {
    auto sid =
        Sid::FromSddlString(L"S-1-5-1234-" + NumberToWString(ace_list.size()));
    ASSERT_TRUE(sid);
    ace_list.emplace_back(*sid, SecurityAccessMode::kGrant, GENERIC_ALL, 0);
  }
  ::SetLastError(0);
  bool result = acl.SetEntries(ace_list);
  DWORD error = ::GetLastError();
  EXPECT_FALSE(result);
  EXPECT_EQ(error, DWORD{ERROR_INVALID_PARAMETER});
}

TEST(AccessControlListTest, Clear) {
  AccessControlList acl;
  EXPECT_TRUE(acl.SetEntry(Sid(WellKnownSid::kWorld),
                           SecurityAccessMode::kGrant, READ_CONTROL, 0));
  EXPECT_EQ(acl.get()->AceCount, 1);
  acl.Clear();
  ASSERT_NE(acl.get(), nullptr);
  EXPECT_EQ(acl.get()->AceCount, 0);
}

}  // namespace base::win
