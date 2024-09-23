// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/win/security_descriptor.h"

#include <windows.h>

#include <aclapi.h>
#include <sddl.h>

#include <optional>
#include <string>
#include <vector>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/unguessable_token.h"
#include "base/win/scoped_handle.h"
#include "base/win/scoped_localalloc.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base::win {

namespace {

constexpr wchar_t kOwnerOnly[] = L"O:BU";
constexpr wchar_t kGroupOnly[] = L"G:SY";
constexpr wchar_t kDaclOnly[] = L"D:(A;;GA;;;WD)";
constexpr wchar_t kProtectedDaclOnly[] = L"D:P(A;;GA;;;WD)";
constexpr wchar_t kSaclOnly[] = L"S:(ML;;;;;SI)";
constexpr wchar_t kProtectedSaclOnly[] = L"S:P(ML;;;;;SI)";
constexpr wchar_t kSaclProtected[] = L"S:P";
constexpr wchar_t kFullSd[] = L"O:BUG:SYD:P(A;;GA;;;WD)S:P(ML;;;;;SI)";
constexpr wchar_t kFileProtected[] = L"D:P(A;;FA;;;WD)";
constexpr wchar_t kFileIntegrity[] = L"S:(ML;;NW;;;ME)";
constexpr wchar_t kFileIntegrityInherit[] = L"S:(ML;OICI;NW;;;ME)";
constexpr wchar_t kFileProtectedIntegrity[] = L"D:P(A;;FA;;;WD)S:(ML;;NW;;;ME)";
constexpr wchar_t kNewDirectory[] = L"D:P(A;OICI;FA;;;WD)";
constexpr wchar_t kInheritedFile[] = L"D:(A;ID;FA;;;WD)";
constexpr wchar_t kProtectedUsers[] = L"D:P(A;;FA;;;BU)";
constexpr wchar_t kEvent[] = L"D:(A;;0x1f0003;;;WD)";
constexpr wchar_t kEventWithSystem[] = L"D:(D;;DC;;;SY)(A;;0x1f0003;;;WD)";
constexpr wchar_t kEventSystemOnly[] = L"D:(D;;DC;;;SY)";
constexpr wchar_t kEventProtectedWithLabel[] =
    L"D:P(A;;0x1f0003;;;WD)S:(ML;;NW;;;ME)";
constexpr wchar_t kEventReadControl[] = L"D:(A;;RC;;;WD)";
constexpr wchar_t kEventReadControlModify[] = L"D:(A;;DCRC;;;WD)";
constexpr wchar_t kNullDacl[] = L"D:NO_ACCESS_CONTROL";
constexpr wchar_t kEmptyDacl[] = L"D:";
constexpr wchar_t kAccessCheckSd[] = L"O:SYG:SYD:P(A;;0x1fffff;;;WD)";
constexpr SECURITY_INFORMATION kAllSecurityInfo =
    OWNER_SECURITY_INFORMATION | GROUP_SECURITY_INFORMATION |
    DACL_SECURITY_INFORMATION | LABEL_SECURITY_INFORMATION;
constexpr SECURITY_INFORMATION kDaclLabelSecurityInfo =
    DACL_SECURITY_INFORMATION | LABEL_SECURITY_INFORMATION;

base::win::ScopedLocalAllocTyped<void> ConvertSddlToSd(const wchar_t* sddl) {
  PSECURITY_DESCRIPTOR sd = nullptr;
  CHECK(ConvertStringSecurityDescriptorToSecurityDescriptor(
      sddl, SDDL_REVISION_1, &sd, nullptr));
  return TakeLocalAlloc(sd);
}

bool CreateFileWithSd(const FilePath& path, void* sd, bool directory) {
  SECURITY_ATTRIBUTES security_attr = {};
  security_attr.nLength = sizeof(security_attr);
  security_attr.lpSecurityDescriptor = sd;
  if (directory)
    return !!::CreateDirectory(path.value().c_str(), &security_attr);

  return ScopedHandle(::CreateFile(path.value().c_str(), GENERIC_ALL, 0,
                                   &security_attr, CREATE_ALWAYS, 0, nullptr))
      .is_valid();
}

bool CreateFileWithDacl(const FilePath& path,
                        const wchar_t* sddl,
                        bool directory) {
  auto sd_ptr = ConvertSddlToSd(sddl);
  CHECK(sd_ptr);
  return CreateFileWithSd(path, sd_ptr.get(), directory);
}

base::win::ScopedHandle CreateEventWithDacl(const wchar_t* name,
                                            const wchar_t* sddl) {
  auto sd_ptr = ConvertSddlToSd(sddl);
  CHECK(sd_ptr);
  SECURITY_ATTRIBUTES security_attr = {};
  security_attr.nLength = sizeof(security_attr);
  security_attr.lpSecurityDescriptor = sd_ptr.get();
  return base::win::ScopedHandle(
      ::CreateEvent(&security_attr, FALSE, FALSE, name));
}

base::win::ScopedHandle DuplicateHandle(const base::win::ScopedHandle& handle,
                                        DWORD access_mask) {
  HANDLE dup_handle;
  CHECK(::DuplicateHandle(::GetCurrentProcess(), handle.get(),
                          ::GetCurrentProcess(), &dup_handle, access_mask,
                          FALSE, 0));
  return base::win::ScopedHandle(dup_handle);
}

void ExpectSid(const std::optional<Sid>& sid, WellKnownSid known_sid) {
  ASSERT_TRUE(sid);
  EXPECT_EQ(*sid, Sid(known_sid));
}

void AccessCheckError(const std::optional<AccessCheckResult>& result,
                      DWORD expected) {
  EXPECT_FALSE(result.has_value());
  EXPECT_EQ(::GetLastError(), expected);
}

void AccessCheckStatusError(const std::optional<AccessCheckResult>& result,
                            DWORD expected) {
  ASSERT_TRUE(result.has_value());
  EXPECT_FALSE(result->access_status);
  EXPECT_EQ(::GetLastError(), expected);
}

void AccessCheckTest(const std::optional<AccessCheckResult>& result,
                     ACCESS_MASK expected_access) {
  ASSERT_TRUE(result.has_value());
  EXPECT_TRUE(result->access_status);
  EXPECT_EQ(result->granted_access, expected_access);
}

template <typename T>
void AccessCheckTest(T type,
                     ACCESS_MASK generic_read,
                     ACCESS_MASK generic_write,
                     ACCESS_MASK generic_execute,
                     ACCESS_MASK generic_all) {
  std::optional<SecurityDescriptor> sd =
      SecurityDescriptor::FromSddl(kAccessCheckSd);
  ASSERT_TRUE(sd);
  std::optional<AccessToken> token = AccessToken::FromCurrentProcess(
      /*impersonation=*/true, TOKEN_ADJUST_DEFAULT);
  ASSERT_TRUE(token.has_value());
  AccessCheckTest(sd->AccessCheck(*token, GENERIC_READ, type), generic_read);
  AccessCheckTest(sd->AccessCheck(*token, GENERIC_WRITE, type), generic_write);
  AccessCheckTest(sd->AccessCheck(*token, GENERIC_EXECUTE, type),
                  generic_execute);
  AccessCheckTest(sd->AccessCheck(*token, GENERIC_ALL, type), generic_all);
  AccessCheckTest(sd->AccessCheck(*token, 0x1fffff, type), 0x1fffff);
  AccessCheckTest(sd->AccessCheck(*token, MAXIMUM_ALLOWED, type), 0x1fffff);
  ASSERT_TRUE(sd->SetMandatoryLabel(SECURITY_MANDATORY_LOW_RID, 0,
                                    SYSTEM_MANDATORY_LABEL_NO_WRITE_UP));
  ASSERT_TRUE(token->SetIntegrityLevel(SECURITY_MANDATORY_UNTRUSTED_RID));
  AccessCheckTest(sd->AccessCheck(*token, GENERIC_READ, type), generic_read);
  AccessCheckTest(sd->AccessCheck(*token, GENERIC_EXECUTE, type),
                  generic_execute);
  AccessCheckStatusError(sd->AccessCheck(*token, GENERIC_WRITE, type),
                         ERROR_ACCESS_DENIED);
  ASSERT_TRUE(sd->SetMandatoryLabel(SECURITY_MANDATORY_UNTRUSTED_RID, 0,
                                    SYSTEM_MANDATORY_LABEL_NO_WRITE_UP));
  AccessCheckTest(sd->AccessCheck(*token, GENERIC_ALL, type), generic_all);
}

void AccessCheckTest(const GENERIC_MAPPING& type) {
  AccessCheckTest(type, type.GenericRead, type.GenericWrite,
                  type.GenericExecute, type.GenericAll);
}

}  // namespace

