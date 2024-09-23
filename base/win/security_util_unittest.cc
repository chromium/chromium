// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/win/security_util.h"

// clang-format off
#include <windows.h>  // Must be in front of other Windows header files.
// clang-format on

#include <aclapi.h>
#include <sddl.h>

#include <utility>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/strings/string_number_conversions_win.h"
#include "base/test/test_file_util.h"
#include "base/win/scoped_handle.h"
#include "base/win/scoped_localalloc.h"
#include "base/win/sid.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace win {

namespace {

constexpr wchar_t kBaseDacl[] = L"D:P(A;;FA;;;WD)";
constexpr wchar_t kTest1Dacl[] = L"D:PAI(A;;FR;;;AU)(A;;FA;;;WD)";
constexpr wchar_t kTest2Dacl[] = L"D:PAI(A;;FA;;;BA)(A;;FA;;;AU)(A;;FA;;;WD)";
constexpr wchar_t kTest1DenyDacl[] = L"D:PAI(D;;FR;;;LG)(A;;FA;;;WD)";
constexpr wchar_t kTest1DaclNoInherit[] = L"D:P(A;;FR;;;AU)(A;;FA;;;WD)";
constexpr wchar_t kTest2DaclNoInherit[] =
    L"D:P(A;;FA;;;BA)(A;;FA;;;AU)(A;;FA;;;WD)";

constexpr wchar_t kBaseDirDacl[] = L"D:P(A;OICI;FA;;;WD)";
constexpr wchar_t kTest1InheritedDacl[] = L"D:(A;ID;FA;;;WD)";
constexpr wchar_t kBaseDir2Dacl[] = L"D:PAI(A;OICI;FR;;;AU)(A;OICI;FA;;;WD)";
constexpr wchar_t kTest2InheritedDacl[] = L"D:AI(A;ID;FR;;;AU)(A;ID;FA;;;WD)";
constexpr wchar_t kBaseDir2DaclNoInherit[] =
    L"D:P(A;OICI;FR;;;AU)(A;OICI;FA;;;WD)";
constexpr wchar_t kTest2InheritedDaclNoInherit[] = L"D:P(A;;FA;;;WD)";
constexpr wchar_t kTest3InheritedDacl[] = L"D:(A;ID;FR;;;AU)(A;ID;FA;;;WD)";

constexpr wchar_t kNoWriteDacDacl[] = L"D:(D;;WD;;;OW)(A;;FRSD;;;WD)";

constexpr wchar_t kAuthenticatedUsersSid[] = L"AU";
constexpr wchar_t kLocalGuestSid[] = L"LG";

}  // namespace

TEST(SecurityUtilTest, GrantAccessToPathErrorCase) {
  ScopedTempDir temp_dir;
  auto sids = Sid::FromSddlStringVector({kAuthenticatedUsersSid});
  ASSERT_TRUE(sids);
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  FilePath path = temp_dir.GetPath().Append(L"test");
  EXPECT_FALSE(
      GrantAccessToPath(path, *sids, FILE_GENERIC_READ, NO_INHERITANCE, true));
  EXPECT_FALSE(
      GrantAccessToPath(path, *sids, FILE_GENERIC_READ, NO_INHERITANCE, false));
  ASSERT_TRUE(CreateWithDacl(path, kBaseDacl, false));
  EXPECT_TRUE(
      GrantAccessToPath(path, *sids, FILE_GENERIC_READ, NO_INHERITANCE, true));
  EXPECT_TRUE(
      GrantAccessToPath(path, *sids, FILE_GENERIC_READ, NO_INHERITANCE, false));
  std::vector<Sid> large_sid_list;
  while (large_sid_list.size() < 0x10000) {
    auto sid = Sid::FromSddlString(L"S-1-5-1234-" +
                                   NumberToWString(large_sid_list.size()));
    ASSERT_TRUE(sid);
    large_sid_list.emplace_back(std::move(*sid));
  }
  EXPECT_FALSE(GrantAccessToPath(path, large_sid_list, FILE_GENERIC_READ,
                                 NO_INHERITANCE, false));
  path = temp_dir.GetPath().Append(L"test_nowritedac");
  ASSERT_TRUE(CreateWithDacl(path, kNoWriteDacDacl, false));
  EXPECT_FALSE(
      GrantAccessToPath(path, *sids, FILE_GENERIC_READ, NO_INHERITANCE, true));
  EXPECT_FALSE(
      GrantAccessToPath(path, *sids, FILE_GENERIC_READ, NO_INHERITANCE, false));
}

