// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/win/security_descriptor.h"

#include <aclapi.h>
#include <sddl.h>
#include <windows.h>

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
#include "third_party/abseil-cpp/absl/types/optional.h"

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

void ExpectSid(const absl::optional<Sid>& sid, WellKnownSid known_sid) {
  ASSERT_TRUE(sid);
  auto compare_sid = Sid::FromKnownSid(known_sid);
  ASSERT_TRUE(compare_sid);
  EXPECT_EQ(*sid, *compare_sid);
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

  sd.set_owner(*Sid::FromKnownSid(WellKnownSid::kBuiltinUsers));
  ExpectSid(sd.owner(), WellKnownSid::kBuiltinUsers);
  sd.clear_owner();
  EXPECT_FALSE(sd.owner());
  sd.set_group(*Sid::FromKnownSid(WellKnownSid::kLocalSystem));
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
  ace_list.emplace_back(*Sid::FromKnownSid(WellKnownSid::kWorld),
                        SecurityAccessMode::kGrant, EVENT_ALL_ACCESS, 0);
  EXPECT_TRUE(sd.SetDaclEntries(ace_list));
  EXPECT_EQ(sd.ToSddl(DACL_SECURITY_INFORMATION), kEvent);
  ace_list.emplace_back(*Sid::FromKnownSid(WellKnownSid::kLocalSystem),
                        SecurityAccessMode::kDeny, EVENT_MODIFY_STATE, 0);
  EXPECT_TRUE(sd.SetDaclEntries(ace_list));
  EXPECT_EQ(sd.ToSddl(DACL_SECURITY_INFORMATION), kEventWithSystem);
  ace_list.emplace_back(*Sid::FromKnownSid(WellKnownSid::kWorld),
                        SecurityAccessMode::kRevoke, EVENT_MODIFY_STATE, 0);
  EXPECT_TRUE(sd.SetDaclEntries(ace_list));
  EXPECT_EQ(sd.ToSddl(DACL_SECURITY_INFORMATION), kEventSystemOnly);
}

TEST(SecurityDescriptorTest, SetDaclEntry) {
  SecurityDescriptor sd;
  EXPECT_TRUE(sd.SetDaclEntry(*Sid::FromKnownSid(WellKnownSid::kWorld),
                              SecurityAccessMode::kGrant, READ_CONTROL, 0));
  EXPECT_EQ(sd.ToSddl(DACL_SECURITY_INFORMATION), kEventReadControl);
  EXPECT_TRUE(sd.SetDaclEntry(*Sid::FromKnownSid(WellKnownSid::kWorld),
                              SecurityAccessMode::kGrant, EVENT_MODIFY_STATE,
                              0));
  EXPECT_EQ(sd.ToSddl(DACL_SECURITY_INFORMATION), kEventReadControlModify);
  EXPECT_TRUE(sd.SetDaclEntry(*Sid::FromKnownSid(WellKnownSid::kWorld),
                              SecurityAccessMode::kSet, EVENT_ALL_ACCESS, 0));
  EXPECT_EQ(sd.ToSddl(DACL_SECURITY_INFORMATION), kEvent);
  EXPECT_TRUE(sd.SetDaclEntry(*Sid::FromKnownSid(WellKnownSid::kLocalSystem),
                              SecurityAccessMode::kDeny, EVENT_MODIFY_STATE,
                              0));
  EXPECT_EQ(sd.ToSddl(DACL_SECURITY_INFORMATION), kEventWithSystem);
  EXPECT_TRUE(sd.SetDaclEntry(*Sid::FromKnownSid(WellKnownSid::kWorld),
                              SecurityAccessMode::kRevoke, EVENT_ALL_ACCESS,
                              0));
  EXPECT_EQ(sd.ToSddl(DACL_SECURITY_INFORMATION), kEventSystemOnly);
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
  EXPECT_TRUE(new_acl.SetEntry(*Sid::FromKnownSid(WellKnownSid::kBuiltinUsers),
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
      L"Default", SecurityObjectType::kWindow, kAllSecurityInfo));
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

}  // namespace base::win