TEST(SecurityDescriptorTest, Initialize) {
  SecurityDescriptor sd;
  EXPECT_FALSE(sd.owner());
  EXPECT_FALSE(sd.group());
  EXPECT_FALSE(sd.dacl());
  EXPECT_FALSE(sd.dacl_protected());
  EXPECT_FALSE(sd.sacl());
  EXPECT_FALSE(sd.sacl_protected());

  sd.set_owner(Sid(WellKnownSid::kBuiltinUsers));
  ExpectSid(sd.owner(), WellKnownSid::kBuiltinUsers);
  sd.clear_owner();
  EXPECT_FALSE(sd.owner());
  sd.set_group(Sid(WellKnownSid::kLocalSystem));
  ExpectSid(sd.group(), WellKnownSid::kLocalSystem);
  sd.clear_group();
  EXPECT_FALSE(sd.group());
  sd.set_dacl(AccessControlList());
  EXPECT_TRUE(sd.dacl());
  EXPECT_FALSE(sd.dacl()->is_null());
  sd.clear_dacl();
  EXPECT_FALSE(sd.dacl());
  sd.set_sacl(AccessControlList());
  EXPECT_TRUE(sd.sacl());
  EXPECT_FALSE(sd.sacl()->is_null());
  sd.clear_sacl();
  EXPECT_FALSE(sd.sacl());
}

