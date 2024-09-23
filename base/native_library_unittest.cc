// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/native_library.h"

#include "base/check.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/test/native_library_test_utils.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

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
      "Frameworks/mylib.framework/mylib";
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
      "Frameworks/mylib.framework";
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
  TestLibrary() : TestLibrary(NativeLibraryOptions()) {}

  explicit TestLibrary(const NativeLibraryOptions& options)
      : library_(nullptr) {
    base::FilePath exe_path;

#if !BUILDFLAG(IS_FUCHSIA)
    // Libraries do not sit alongside the executable in Fuchsia. NativeLibrary
    // is aware of this and is able to resolve library paths correctly.
    CHECK(base::PathService::Get(base::DIR_EXE, &exe_path));
#endif

    library_ = LoadNativeLibraryWithOptions(
        exe_path.AppendASCII(kTestLibraryName), options, nullptr);
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

#endif  // !BUILDFLAG(IS_ANDROID)

// Android dlopen() requires further investigation, as it might vary across
// versions with respect to symbol resolution scope.
// TSan and MSan error out on RTLD_DEEPBIND, https://crbug.com/705255
#if !BUILDFLAG(IS_ANDROID) && !defined(THREAD_SANITIZER) && \
    !defined(MEMORY_SANITIZER)

// Verifies that the |prefer_own_symbols| option satisfies its guarantee that
// a loaded library will always prefer local symbol resolution before
// considering global symbols.
TEST(NativeLibraryTest, LoadLibraryPreferOwnSymbols) {
  NativeLibraryOptions options;
  options.prefer_own_symbols = true;
  TestLibrary library(options);

  // Verify that this binary and the DSO use different storage for
  // |g_native_library_exported_value|.
  g_native_library_exported_value = 1;
  library.Call<void>("SetExportedValue", 2);
  EXPECT_EQ(1, g_native_library_exported_value);
  g_native_library_exported_value = 3;
  EXPECT_EQ(2, library.Call<int>("GetExportedValue"));

  // Both this binary and the library link against the
  // native_library_test_utils source library, which in turn exports the
  // NativeLibraryTestIncrement() function whose return value depends on some
  // static internal state.
  //
  // The DSO's GetIncrementValue() forwards to that function inside the DSO.
  //
  // Here we verify that direct calls to NativeLibraryTestIncrement() in this
  // binary return a sequence of values independent from the sequence returned
  // by GetIncrementValue(), ensuring that the DSO is calling its own local
  // definition of NativeLibraryTestIncrement().

  // Ensure that the counter starts at the expected value (0).
  library.Call<void>("NativeLibraryResetCounter");
  NativeLibraryResetCounter();

  EXPECT_EQ(1, library.Call<int>("GetIncrementValue"));
  EXPECT_EQ(1, NativeLibraryTestIncrement());
  EXPECT_EQ(2, library.Call<int>("GetIncrementValue"));
  EXPECT_EQ(3, library.Call<int>("GetIncrementValue"));
  EXPECT_EQ(4, library.Call<int>("NativeLibraryTestIncrement"));
  EXPECT_EQ(2, NativeLibraryTestIncrement());
  EXPECT_EQ(3, NativeLibraryTestIncrement());
}

#endif  // !BUILDFLAG(IS_ANDROID) && !defined(THREAD_SANITIZER) && \
        // !defined(MEMORY_SANITIZER)

#endif  // !defined(ADDRESS_SANITIZER)

}  // namespace base
