// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/native_library.h"

#include "base/check.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>
#endif

namespace base {

const FilePath::CharType kDummyLibraryPath[] =
    FILE_PATH_LITERAL("dummy_library");

TEST(NativeLibraryTest, LoadFailure) {
  NativeLibraryLoadError error;
  EXPECT_FALSE(LoadNativeLibrary(FilePath(kDummyLibraryPath), &error));
  EXPECT_FALSE(error.ToString().empty());
}

// |error| is optional and can be null.
TEST(NativeLibraryTest, LoadFailureWithNullError) {
  EXPECT_FALSE(LoadNativeLibrary(FilePath(kDummyLibraryPath), nullptr));
}

TEST(NativeLibraryTest, LoadWithOptionsFailure) {
  NativeLibraryLoadError error;
  NativeLibraryOptions options;
  EXPECT_FALSE(LoadNativeLibraryWithOptions(FilePath(kDummyLibraryPath),
                                            options, &error));
  EXPECT_FALSE(error.ToString().empty());
}

#if BUILDFLAG(IS_FUCHSIA)
TEST(NativeLibraryTest, LoadAbsolutePath) {
  EXPECT_TRUE(LoadNativeLibrary(FilePath("/pkg/lib/libtest_shared_library.so"),
                                nullptr));
}

TEST(NativeLibraryTest, LoadAbsolutePath_OutsideLibraryRoot) {
  NativeLibraryLoadError error;
  EXPECT_FALSE(LoadNativeLibrary(FilePath("/pkg/tmp/libtest_shared_library.so"),
                                 &error));
  std::string expected_error =
      "Absolute library paths must begin with /pkg/lib";
  EXPECT_EQ(error.ToString(), expected_error);
}
#endif

TEST(NativeLibraryTest, GetNativeLibraryName) {
  const char kExpectedName[] =
#if BUILDFLAG(IS_WIN)
      "mylib.dll";
#elif BUILDFLAG(IS_IOS)
      "mylib.framework/mylib";
#elif BUILDFLAG(IS_MAC)
      "libmylib.dylib";
#elif BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
      "libmylib.so";
#endif
  EXPECT_EQ(kExpectedName, GetNativeLibraryName("mylib"));
}

TEST(NativeLibraryTest, GetLoadableModuleName) {
  const char kExpectedName[] =
#if BUILDFLAG(IS_WIN)
      "mylib.dll";
#elif BUILDFLAG(IS_IOS)
      "mylib.framework";
#elif BUILDFLAG(IS_MAC)
      "mylib.so";
#elif BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
      "libmylib.so";
#endif
  EXPECT_EQ(kExpectedName, GetLoadableModuleName("mylib"));
}

// ASAN will complain about our intentional ODR violation because of
// |g_native_library_exported_value| being defined globally both here
// and in the shared library.
#if !defined(ADDRESS_SANITIZER)

const char kTestLibraryName[] =
#if BUILDFLAG(IS_WIN)
    "test_shared_library.dll";
#elif BUILDFLAG(IS_IOS)
    "Frameworks/test_shared_library_ios.framework/test_shared_library_ios";
#elif BUILDFLAG(IS_MAC)
    "libtest_shared_library.dylib";
#elif BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
    "libtest_shared_library.so";
#endif

class TestLibrary {
 public:
  TestLibrary() : library_(nullptr) {
    base::FilePath exe_path;

#if !BUILDFLAG(IS_FUCHSIA)
    // Libraries do not sit alongside the executable in Fuchsia. NativeLibrary
    // is aware of this and is able to resolve library paths correctly.
    CHECK(base::PathService::Get(base::DIR_EXE, &exe_path));
#endif

    library_ =
        LoadNativeLibrary(exe_path.AppendASCII(kTestLibraryName), nullptr);
    CHECK(library_);
  }