TEST(SecurityDescriptorTest, FromPointer) {
  auto sd = SecurityDescriptor::FromPointer(nullptr);
  EXPECT_FALSE(sd);
  SECURITY_DESCRIPTOR sd_abs = {};
  sd = SecurityDescriptor::FromPointer(&sd_abs);
  EXPECT_FALSE(sd);
  sd = SecurityDescriptor::FromPointer(ConvertSddlToSd(kOwnerOnly).get());
  ASSERT_TRUE(sd);
  ExpectSid(sd->owner(), WellKnownSid::kBuiltinUsers);
  sd = SecurityDescriptor::FromPointer(ConvertSddlToSd(kGroupOnly).get());
  ASSERT_TRUE(sd);
  ExpectSid(sd->group(), WellKnownSid::kLocalSystem);
  sd = SecurityDescriptor::FromPointer(ConvertSddlToSd(kDaclOnly).get());
  ASSERT_TRUE(sd);
  EXPECT_TRUE(sd->dacl());
  EXPECT_FALSE(sd->dacl_protected());
  sd = SecurityDescriptor::FromPointer(
      ConvertSddlToSd(kProtectedDaclOnly).get());
  ASSERT_TRUE(sd);
  EXPECT_TRUE(sd->dacl());
  EXPECT_TRUE(sd->dacl_protected());
  sd = SecurityDescriptor::FromPointer(ConvertSddlToSd(kSaclOnly).get());
  ASSERT_TRUE(sd);
  EXPECT_TRUE(sd->sacl());
  EXPECT_FALSE(sd->sacl_protected());
  sd = SecurityDescriptor::FromPointer(
      ConvertSddlToSd(kProtectedSaclOnly).get());
  ASSERT_TRUE(sd);
  EXPECT_TRUE(sd->sacl());
  EXPECT_TRUE(sd->sacl_protected());
  sd = SecurityDescriptor::FromPointer(ConvertSddlToSd(kFullSd).get());
  ASSERT_TRUE(sd);
  ExpectSid(sd->owner(), WellKnownSid::kBuiltinUsers);
  ExpectSid(sd->group(), WellKnownSid::kLocalSystem);
  EXPECT_TRUE(sd->dacl());
  EXPECT_TRUE(sd->dacl_protected());
  EXPECT_TRUE(sd->sacl());
  EXPECT_TRUE(sd->sacl_protected());
}

TEST(SecurityDescriptorTest, ToSddl) {
  auto sd = SecurityDescriptor::FromPointer(ConvertSddlToSd(kFullSd).get());
  ASSERT_TRUE(sd);
  EXPECT_EQ(sd->ToSddl(0), L"");
  EXPECT_EQ(sd->ToSddl(OWNER_SECURITY_INFORMATION), kOwnerOnly);
  EXPECT_EQ(sd->ToSddl(GROUP_SECURITY_INFORMATION), kGroupOnly);
  EXPECT_EQ(sd->ToSddl(DACL_SECURITY_INFORMATION), kProtectedDaclOnly);
  EXPECT_EQ(sd->ToSddl(LABEL_SECURITY_INFORMATION), kProtectedSaclOnly);
  EXPECT_EQ(sd->ToSddl(SACL_SECURITY_INFORMATION), kSaclProtected);
  EXPECT_EQ(sd->ToSddl(kAllSecurityInfo), kFullSd);
  SecurityDescriptor empty_sd;
  empty_sd.set_dacl(AccessControlList());
  EXPECT_EQ(empty_sd.ToSddl(DACL_SECURITY_INFORMATION), kEmptyDacl);
}

