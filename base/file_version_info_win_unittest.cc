// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "base/file_version_info_win.h"

#include <windows.h>

#include <stddef.h>

#include <memory>

#include "base/file_version_info.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/scoped_native_library.h"
#include "base/strings/string_util.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::FilePath;

namespace {

FilePath GetTestDataPath() {
  FilePath path;
  base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &path);
  path = path.AppendASCII("base");
  path = path.AppendASCII("test");
  path = path.AppendASCII("data");
  path = path.AppendASCII("file_version_info_unittest");
  return path;
}

class FileVersionInfoFactory {
 public:
  explicit FileVersionInfoFactory(const FilePath& path) : path_(path) {}
  FileVersionInfoFactory(const FileVersionInfoFactory&) = delete;
  FileVersionInfoFactory& operator=(const FileVersionInfoFactory&) = delete;

  std::unique_ptr<FileVersionInfo> Create() const {
    return FileVersionInfo::CreateFileVersionInfo(path_);
  }

 private:
  const FilePath path_;
};

class FileVersionInfoForModuleFactory {
 public:
  explicit FileVersionInfoForModuleFactory(const FilePath& path)
      // Load the library with LOAD_LIBRARY_AS_IMAGE_RESOURCE since it shouldn't
      // be executed.
      : library_(::LoadLibraryEx(path.value().c_str(),
                                 nullptr,
                                 LOAD_LIBRARY_AS_IMAGE_RESOURCE)) {
    EXPECT_TRUE(library_.is_valid());
  }
  FileVersionInfoForModuleFactory(const FileVersionInfoForModuleFactory&) =
      delete;
  FileVersionInfoForModuleFactory& operator=(
      const FileVersionInfoForModuleFactory&) = delete;

  std::unique_ptr<FileVersionInfo> Create() const {
    return FileVersionInfo::CreateFileVersionInfoForModule(library_.get());
  }

 private:
  const base::ScopedNativeLibrary library_;
};

template <typename T>
class FileVersionInfoTest : public testing::Test {};

using FileVersionInfoFactories =
    ::testing::Types<FileVersionInfoFactory, FileVersionInfoForModuleFactory>;

}  // namespace

TYPED_TEST_SUITE(FileVersionInfoTest, FileVersionInfoFactories);

TYPED_TEST(FileVersionInfoTest, HardCodedProperties) {
  const base::FilePath::CharType kDLLName[] =
      FILE_PATH_LITERAL("FileVersionInfoTest1.dll");

  const wchar_t* const kExpectedValues[15] = {
      // FileVersionInfoTest.dll
      L"Goooooogle",                      // company_name
      L"Google",                          // company_short_name
      L"This is the product name",        // product_name
      L"This is the product short name",  // product_short_name
      L"The Internal Name",               // internal_name
      L"4.3.2.1",                         // product_version
      L"Special build property",          // special_build
      L"This is the original filename",   // original_filename
      L"This is my file description",     // file_description
      L"1.2.3.4",                         // file_version
  };

  FilePath dll_path = GetTestDataPath();
  dll_path = dll_path.Append(kDLLName);

  TypeParam factory(dll_path);
  std::unique_ptr<FileVersionInfo> version_info(factory.Create());
  ASSERT_TRUE(version_info);

  int j = 0;
  EXPECT_EQ(kExpectedValues[j++],
            base::AsWStringView(version_info->company_name()));
  EXPECT_EQ(kExpectedValues[j++],
            base::AsWStringView(version_info->company_short_name()));
  EXPECT_EQ(kExpectedValues[j++],
            base::AsWStringView(version_info->product_name()));
  EXPECT_EQ(kExpectedValues[j++],
            base::AsWStringView(version_info->product_short_name()));
  EXPECT_EQ(kExpectedValues[j++],
            base::AsWStringView(version_info->internal_name()));
  EXPECT_EQ(kExpectedValues[j++],
            base::AsWStringView(version_info->product_version()));
  EXPECT_EQ(kExpectedValues[j++],
            base::AsWStringView(version_info->special_build()));
  EXPECT_EQ(kExpectedValues[j++],
            base::AsWStringView(version_info->original_filename()));
  EXPECT_EQ(kExpectedValues[j++],
            base::AsWStringView(version_info->file_description()));
  EXPECT_EQ(kExpectedValues[j++],
            base::AsWStringView(version_info->file_version()));
}

TYPED_TEST(FileVersionInfoTest, CustomProperties) {
  FilePath dll_path = GetTestDataPath();
  dll_path = dll_path.AppendASCII("FileVersionInfoTest1.dll");

  TypeParam factory(dll_path);
  std::unique_ptr<FileVersionInfo> version_info(factory.Create());
  ASSERT_TRUE(version_info);

  // Test few existing properties.
  std::u16string str;
  FileVersionInfoWin* version_info_win =
      static_cast<FileVersionInfoWin*>(version_info.get());
  EXPECT_TRUE(version_info_win->GetValue(u"Custom prop 1", &str));
  EXPECT_EQ(u"Un", str);
  EXPECT_EQ(u"Un", version_info_win->GetStringValue(u"Custom prop 1"));

  EXPECT_TRUE(version_info_win->GetValue(u"Custom prop 2", &str));
  EXPECT_EQ(u"Deux", str);
  EXPECT_EQ(u"Deux", version_info_win->GetStringValue(u"Custom prop 2"));

  EXPECT_TRUE(version_info_win->GetValue(u"Custom prop 3", &str));
  EXPECT_EQ(u"1600 Amphitheatre Parkway Mountain View, CA 94043", str);
  EXPECT_EQ(u"1600 Amphitheatre Parkway Mountain View, CA 94043",
            version_info_win->GetStringValue(u"Custom prop 3"));

  // Test an non-existing property.
  EXPECT_FALSE(version_info_win->GetValue(u"Unknown property", &str));
  EXPECT_EQ(std::u16string(),
            version_info_win->GetStringValue(u"Unknown property"));

  EXPECT_EQ(base::Version(std::vector<uint32_t>{1, 0, 0, 1}),
            version_info_win->GetFileVersion());
}

TYPED_TEST(FileVersionInfoTest, NoVersionInfo) {
  FilePath dll_path = GetTestDataPath();
  dll_path = dll_path.AppendASCII("no_version_info.dll");

  TypeParam factory(dll_path);
  ASSERT_FALSE(factory.Create());
}