  TestLibrary(const TestLibrary&) = delete;
  TestLibrary& operator=(const TestLibrary&) = delete;
  ~TestLibrary() { UnloadNativeLibrary(library_); }

  template <typename ReturnType, typename... Args>
  ReturnType Call(const char* function_name, Args... args) {
    return reinterpret_cast<ReturnType (*)(Args...)>(
        GetFunctionPointerFromNativeLibrary(library_, function_name))(args...);
  }

 private:
  NativeLibrary library_;
};

// NativeLibraaryTest.LoadLibrary is failing on M tablets only.
// crbug/641309
#if !BUILDFLAG(IS_ANDROID)

// Verifies that we can load a native library and resolve its exported symbols.
TEST(NativeLibraryTest, LoadLibrary) {
  TestLibrary library;
  EXPECT_EQ(5, library.Call<int>("GetSimpleTestValue"));
}

#if BUILDFLAG(IS_WIN)

// Verifies that loading an unsigned test library succeeds when signature
// verification is disabled (`verify_signature = false`).
TEST(NativeLibraryTest, LoadWithOptions_VerifySignatureDisabled) {
  base::FilePath exe_path;
  CHECK(base::PathService::Get(base::DIR_EXE, &exe_path));
  base::FilePath library_path = exe_path.AppendASCII(kTestLibraryName);

  NativeLibraryLoadError error;
  NativeLibraryOptions options;
  options.verify_signature = false;
  NativeLibrary library =
      LoadNativeLibraryWithOptions(library_path, options, &error);
  EXPECT_TRUE(library);
  if (library) {
    UnloadNativeLibrary(library);
  }
}

// Verifies that loading an unsigned test library fails with
// `TRUST_E_SUBJECT_NOT_TRUSTED` when signature verification is explicitly
// forced (`force_verify_in_dev_builds = true`).
TEST(NativeLibraryTest, LoadWithOptions_VerifySignatureForcedFailure) {
  base::FilePath exe_path;
  CHECK(base::PathService::Get(base::DIR_EXE, &exe_path));
  base::FilePath library_path = exe_path.AppendASCII(kTestLibraryName);

  NativeLibraryLoadError error;
  NativeLibraryOptions options;
  options.verify_signature = true;
  options.force_verify_in_dev_builds = true;
  NativeLibrary library =
      LoadNativeLibraryWithOptions(library_path, options, &error);
  EXPECT_FALSE(library);
  EXPECT_EQ(error.code, static_cast<DWORD>(TRUST_E_SUBJECT_NOT_TRUSTED));
}

// Verifies that with default signature verification
// (`force_verify_in_dev_builds = false`), official release branded builds
// enforce verification by default (rejecting the unsigned library with
// `TRUST_E_SUBJECT_NOT_TRUSTED`), whereas non-branded or debug builds bypass
// verification by default (allowing the library to load).
TEST(NativeLibraryTest, LoadWithOptions_VerifySignatureDefault) {
  base::FilePath exe_path;
  CHECK(base::PathService::Get(base::DIR_EXE, &exe_path));
  base::FilePath library_path = exe_path.AppendASCII(kTestLibraryName);

  NativeLibraryLoadError error;
  NativeLibraryOptions options;
  options.verify_signature = true;
  options.force_verify_in_dev_builds = false;
  NativeLibrary library =
      LoadNativeLibraryWithOptions(library_path, options, &error);
#if BUILDFLAG(GOOGLE_CHROME_BRANDING) && defined(NDEBUG)
  EXPECT_FALSE(library);
  EXPECT_EQ(error.code, static_cast<DWORD>(TRUST_E_SUBJECT_NOT_TRUSTED));
#else
  EXPECT_TRUE(library);
  if (library) {
    UnloadNativeLibrary(library);
  }
#endif
}

#endif  // BUILDFLAG(IS_WIN)

#endif  // !BUILDFLAG(IS_ANDROID)

#endif  // !defined(ADDRESS_SANITIZER)

}  // namespace base