TEST(SecurityDescriptorTest, FromSddl) {
  auto sd = SecurityDescriptor::FromSddl(L"");
  EXPECT_TRUE(sd);
  EXPECT_FALSE(sd->owner());
  EXPECT_FALSE(sd->group());
  EXPECT_FALSE(sd->dacl());
  EXPECT_FALSE(sd->sacl());
  sd = SecurityDescriptor::FromSddl(kOwnerOnly);
  ASSERT_TRUE(sd);
  ExpectSid(sd->owner(), WellKnownSid::kBuiltinUsers);
  sd = SecurityDescriptor::FromSddl(kGroupOnly);
  ASSERT_TRUE(sd);
  ExpectSid(sd->group(), WellKnownSid::kLocalSystem);
  sd = SecurityDescriptor::FromSddl(kDaclOnly);
  ASSERT_TRUE(sd);
  EXPECT_TRUE(sd->dacl());
  EXPECT_FALSE(sd->dacl_protected());
  sd = SecurityDescriptor::FromSddl(kProtectedDaclOnly);
  ASSERT_TRUE(sd);
  EXPECT_TRUE(sd->dacl());
  EXPECT_TRUE(sd->dacl_protected());
  sd = SecurityDescriptor::FromSddl(kSaclOnly);
  ASSERT_TRUE(sd);
  EXPECT_TRUE(sd->sacl());
  EXPECT_FALSE(sd->sacl_protected());
  sd = SecurityDescriptor::FromSddl(kProtectedSaclOnly);
  ASSERT_TRUE(sd);
  EXPECT_TRUE(sd->sacl());
  EXPECT_TRUE(sd->sacl_protected());
  sd = SecurityDescriptor::FromSddl(kFullSd);
  ASSERT_TRUE(sd);
  ExpectSid(sd->owner(), WellKnownSid::kBuiltinUsers);
  ExpectSid(sd->group(), WellKnownSid::kLocalSystem);
  EXPECT_TRUE(sd->dacl());
  EXPECT_TRUE(sd->dacl_protected());
  EXPECT_TRUE(sd->sacl());
  EXPECT_TRUE(sd->sacl_protected());
  sd = SecurityDescriptor::FromSddl(kNullDacl);
  ASSERT_TRUE(sd);
  ASSERT_TRUE(sd->dacl());
  EXPECT_TRUE(sd->dacl()->is_null());
}

TEST(SecurityDescriptorTest, Clone) {
  SecurityDescriptor cloned = SecurityDescriptor().Clone();
  EXPECT_FALSE(cloned.owner());
  EXPECT_FALSE(cloned.group());
  EXPECT_FALSE(cloned.dacl());
  EXPECT_FALSE(cloned.dacl_protected());
  EXPECT_FALSE(cloned.sacl());
  EXPECT_FALSE(cloned.sacl_protected());
  auto sd = SecurityDescriptor::FromSddl(kFullSd);
  ASSERT_TRUE(sd);
  cloned = sd->Clone();
  EXPECT_EQ(sd->owner(), cloned.owner());
  EXPECT_NE(sd->owner()->GetPSID(), cloned.owner()->GetPSID());
  EXPECT_EQ(sd->group(), cloned.group());
  EXPECT_NE(sd->group()->GetPSID(), cloned.group()->GetPSID());
  EXPECT_NE(sd->dacl()->get(), cloned.dacl()->get());
  EXPECT_EQ(sd->dacl_protected(), cloned.dacl_protected());
  EXPECT_NE(sd->sacl()->get(), cloned.sacl()->get());
  EXPECT_EQ(sd->sacl_protected(), cloned.sacl_protected());
}

TEST(SecurityDescriptorTest, ToAbsolute) {
  auto sd = SecurityDescriptor::FromPointer(ConvertSddlToSd(kFullSd).get());
  ASSERT_TRUE(sd);
  SECURITY_DESCRIPTOR sd_abs;
  sd->ToAbsolute(sd_abs);
  EXPECT_EQ(sd_abs.Revision, SECURITY_DESCRIPTOR_REVISION);
  EXPECT_EQ(sd_abs.Control, SE_DACL_PRESENT | SE_DACL_PROTECTED |
                                SE_SACL_PRESENT | SE_SACL_PROTECTED);
  EXPECT_EQ(sd_abs.Owner, sd->owner()->GetPSID());
  EXPECT_EQ(sd_abs.Group, sd->group()->GetPSID());
  EXPECT_EQ(sd_abs.Dacl, sd->dacl()->get());
  EXPECT_EQ(sd_abs.Sacl, sd->sacl()->get());
}

