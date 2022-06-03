// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/win/security_util.h"

#include <aclapi.h>
#include <sddl.h>
#include <windows.h>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
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

std::wstring GetFileDacl(const FilePath& path) {
  PSECURITY_DESCRIPTOR sd;
  if (::GetNamedSecurityInfo(path.value().c_str(), SE_FILE_OBJECT,
                             DACL_SECURITY_INFORMATION, nullptr, nullptr,
                             nullptr, nullptr, &sd) != ERROR_SUCCESS) {
    return std::wstring();
  }
  auto sd_ptr = TakeLocalAlloc(sd);
  LPWSTR sddl;
  if (!::ConvertSecurityDescriptorToStringSecurityDescriptor(
          sd_ptr.get(), SDDL_REVISION_1, DACL_SECURITY_INFORMATION, &sddl,
          nullptr)) {
    return std::wstring();
  }
  return TakeLocalAlloc(sddl).get();
}

bool CreateWithDacl(const FilePath& path, const wchar_t* sddl, bool directory) {
  PSECURITY_DESCRIPTOR sd;
  if (!::ConvertStringSecurityDescriptorToSecurityDescriptor(
          sddl, SDDL_REVISION_1, &sd, nullptr)) {
    return false;
  }
  auto sd_ptr = TakeLocalAlloc(sd);
  SECURITY_ATTRIBUTES security_attr = {};
  security_attr.nLength = sizeof(security_attr);
  security_attr.lpSecurityDescriptor = sd_ptr.get();
  if (directory)
    return !!::CreateDirectory(path.value().c_str(), &security_attr);

  return ScopedHandle(::CreateFile(path.value().c_str(), GENERIC_ALL, 0,
                                   &security_attr, CREATE_ALWAYS, 0, nullptr))
      .IsValid();
}

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

}  // namespace win
}  // namespace base