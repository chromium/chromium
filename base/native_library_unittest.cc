// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_path.h"
#include "base/native_library.h"
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

TEST(NativeLibraryTest, GetNativeLibraryName) {
  const char kExpectedName[] =
#if defined(OS_WIN)
      "mylib.dll";
#elif defined(OS_IOS)
      "mylib";
#elif defined(OS_MAC)
      "libmylib.dylib";
#elif defined(OS_POSIX) || defined(OS_FUCHSIA)
      "libmylib.so";
#endif
  EXPECT_EQ(kExpectedName, GetNativeLibraryName("mylib"));
}

TEST(NativeLibraryTest, GetLoadableModuleName) {
  const char kExpectedName[] =
#if defined(OS_WIN)
      "mylib.dll";
#elif defined(OS_IOS)
      "mylib";
#elif defined(OS_MAC)
      "mylib.so";
#elif defined(OS_POSIX) || defined(OS_FUCHSIA)
      "libmylib.so";
#endif
  EXPECT_EQ(kExpectedName, GetLoadableModuleName("mylib"));
}

// We don't support dynamic loading on iOS, and ASAN will complain about our
// intentional ODR violation because of |g_native_library_exported_value| being
// defined globally both here and in the shared library.
#if !defined(OS_IOS) && !defined(ADDRESS_SANITIZER)

const char kTestLibraryName[] =
#if defined(OS_WIN)
    "test_shared_library.dll";
#elif defined(OS_MAC)
    "libtest_shared_library.dylib";
#elif defined(OS_ANDROID) && defined(COMPONENT_BUILD)
    "libtest_shared_library.cr.so";
#elif defined(OS_POSIX) || defined(OS_FUCHSIA)
    "libtest_shared_library.so";
#endif

class TestLibrary {
 public:
  TestLibrary() : TestLibrary(NativeLibraryOptions()) {}

  explicit TestLibrary(const NativeLibraryOptions& options)
    : library_(nullptr) {
    base::FilePath exe_path;

#if !defined(OS_FUCHSIA)
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
  ~TestLibrary() {
    UnloadNativeLibrary(library_);
  }

  template <typename ReturnType, typename... Args>
  ReturnType Call(const char* function_name, Args... args) {
    return reinterpret_cast<ReturnType(*)(Args...)>(
        GetFunctionPointerFromNativeLibrary(library_, function_name))(args...);
  }

 private:
  NativeLibrary library_;
};

// NativeLibraaryTest.LoadLibrary is failing on M tablets only.
// crbug/641309
#if !defined(OS_ANDROID)

// Verifies that we can load a native library and resolve its exported symbols.
TEST(NativeLibraryTest, LoadLibrary) {
  TestLibrary library;
  EXPECT_EQ(5, library.Call<int>("GetSimpleTestValue"));
}

#endif  // !defined(OS_ANDROID)

// Android dlopen() requires further investigation, as it might vary across
// versions with respect to symbol resolution scope.
// TSan and MSan error out on RTLD_DEEPBIND, https://crbug.com/705255
#if !defined(OS_ANDROID) && !defined(THREAD_SANITIZER) && \
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
  EXPECT_EQ(1, library.Call<int>("GetIncrementValue"));
  EXPECT_EQ(1, NativeLibraryTestIncrement());
  EXPECT_EQ(2, library.Call<int>("GetIncrementValue"));
  EXPECT_EQ(3, library.Call<int>("GetIncrementValue"));
  EXPECT_EQ(4, library.Call<int>("NativeLibraryTestIncrement"));
  EXPECT_EQ(2, NativeLibraryTestIncrement());
  EXPECT_EQ(3, NativeLibraryTestIncrement());
}

#endif  // !defined(OS_ANDROID)

#endif  // !defined(OS_IOS) && !defined(ADDRESS_SANITIZER)

}  // namespace base