TEST(SecurityDescriptorTest, ToSelfRelative) {
  auto sd = SecurityDescriptor::FromPointer(ConvertSddlToSd(kFullSd).get());
  ASSERT_TRUE(sd);
  auto sd_rel = sd->ToSelfRelative();
  ASSERT_TRUE(sd_rel);
  EXPECT_TRUE(sd_rel->get());
  EXPECT_EQ(sd_rel->size(), ::GetSecurityDescriptorLength(sd_rel->get()));
  sd = SecurityDescriptor::FromPointer(sd_rel->get());
  ASSERT_TRUE(sd);
  EXPECT_EQ(sd->ToSddl(kAllSecurityInfo), kFullSd);
}

TEST(SecurityDescriptorTest, SetMandatoryLabel) {
  SecurityDescriptor sd;
  EXPECT_FALSE(sd.sacl());
  sd.SetMandatoryLabel(SECURITY_MANDATORY_SYSTEM_RID, 0, 0);
  EXPECT_TRUE(sd.sacl());
  EXPECT_EQ(sd.ToSddl(LABEL_SECURITY_INFORMATION), kSaclOnly);
  sd.SetMandatoryLabel(SECURITY_MANDATORY_MEDIUM_RID,
                       OBJECT_INHERIT_ACE | CONTAINER_INHERIT_ACE,
                       SYSTEM_MANDATORY_LABEL_NO_WRITE_UP);
  EXPECT_TRUE(sd.sacl());
  EXPECT_EQ(sd.ToSddl(LABEL_SECURITY_INFORMATION), kFileIntegrityInherit);
}

TEST(SecurityDescriptorTest, SetDaclEntries) {
  SecurityDescriptor sd;
  EXPECT_FALSE(sd.dacl());
  std::vector<ExplicitAccessEntry> ace_list;
  EXPECT_TRUE(sd.SetDaclEntries(ace_list));
  EXPECT_EQ(sd.ToSddl(DACL_SECURITY_INFORMATION), kEmptyDacl);
  ace_list.emplace_back(Sid(WellKnownSid::kWorld), SecurityAccessMode::kGrant,
                        EVENT_ALL_ACCESS, 0);
  EXPECT_TRUE(sd.SetDaclEntries(ace_list));
  EXPECT_EQ(sd.ToSddl(DACL_SECURITY_INFORMATION), kEvent);
  ace_list.emplace_back(Sid(WellKnownSid::kLocalSystem),
                        SecurityAccessMode::kDeny, EVENT_MODIFY_STATE, 0);
  EXPECT_TRUE(sd.SetDaclEntries(ace_list));
  EXPECT_EQ(sd.ToSddl(DACL_SECURITY_INFORMATION), kEventWithSystem);
  ace_list.emplace_back(Sid(WellKnownSid::kWorld), SecurityAccessMode::kRevoke,
                        EVENT_MODIFY_STATE, 0);
  EXPECT_TRUE(sd.SetDaclEntries(ace_list));
  EXPECT_EQ(sd.ToSddl(DACL_SECURITY_INFORMATION), kEventSystemOnly);
}

TEST(SecurityDescriptorTest, SetDaclEntry) {
  SecurityDescriptor sd;
  EXPECT_TRUE(sd.SetDaclEntry(Sid(WellKnownSid::kWorld),
                              SecurityAccessMode::kGrant, READ_CONTROL, 0));
  EXPECT_EQ(sd.ToSddl(DACL_SECURITY_INFORMATION), kEventReadControl);
  EXPECT_TRUE(sd.SetDaclEntry(Sid(WellKnownSid::kWorld),
                              SecurityAccessMode::kGrant, EVENT_MODIFY_STATE,
                              0));
  EXPECT_EQ(sd.ToSddl(DACL_SECURITY_INFORMATION), kEventReadControlModify);
  EXPECT_TRUE(sd.SetDaclEntry(Sid(WellKnownSid::kWorld),
                              SecurityAccessMode::kSet, EVENT_ALL_ACCESS, 0));
  EXPECT_EQ(sd.ToSddl(DACL_SECURITY_INFORMATION), kEvent);
  EXPECT_TRUE(sd.SetDaclEntry(Sid(WellKnownSid::kLocalSystem),
                              SecurityAccessMode::kDeny, EVENT_MODIFY_STATE,
                              0));
  EXPECT_EQ(sd.ToSddl(DACL_SECURITY_INFORMATION), kEventWithSystem);
  EXPECT_TRUE(sd.SetDaclEntry(Sid(WellKnownSid::kWorld),
                              SecurityAccessMode::kRevoke, EVENT_ALL_ACCESS,
                              0));
  EXPECT_EQ(sd.ToSddl(DACL_SECURITY_INFORMATION), kEventSystemOnly);
  sd.clear_dacl();
  EXPECT_TRUE(sd.SetDaclEntry(WellKnownSid::kWorld, SecurityAccessMode::kGrant,
                              READ_CONTROL, 0));
  EXPECT_EQ(sd.ToSddl(DACL_SECURITY_INFORMATION), kEventReadControl);
}