TEST(SecurityUtilTest, GrantAccessToPathFile) {
  ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  FilePath path = temp_dir.GetPath().Append(L"test");
  ASSERT_TRUE(CreateWithDacl(path, kBaseDacl, false));
  EXPECT_EQ(kBaseDacl, GetFileDacl(path));
  auto sids = Sid::FromSddlStringVector({kAuthenticatedUsersSid});
  ASSERT_TRUE(sids);
  EXPECT_TRUE(
      GrantAccessToPath(path, *sids, FILE_GENERIC_READ, NO_INHERITANCE, true));
  EXPECT_EQ(kTest1Dacl, GetFileDacl(path));
  auto sids2 = Sid::FromSddlStringVector({L"S-1-5-11", L"BA"});
  ASSERT_TRUE(sids2);
  EXPECT_TRUE(
      GrantAccessToPath(path, *sids2, GENERIC_ALL, NO_INHERITANCE, true));
  EXPECT_EQ(kTest2Dacl, GetFileDacl(path));
}

TEST(SecurityUtilTest, GrantAccessToPathFileNoInherit) {
  ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  FilePath path = temp_dir.GetPath().Append(L"test");
  ASSERT_TRUE(CreateWithDacl(path, kBaseDacl, false));
  EXPECT_EQ(kBaseDacl, GetFileDacl(path));
  EXPECT_TRUE(
      GrantAccessToPath(path, {}, FILE_GENERIC_READ, NO_INHERITANCE, false));
  EXPECT_EQ(kBaseDacl, GetFileDacl(path));
  auto sids = Sid::FromSddlStringVector({kAuthenticatedUsersSid});
  ASSERT_TRUE(sids);
  EXPECT_TRUE(
      GrantAccessToPath(path, *sids, FILE_GENERIC_READ, NO_INHERITANCE, false));
  EXPECT_EQ(kTest1DaclNoInherit, GetFileDacl(path));
  auto sids2 = Sid::FromSddlStringVector({L"S-1-5-11", L"BA"});
  ASSERT_TRUE(sids2);
  EXPECT_TRUE(
      GrantAccessToPath(path, *sids2, GENERIC_ALL, NO_INHERITANCE, false));
  EXPECT_EQ(kTest2DaclNoInherit, GetFileDacl(path));
}

TEST(SecurityUtilTest, DenyAccessToPathFile) {
  ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  FilePath path = temp_dir.GetPath().Append(L"test");
  ASSERT_TRUE(CreateWithDacl(path, kBaseDacl, false));
  EXPECT_EQ(kBaseDacl, GetFileDacl(path));
  EXPECT_TRUE(
      DenyAccessToPath(path, {}, FILE_GENERIC_READ, NO_INHERITANCE, true));
  EXPECT_EQ(kBaseDacl, GetFileDacl(path));
  auto sids = Sid::FromSddlStringVector({kLocalGuestSid});
  ASSERT_TRUE(sids);
  EXPECT_TRUE(
      DenyAccessToPath(path, *sids, FILE_GENERIC_READ, NO_INHERITANCE, true));
  EXPECT_EQ(kTest1DenyDacl, GetFileDacl(path));
}

TEST(SecurityUtilTest, DenyAccessToPathFileMultiple) {
  ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  FilePath path = temp_dir.GetPath().Append(L"test");
  ASSERT_TRUE(CreateWithDacl(path, kBaseDacl, false));
  EXPECT_EQ(kBaseDacl, GetFileDacl(path));
  auto sids = Sid::FromSddlStringVector({kLocalGuestSid});
  ASSERT_TRUE(sids);
  EXPECT_TRUE(
      DenyAccessToPath(path, *sids, FILE_GENERIC_READ, NO_INHERITANCE, true));
  // Verify setting same ACE on same file does not change the ACL.
  EXPECT_TRUE(
      DenyAccessToPath(path, *sids, FILE_GENERIC_READ, NO_INHERITANCE, true));
  EXPECT_TRUE(
      DenyAccessToPath(path, *sids, FILE_GENERIC_READ, NO_INHERITANCE, true));
  EXPECT_EQ(kTest1DenyDacl, GetFileDacl(path));
}

TEST(SecurityUtilTest, GrantAccessToPathDirectory) {
  ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  FilePath path = temp_dir.GetPath().Append(L"testdir");
  ASSERT_TRUE(CreateWithDacl(path, kBaseDirDacl, true));
  EXPECT_EQ(kBaseDirDacl, GetFileDacl(path));
  FilePath file_path = path.Append(L"test");
  File file(file_path, File::FLAG_CREATE_ALWAYS | File::FLAG_WRITE);
  ASSERT_TRUE(file.IsValid());
  file.Close();
  EXPECT_EQ(kTest1InheritedDacl, GetFileDacl(file_path));
  auto sids = Sid::FromSddlStringVector({kAuthenticatedUsersSid});
  ASSERT_TRUE(sids);
  EXPECT_TRUE(GrantAccessToPath(path, *sids, FILE_GENERIC_READ,
                                OBJECT_INHERIT_ACE | CONTAINER_INHERIT_ACE,
                                true));
  EXPECT_EQ(kBaseDir2Dacl, GetFileDacl(path));
  EXPECT_EQ(kTest2InheritedDacl, GetFileDacl(file_path));
}