TEST(SecurityDescriptorTest, FromFile) {
  ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  FilePath path = temp_dir.GetPath().Append(L"test");
  EXPECT_FALSE(SecurityDescriptor::FromFile(path, kAllSecurityInfo));
  ASSERT_TRUE(CreateFileWithDacl(path, kFileProtectedIntegrity, false));
  auto sd = SecurityDescriptor::FromFile(path, kAllSecurityInfo);
  ASSERT_TRUE(sd);
  EXPECT_EQ(sd->ToSddl(DACL_SECURITY_INFORMATION), kFileProtected);
  sd = SecurityDescriptor::FromFile(path, LABEL_SECURITY_INFORMATION);
  ASSERT_TRUE(sd);
  EXPECT_EQ(sd->ToSddl(LABEL_SECURITY_INFORMATION), kFileIntegrity);
  sd = SecurityDescriptor::FromFile(path, kAllSecurityInfo);
  ASSERT_TRUE(sd);
  EXPECT_EQ(sd->ToSddl(kDaclLabelSecurityInfo), kFileProtectedIntegrity);
}

TEST(SecurityDescriptorTest, WriteToFile) {
  ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  FilePath dir_path = temp_dir.GetPath().Append(L"test");
  ASSERT_TRUE(CreateFileWithDacl(dir_path, kNewDirectory, true));
  FilePath path = dir_path.Append(L"test");
  ASSERT_TRUE(CreateFileWithSd(path, nullptr, false));

  auto curr_sd = SecurityDescriptor::FromFile(path, DACL_SECURITY_INFORMATION);
  ASSERT_TRUE(curr_sd);
  EXPECT_EQ(curr_sd->ToSddl(DACL_SECURITY_INFORMATION), kInheritedFile);

  AccessControlList new_acl;
  EXPECT_TRUE(new_acl.SetEntry(Sid(WellKnownSid::kBuiltinUsers),
                               SecurityAccessMode::kGrant, FILE_ALL_ACCESS, 0));
  SecurityDescriptor new_sd;
  new_sd.set_dacl(new_acl);
  new_sd.set_dacl_protected(true);
  EXPECT_TRUE(new_sd.WriteToFile(path, DACL_SECURITY_INFORMATION));
  curr_sd = SecurityDescriptor::FromFile(path, DACL_SECURITY_INFORMATION);
  ASSERT_TRUE(curr_sd);
  EXPECT_EQ(curr_sd->ToSddl(DACL_SECURITY_INFORMATION), kProtectedUsers);

  SecurityDescriptor empty_sd;
  empty_sd.set_dacl(AccessControlList{});
  EXPECT_TRUE(empty_sd.WriteToFile(path, DACL_SECURITY_INFORMATION));
  curr_sd = SecurityDescriptor::FromFile(path, DACL_SECURITY_INFORMATION);
  ASSERT_TRUE(curr_sd);
  EXPECT_EQ(curr_sd->ToSddl(DACL_SECURITY_INFORMATION), kInheritedFile);

  auto label_acl = AccessControlList::FromMandatoryLabel(
      SECURITY_MANDATORY_MEDIUM_RID, 0, SYSTEM_MANDATORY_LABEL_NO_WRITE_UP);
  ASSERT_TRUE(label_acl);
  SecurityDescriptor label_sd;
  label_sd.set_sacl(*label_acl);
  EXPECT_TRUE(label_sd.WriteToFile(path, LABEL_SECURITY_INFORMATION));
  curr_sd = SecurityDescriptor::FromFile(path, LABEL_SECURITY_INFORMATION);
  ASSERT_TRUE(curr_sd);
  EXPECT_EQ(curr_sd->ToSddl(LABEL_SECURITY_INFORMATION), kFileIntegrity);
}

TEST(SecurityDescriptorTest, FromName) {
  std::wstring name =
      base::ASCIIToWide(base::UnguessableToken::Create().ToString());
  EXPECT_FALSE(SecurityDescriptor::FromName(
      name.c_str(), SecurityObjectType::kKernel, kAllSecurityInfo));
  base::win::ScopedHandle handle = CreateEventWithDacl(name.c_str(), kEvent);
  ASSERT_TRUE(handle.is_valid());
  auto curr_sd = SecurityDescriptor::FromName(
      name.c_str(), SecurityObjectType::kKernel, kAllSecurityInfo);
  ASSERT_TRUE(curr_sd);
  EXPECT_EQ(curr_sd->ToSddl(DACL_SECURITY_INFORMATION), kEvent);
  EXPECT_TRUE(SecurityDescriptor::FromName(
      L"MACHINE\\SOFTWARE", SecurityObjectType::kRegistry, kAllSecurityInfo));
  EXPECT_TRUE(SecurityDescriptor::FromName(L".", SecurityObjectType::kFile,
                                           kAllSecurityInfo));
  EXPECT_FALSE(SecurityDescriptor::FromName(
      L"WinSta0", SecurityObjectType::kWindowStation, kAllSecurityInfo));
  EXPECT_FALSE(SecurityDescriptor::FromName(
      L"Default", SecurityObjectType::kDesktop, kAllSecurityInfo));
}

TEST(SecurityDescriptorTest, WriteToName) {
  std::wstring name =
      base::ASCIIToWide(base::UnguessableToken::Create().ToString());
  EXPECT_FALSE(SecurityDescriptor().WriteToName(
      name.c_str(), SecurityObjectType::kKernel, kAllSecurityInfo));
  base::win::ScopedHandle handle = CreateEventWithDacl(name.c_str(), kEvent);
  ASSERT_TRUE(handle.is_valid());
  auto curr_sd = SecurityDescriptor::FromName(
      name.c_str(), SecurityObjectType::kKernel, kAllSecurityInfo);
  ASSERT_TRUE(curr_sd);
  curr_sd->set_dacl_protected(true);
  curr_sd->SetMandatoryLabel(SECURITY_MANDATORY_MEDIUM_RID, 0,
                             SYSTEM_MANDATORY_LABEL_NO_WRITE_UP);

  EXPECT_TRUE(curr_sd->WriteToName(name.c_str(), SecurityObjectType::kKernel,
                                   kDaclLabelSecurityInfo));

  curr_sd = SecurityDescriptor::FromName(
      name.c_str(), SecurityObjectType::kKernel, kAllSecurityInfo);
  ASSERT_TRUE(curr_sd);
  EXPECT_EQ(curr_sd->ToSddl(kDaclLabelSecurityInfo), kEventProtectedWithLabel);
}

TEST(SecurityDescriptorTest, FromHandle) {
  EXPECT_FALSE(SecurityDescriptor::FromHandle(
      nullptr, SecurityObjectType::kKernel, kAllSecurityInfo));
  auto handle = CreateEventWithDacl(nullptr, kEvent);
  ASSERT_TRUE(handle.is_valid());
  auto curr_sd = SecurityDescriptor::FromHandle(
      handle.get(), SecurityObjectType::kKernel, kAllSecurityInfo);
  ASSERT_TRUE(curr_sd);
  EXPECT_EQ(curr_sd->ToSddl(DACL_SECURITY_INFORMATION), kEvent);
  auto dup_handle = DuplicateHandle(handle, EVENT_MODIFY_STATE);
  EXPECT_FALSE(SecurityDescriptor::FromHandle(
      dup_handle.get(), SecurityObjectType::kKernel, kAllSecurityInfo));
  EXPECT_TRUE(SecurityDescriptor::FromHandle(::GetProcessWindowStation(),
                                             SecurityObjectType::kWindowStation,
                                             kAllSecurityInfo));
  EXPECT_TRUE(SecurityDescriptor::FromHandle(
      ::GetThreadDesktop(::GetCurrentThreadId()), SecurityObjectType::kDesktop,
      kAllSecurityInfo));
}