TEST(SecurityUtilTest, GrantAccessToPathDirectoryNoInherit) {
  ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  FilePath path = temp_dir.GetPath().Append(L"testdir");
  ASSERT_TRUE(CreateWithDacl(path, kBaseDirDacl, true));
  EXPECT_EQ(kBaseDirDacl, GetFileDacl(path));
  FilePath file_path = path.Append(L"test");
  File file(file_path, File::FLAG_CREATE_ALWAYS | File::FLAG_WRITE);
  ASSERT_TRUE(file.IsValid());
  file.Close();
  EXPECT_EQ(kTest1InheritedDacl, GetFileDacl(file_path));
  auto sids = Sid::FromSddlStringVector({kAuthenticatedUsersSid});
  ASSERT_TRUE(sids);
  EXPECT_TRUE(GrantAccessToPath(path, *sids, FILE_GENERIC_READ,
                                OBJECT_INHERIT_ACE | CONTAINER_INHERIT_ACE,
                                false));
  EXPECT_EQ(kBaseDir2DaclNoInherit, GetFileDacl(path));
  EXPECT_EQ(kTest2InheritedDaclNoInherit, GetFileDacl(file_path));

  FilePath file_path2 = path.Append(L"test2");
  File file2(file_path2, File::FLAG_CREATE_ALWAYS | File::FLAG_WRITE);
  ASSERT_TRUE(file2.IsValid());
  file2.Close();
  EXPECT_EQ(kTest3InheritedDacl, GetFileDacl(file_path2));
}

TEST(SecurityUtilTest, CloneSidVector) {
  std::vector<Sid> sids =
      Sid::FromKnownSidVector({WellKnownSid::kNull, WellKnownSid::kWorld});
  std::vector<Sid> clone = CloneSidVector(sids);
  ASSERT_EQ(sids.size(), clone.size());
  for (size_t index = 0; index < sids.size(); ++index) {
    ASSERT_EQ(sids[index], clone[index]);
    ASSERT_NE(sids[index].GetPSID(), clone[index].GetPSID());
  }
  ASSERT_EQ(CloneSidVector(std::vector<Sid>()).size(), 0U);
}

TEST(SecurityUtilTest, AppendSidVector) {
  std::vector<Sid> sids =
      Sid::FromKnownSidVector({WellKnownSid::kNull, WellKnownSid::kWorld});

  std::vector<Sid> total_sids;
  AppendSidVector(total_sids, sids);
  EXPECT_EQ(total_sids.size(), sids.size());

  std::vector<Sid> sids2 = Sid::FromKnownSidVector(
      {WellKnownSid::kCreatorOwner, WellKnownSid::kNetwork});
  AppendSidVector(total_sids, sids2);
  EXPECT_EQ(total_sids.size(), sids.size() + sids2.size());

  auto sid_interator = total_sids.cbegin();
  for (size_t index = 0; index < sids.size(); ++index) {
    ASSERT_EQ(*sid_interator, sids[index]);
    ASSERT_NE(sid_interator->GetPSID(), sids[index].GetPSID());
    sid_interator++;
  }
  for (size_t index = 0; index < sids2.size(); ++index) {
    ASSERT_EQ(*sid_interator, sids2[index]);
    ASSERT_NE(sid_interator->GetPSID(), sids2[index].GetPSID());
    sid_interator++;
  }
}

TEST(SecurityUtilTest, GetGrantedAccess) {
  EXPECT_FALSE(GetGrantedAccess(nullptr));
  ScopedHandle handle(::CreateMutexEx(nullptr, nullptr, 0, MUTEX_MODIFY_STATE));
  EXPECT_EQ(GetGrantedAccess(handle.get()), DWORD{MUTEX_MODIFY_STATE});
  handle.Set(::CreateMutexEx(nullptr, nullptr, 0, READ_CONTROL));
  EXPECT_EQ(GetGrantedAccess(handle.get()), DWORD{READ_CONTROL});
  handle.Set(::CreateMutexEx(nullptr, nullptr, 0, GENERIC_ALL));
  EXPECT_EQ(GetGrantedAccess(handle.get()), DWORD{MUTEX_ALL_ACCESS});
}

}  // namespace win
}  // namespace base