TEST(SecurityDescriptorTest, WriteToHandle) {
  EXPECT_FALSE(SecurityDescriptor().WriteToHandle(
      nullptr, SecurityObjectType::kKernel, kAllSecurityInfo));
  base::win::ScopedHandle handle = CreateEventWithDacl(nullptr, kEvent);
  ASSERT_TRUE(handle.is_valid());
  auto curr_sd = SecurityDescriptor::FromHandle(
      handle.get(), SecurityObjectType::kKernel, kAllSecurityInfo);
  EXPECT_EQ(curr_sd->ToSddl(DACL_SECURITY_INFORMATION), kEvent);
  ASSERT_TRUE(curr_sd);
  curr_sd->set_dacl_protected(true);
  curr_sd->SetMandatoryLabel(SECURITY_MANDATORY_MEDIUM_RID, 0,
                             SYSTEM_MANDATORY_LABEL_NO_WRITE_UP);

  EXPECT_TRUE(curr_sd->WriteToHandle(handle.get(), SecurityObjectType::kKernel,
                                     kDaclLabelSecurityInfo));

  curr_sd = SecurityDescriptor::FromHandle(
      handle.get(), SecurityObjectType::kKernel, kAllSecurityInfo);
  ASSERT_TRUE(curr_sd);
  EXPECT_EQ(curr_sd->ToSddl(kDaclLabelSecurityInfo), kEventProtectedWithLabel);
}

TEST(SecurityDescriptorTest, AccessCheck) {
  std::optional<SecurityDescriptor> sd =
      SecurityDescriptor::FromSddl(kAccessCheckSd);
  ASSERT_TRUE(sd);
  std::optional<AccessToken> token = AccessToken::FromCurrentProcess();
  ASSERT_TRUE(token);
  AccessCheckError(sd->AccessCheck(*token, 1, SecurityObjectType::kFile),
                   ERROR_NO_IMPERSONATION_TOKEN);
  token = AccessToken::FromCurrentProcess(/*impersonation=*/true);
  ASSERT_TRUE(token);
  AccessCheckError(sd->AccessCheck(*token, 1, SecurityObjectType::kKernel),
                   ERROR_INVALID_PARAMETER);
  sd->set_dacl(AccessControlList());
  AccessCheckStatusError(sd->AccessCheck(*token, 1, SecurityObjectType::kFile),
                         ERROR_ACCESS_DENIED);
  sd->clear_owner();
  AccessCheckError(sd->AccessCheck(*token, 1, SecurityObjectType::kFile),
                   ERROR_INVALID_SECURITY_DESCR);
  AccessCheckTest({1, 2, 4, 8});
  AccessCheckTest(SecurityObjectType::kFile, FILE_GENERIC_READ,
                  FILE_GENERIC_WRITE, FILE_GENERIC_EXECUTE, FILE_ALL_ACCESS);
  AccessCheckTest(SecurityObjectType::kRegistry, KEY_READ, KEY_WRITE,
                  KEY_EXECUTE, KEY_ALL_ACCESS);
  AccessCheckTest(
      SecurityObjectType::kDesktop,
      STANDARD_RIGHTS_READ | DESKTOP_READOBJECTS | DESKTOP_ENUMERATE,
      STANDARD_RIGHTS_WRITE | DESKTOP_CREATEWINDOW | DESKTOP_CREATEMENU |
          DESKTOP_HOOKCONTROL | DESKTOP_JOURNALRECORD |
          DESKTOP_JOURNALPLAYBACK | DESKTOP_WRITEOBJECTS,
      STANDARD_RIGHTS_EXECUTE | DESKTOP_SWITCHDESKTOP,
      STANDARD_RIGHTS_REQUIRED | DESKTOP_CREATEMENU | DESKTOP_CREATEWINDOW |
          DESKTOP_ENUMERATE | DESKTOP_HOOKCONTROL | DESKTOP_JOURNALPLAYBACK |
          DESKTOP_JOURNALRECORD | DESKTOP_READOBJECTS | DESKTOP_SWITCHDESKTOP |
          DESKTOP_WRITEOBJECTS);
  AccessCheckTest(
      SecurityObjectType::kWindowStation,
      STANDARD_RIGHTS_READ | WINSTA_ENUMDESKTOPS | WINSTA_ENUMERATE |
          WINSTA_READATTRIBUTES | WINSTA_READSCREEN,
      STANDARD_RIGHTS_WRITE | WINSTA_ACCESSCLIPBOARD | WINSTA_CREATEDESKTOP |
          WINSTA_WRITEATTRIBUTES,
      STANDARD_RIGHTS_EXECUTE | WINSTA_ACCESSGLOBALATOMS | WINSTA_EXITWINDOWS,
      STANDARD_RIGHTS_REQUIRED | WINSTA_ACCESSCLIPBOARD |
          WINSTA_ACCESSGLOBALATOMS | WINSTA_CREATEDESKTOP |
          WINSTA_ENUMDESKTOPS | WINSTA_ENUMERATE | WINSTA_EXITWINDOWS |
          WINSTA_READATTRIBUTES | WINSTA_READSCREEN | WINSTA_WRITEATTRIBUTES);
}

}  // namespace base::win
