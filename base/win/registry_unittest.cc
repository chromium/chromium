// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "base/win/registry.h"

#include <windows.h>

#include <shlobj.h>
#include <stdint.h>

#include <cstring>
#include <iterator>
#include <string_view>
#include <utility>

#include "base/compiler_specific.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/test/bind.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/test/test_reg_util_win.h"
#include "base/threading/simple_thread.h"
#include "base/win/win_util.h"
#include "base/win/windows_version.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base::win {

namespace {

constexpr wchar_t kRootKey[] = L"Base_Registry_Unittest";

// A test harness for registry tests that operate in HKCU. Each test is given
// a valid key distinct from that used by other tests.
class RegistryTest : public testing::Test {
 protected:
  RegistryTest() : root_key_(std::wstring(L"Software\\") + kRootKey) {}

  void SetUp() override {
    ASSERT_NO_FATAL_FAILURE(registry_override_.OverrideRegistry(
        HKEY_CURRENT_USER, &override_path_));

    // Create the test's root key.
    RegKey key(HKEY_CURRENT_USER, L"", KEY_CREATE_SUB_KEY);
    ASSERT_NE(ERROR_SUCCESS,
              key.Open(HKEY_CURRENT_USER, root_key().c_str(), KEY_READ));
    ASSERT_EQ(ERROR_SUCCESS,
              key.Create(HKEY_CURRENT_USER, root_key().c_str(), KEY_READ));
  }

  // Returns the path to a key under HKCU that is made available for exclusive
  // use by a test.
  const std::wstring& root_key() const { return root_key_; }

  const std::wstring& override_path() const { return override_path_; }

 private:
  registry_util::RegistryOverrideManager registry_override_;
  const std::wstring root_key_;
  std::wstring override_path_;
};

}  // namespace

TEST_F(RegistryTest, ValueTest) {
  RegKey key;

  ASSERT_EQ(ERROR_SUCCESS, key.Open(HKEY_CURRENT_USER, root_key().c_str(),
                                    KEY_READ | KEY_SET_VALUE));
  ASSERT_TRUE(key.Valid());

  const wchar_t kStringValueName[] = L"StringValue";
  const wchar_t kDWORDValueName[] = L"DWORDValue";
  const wchar_t kInt64ValueName[] = L"Int64Value";
  const wchar_t kStringData[] = L"string data";
  const DWORD kDWORDData = 0xdeadbabe;
  const int64_t kInt64Data = 0xdeadbabedeadbabeLL;

  // Test value creation
  ASSERT_EQ(ERROR_SUCCESS, key.WriteValue(kStringValueName, kStringData));
  ASSERT_EQ(ERROR_SUCCESS, key.WriteValue(kDWORDValueName, kDWORDData));
  ASSERT_EQ(ERROR_SUCCESS, key.WriteValue(kInt64ValueName, &kInt64Data,
                                          sizeof(kInt64Data), REG_QWORD));
  EXPECT_THAT(key.GetValueCount(), base::test::ValueIs(3U));
  EXPECT_TRUE(key.HasValue(kStringValueName));
  EXPECT_TRUE(key.HasValue(kDWORDValueName));
  EXPECT_TRUE(key.HasValue(kInt64ValueName));

  // Test Read
  std::wstring string_value;
  DWORD dword_value = 0;
  int64_t int64_value = 0;
  ASSERT_EQ(ERROR_SUCCESS, key.ReadValue(kStringValueName, &string_value));
  ASSERT_EQ(ERROR_SUCCESS, key.ReadValueDW(kDWORDValueName, &dword_value));
  ASSERT_EQ(ERROR_SUCCESS, key.ReadInt64(kInt64ValueName, &int64_value));
  EXPECT_EQ(kStringData, string_value);
  EXPECT_EQ(kDWORDData, dword_value);
  EXPECT_EQ(kInt64Data, int64_value);

  // Make sure out args are not touched if ReadValue fails
  const wchar_t* kNonExistent = L"NonExistent";
  ASSERT_NE(ERROR_SUCCESS, key.ReadValue(kNonExistent, &string_value));
  ASSERT_NE(ERROR_SUCCESS, key.ReadValueDW(kNonExistent, &dword_value));
  ASSERT_NE(ERROR_SUCCESS, key.ReadInt64(kNonExistent, &int64_value));
  EXPECT_EQ(kStringData, string_value);
  EXPECT_EQ(kDWORDData, dword_value);
  EXPECT_EQ(kInt64Data, int64_value);

  // Test delete
  ASSERT_EQ(ERROR_SUCCESS, key.DeleteValue(kStringValueName));
  ASSERT_EQ(ERROR_SUCCESS, key.DeleteValue(kDWORDValueName));
  ASSERT_EQ(ERROR_SUCCESS, key.DeleteValue(kInt64ValueName));
  EXPECT_THAT(key.GetValueCount(), base::test::ValueIs(0U));
  EXPECT_FALSE(key.HasValue(kStringValueName));
  EXPECT_FALSE(key.HasValue(kDWORDValueName));
  EXPECT_FALSE(key.HasValue(kInt64ValueName));
}

TEST_F(RegistryTest, BigValueIteratorTest) {
  RegKey key;
  ASSERT_EQ(ERROR_SUCCESS, key.Open(HKEY_CURRENT_USER, root_key().c_str(),
                                    KEY_READ | KEY_SET_VALUE));
  ASSERT_TRUE(key.Valid());

  // Create a test value that is larger than MAX_PATH.
  std::wstring data(MAX_PATH * 2, 'a');

  ASSERT_EQ(ERROR_SUCCESS, key.WriteValue(data.c_str(), data.c_str()));

  RegistryValueIterator iterator(HKEY_CURRENT_USER, root_key().c_str());
  ASSERT_TRUE(iterator.Valid());
  EXPECT_EQ(data, iterator.Name());
  EXPECT_EQ(data, iterator.Value());
  // ValueSize() is in bytes, including NUL.
  EXPECT_EQ((MAX_PATH * 2 + 1) * sizeof(wchar_t), iterator.ValueSize());
  ++iterator;
  EXPECT_FALSE(iterator.Valid());
}

TEST_F(RegistryTest, TruncatedCharTest) {
  RegKey key;
  ASSERT_EQ(ERROR_SUCCESS, key.Open(HKEY_CURRENT_USER, root_key().c_str(),
                                    KEY_READ | KEY_SET_VALUE));
  ASSERT_TRUE(key.Valid());

  const wchar_t kName[] = L"name";
  // kData size is not a multiple of sizeof(wchar_t).
  const uint8_t kData[] = {1, 2, 3, 4, 5};
  EXPECT_EQ(5u, std::size(kData));
  ASSERT_EQ(ERROR_SUCCESS,
            key.WriteValue(kName, kData, std::size(kData), REG_BINARY));

  RegistryValueIterator iterator(HKEY_CURRENT_USER, root_key().c_str());
  ASSERT_TRUE(iterator.Valid());
  // Avoid having to use EXPECT_STREQ here by leveraging std::string_view's
  // operator== to perform a deep comparison.
  EXPECT_EQ(std::wstring_view(kName), std::wstring_view(iterator.Name()));
  // ValueSize() is in bytes.
  ASSERT_EQ(std::size(kData), iterator.ValueSize());
  // Value() is NUL terminated.
  int end = (iterator.ValueSize() + sizeof(wchar_t) - 1) / sizeof(wchar_t);
  EXPECT_NE('\0', iterator.Value()[end - 1]);
  EXPECT_EQ('\0', iterator.Value()[end]);
  EXPECT_EQ(0, std::memcmp(kData, iterator.Value(), std::size(kData)));
  ++iterator;
  EXPECT_FALSE(iterator.Valid());
}

// Tests that the value iterator is okay with an empty key.
TEST_F(RegistryTest, ValueIteratorEmptyKey) {
  RegistryValueIterator iterator(HKEY_CURRENT_USER, root_key().c_str());
  EXPECT_EQ(iterator.ValueCount(), 0U);
  EXPECT_FALSE(iterator.Valid());
}

// Tests that the default value is seen by a value iterator.
TEST_F(RegistryTest, ValueIteratorDefaultValue) {
  const std::wstring_view kTestString(L"i miss you");
  ASSERT_EQ(RegKey(HKEY_CURRENT_USER, root_key().c_str(), KEY_SET_VALUE)
                .WriteValue(nullptr, kTestString.data()),
            ERROR_SUCCESS);
  RegistryValueIterator iterator(HKEY_CURRENT_USER, root_key().c_str());
  EXPECT_EQ(iterator.ValueCount(), 1U);
  ASSERT_TRUE(iterator.Valid());
  EXPECT_EQ(std::wstring_view(iterator.Name()), std::wstring_view());
  EXPECT_EQ(iterator.ValueSize(), (kTestString.size() + 1) * sizeof(wchar_t));
  EXPECT_EQ(iterator.Type(), REG_SZ);
  EXPECT_EQ(std::wstring_view(iterator.Value()), kTestString);
  ++iterator;
  EXPECT_FALSE(iterator.Valid());
}

TEST_F(RegistryTest, NonRecursiveDelete) {
  RegKey key;
  // Create root_key()
  //                  \->Bar (TestValue)
  //                     \->Foo (TestValue)
  ASSERT_EQ(ERROR_SUCCESS, key.Open(HKEY_CURRENT_USER, root_key().c_str(),
                                    KEY_CREATE_SUB_KEY));
  ASSERT_EQ(ERROR_SUCCESS, key.CreateKey(L"Bar", KEY_WRITE));
  ASSERT_EQ(ERROR_SUCCESS, key.WriteValue(L"TestValue", L"TestData"));
  ASSERT_EQ(ERROR_SUCCESS, key.CreateKey(L"Foo", KEY_WRITE));
  ASSERT_EQ(ERROR_SUCCESS, key.WriteValue(L"TestValue", L"TestData"));
  key.Close();

  const std::wstring bar_path = root_key() + L"\\Bar";
  // Non-recursive delete of Bar from root_key() should fail.
  ASSERT_EQ(ERROR_SUCCESS,
            key.Open(HKEY_CURRENT_USER, root_key().c_str(), KEY_QUERY_VALUE));
  ASSERT_NE(ERROR_SUCCESS,
            key.DeleteKey(L"Bar", RegKey::RecursiveDelete(false)));
  key.Close();
  ASSERT_TRUE(
      RegKey(HKEY_CURRENT_USER, bar_path.c_str(), KEY_QUERY_VALUE).Valid());

  // Non-recursive delete of Bar from itself should fail.
  ASSERT_EQ(ERROR_SUCCESS,
            key.Open(HKEY_CURRENT_USER, bar_path.c_str(), KEY_QUERY_VALUE));
  ASSERT_NE(ERROR_SUCCESS, key.DeleteKey(L"", RegKey::RecursiveDelete(false)));
  key.Close();
  ASSERT_TRUE(
      RegKey(HKEY_CURRENT_USER, root_key().c_str(), KEY_QUERY_VALUE).Valid());

  // Non-recursive delete of the subkey and then root_key() should succeed.
  ASSERT_EQ(ERROR_SUCCESS,
            key.Open(HKEY_CURRENT_USER, bar_path.c_str(), KEY_QUERY_VALUE));
  ASSERT_EQ(ERROR_SUCCESS,
            key.DeleteKey(L"Foo", RegKey::RecursiveDelete(false)));
  ASSERT_EQ(ERROR_SUCCESS, key.DeleteKey(L"", RegKey::RecursiveDelete(false)));
  key.Close();
  ASSERT_FALSE(
      RegKey(HKEY_CURRENT_USER, bar_path.c_str(), KEY_QUERY_VALUE).Valid());
}

TEST_F(RegistryTest, RecursiveDelete) {
  RegKey key;
  // Create root_key()
  //                  \->Bar (TestValue)
  //                     \->Foo (TestValue)
  //                        \->Bar
  //                           \->Foo
  //                  \->Moo
  //                  \->Foo
  // and delete root_key()
  std::wstring key_path = root_key();
  ASSERT_EQ(ERROR_SUCCESS,
            key.Open(HKEY_CURRENT_USER, key_path.c_str(), KEY_CREATE_SUB_KEY));
  ASSERT_EQ(ERROR_SUCCESS, key.CreateKey(L"Bar", KEY_WRITE));
  ASSERT_EQ(ERROR_SUCCESS, key.WriteValue(L"TestValue", L"TestData"));
  ASSERT_EQ(ERROR_SUCCESS,
            key.Open(HKEY_CURRENT_USER, key_path.c_str(), KEY_CREATE_SUB_KEY));
  ASSERT_EQ(ERROR_SUCCESS, key.CreateKey(L"Moo", KEY_WRITE));
  ASSERT_EQ(ERROR_SUCCESS,
            key.Open(HKEY_CURRENT_USER, key_path.c_str(), KEY_CREATE_SUB_KEY));
  ASSERT_EQ(ERROR_SUCCESS, key.CreateKey(L"Foo", KEY_WRITE));

  key_path += L"\\Bar";
  ASSERT_EQ(ERROR_SUCCESS,
            key.Open(HKEY_CURRENT_USER, key_path.c_str(), KEY_CREATE_SUB_KEY));
  key_path += L"\\Foo";
  ASSERT_EQ(ERROR_SUCCESS, key.CreateKey(L"Foo", KEY_WRITE));
  ASSERT_EQ(ERROR_SUCCESS, key.WriteValue(L"TestValue", L"TestData"));
  ASSERT_EQ(ERROR_SUCCESS,
            key.Open(HKEY_CURRENT_USER, key_path.c_str(), KEY_READ));

  ASSERT_EQ(ERROR_SUCCESS,
            key.Open(HKEY_CURRENT_USER, key_path.c_str(), KEY_CREATE_SUB_KEY));
  ASSERT_EQ(ERROR_SUCCESS, key.CreateKey(L"Bar", KEY_WRITE));
  ASSERT_EQ(ERROR_SUCCESS, key.CreateKey(L"Foo", KEY_WRITE));
  ASSERT_EQ(ERROR_SUCCESS,
            key.Open(HKEY_CURRENT_USER, key_path.c_str(), KEY_WRITE));
  ASSERT_EQ(ERROR_SUCCESS, key.DeleteKey(L""));
  ASSERT_NE(ERROR_SUCCESS,
            key.Open(HKEY_CURRENT_USER, key_path.c_str(), KEY_READ));

  ASSERT_EQ(ERROR_SUCCESS,
            key.Open(HKEY_CURRENT_USER, root_key().c_str(), KEY_WRITE));
  ASSERT_EQ(ERROR_SUCCESS, key.DeleteKey(L"Bar"));
  ASSERT_NE(ERROR_SUCCESS, key.DeleteKey(L"Bar"));
  ASSERT_NE(ERROR_SUCCESS,
            key.Open(HKEY_CURRENT_USER, key_path.c_str(), KEY_READ));
}

TEST_F(RegistryTest, OpenSubKey) {
  RegKey key;
  ASSERT_EQ(ERROR_SUCCESS, key.Open(HKEY_CURRENT_USER, root_key().c_str(),
                                    KEY_READ | KEY_CREATE_SUB_KEY));

  ASSERT_NE(ERROR_SUCCESS, key.OpenKey(L"foo", KEY_READ));
  ASSERT_EQ(ERROR_SUCCESS, key.CreateKey(L"foo", KEY_READ));
  ASSERT_EQ(ERROR_SUCCESS,
            key.Open(HKEY_CURRENT_USER, root_key().c_str(), KEY_READ));
  ASSERT_EQ(ERROR_SUCCESS, key.OpenKey(L"foo", KEY_READ));

  std::wstring foo_key = root_key() + L"\\Foo";
  ASSERT_EQ(ERROR_SUCCESS,
            key.Open(HKEY_CURRENT_USER, foo_key.c_str(), KEY_READ));

  ASSERT_EQ(ERROR_SUCCESS,
            key.Open(HKEY_CURRENT_USER, root_key().c_str(), KEY_WRITE));
  ASSERT_EQ(ERROR_SUCCESS, key.DeleteKey(L"foo"));
}

TEST_F(RegistryTest, InvalidRelativeKeyCreate) {
  RegKey key(HKEY_CURRENT_USER,
             base::StrCat({this->root_key(), L"_DoesNotExist"}).c_str(),
             KEY_WOW64_32KEY | KEY_READ);
  ASSERT_EQ(key.CreateKey(L"SomeSubKey", KEY_WOW64_32KEY | KEY_WRITE),
            ERROR_INVALID_HANDLE);
}

TEST_F(RegistryTest, InvalidRelativeKeyOpen) {
  RegKey key(HKEY_CURRENT_USER,
             base::StrCat({this->root_key(), L"_DoesNotExist"}).c_str(),
             KEY_WOW64_32KEY | KEY_READ);
  ASSERT_EQ(key.OpenKey(L"SomeSubKey", KEY_WOW64_32KEY | KEY_READ),
            ERROR_INVALID_HANDLE);
}

namespace {

class TestChangeDelegate {
 public:
  TestChangeDelegate() = default;
  ~TestChangeDelegate() = default;

  void OnKeyChanged(base::OnceClosure quit_closure) {
    std::move(quit_closure).Run();
    called_ = true;
  }

  bool WasCalled() {
    bool was_called = called_;
    called_ = false;
    return was_called;
  }

 private:
  bool called_ = false;
};

}  // namespace

TEST_F(RegistryTest, ChangeCallback) {
  RegKey key;
  TestChangeDelegate delegate;
  test::TaskEnvironment task_environment;
  base::RunLoop loop1;
  base::RunLoop loop2;
  base::RunLoop loop3;

  ASSERT_EQ(ERROR_SUCCESS,
            key.Open(HKEY_CURRENT_USER, root_key().c_str(), KEY_READ));

  ASSERT_TRUE(key.StartWatching(BindOnce(&TestChangeDelegate::OnKeyChanged,
                                         Unretained(&delegate),
                                         loop1.QuitWhenIdleClosure())));
  EXPECT_FALSE(delegate.WasCalled());

  // Make some change.
  RegKey key2;
  ASSERT_EQ(ERROR_SUCCESS, key2.Open(HKEY_CURRENT_USER, root_key().c_str(),
                                     KEY_READ | KEY_SET_VALUE));
  ASSERT_TRUE(key2.Valid());
  EXPECT_EQ(ERROR_SUCCESS, key2.WriteValue(L"name", L"data"));

  // Allow delivery of the notification.
  EXPECT_FALSE(delegate.WasCalled());
  loop1.Run();

  ASSERT_TRUE(delegate.WasCalled());
  EXPECT_FALSE(delegate.WasCalled());

  ASSERT_TRUE(key.StartWatching(BindOnce(&TestChangeDelegate::OnKeyChanged,
                                         Unretained(&delegate),
                                         loop2.QuitWhenIdleClosure())));

  // Change something else.
  EXPECT_EQ(ERROR_SUCCESS, key2.WriteValue(L"name2", L"data2"));
  loop2.Run();
  ASSERT_TRUE(delegate.WasCalled());

  ASSERT_TRUE(key.StartWatching(BindOnce(&TestChangeDelegate::OnKeyChanged,
                                         Unretained(&delegate),
                                         loop3.QuitWhenIdleClosure())));
  loop3.RunUntilIdle();
  EXPECT_FALSE(delegate.WasCalled());
}

namespace {

// A thread that runs tasks from a TestMockTimeTaskRunner.
class RegistryWatcherThread : public SimpleThread {
 public:
  explicit RegistryWatcherThread(
      scoped_refptr<base::TestMockTimeTaskRunner> task_runner)
      : SimpleThread("RegistryWatcherThread"),
        task_runner_(std::move(task_runner)) {}

 private:
  void Run() override {
    task_runner_->DetachFromThread();
    task_runner_->RunUntilIdle();
  }
  const scoped_refptr<base::TestMockTimeTaskRunner> task_runner_;
};

}  // namespace

TEST_F(RegistryTest, WatcherNotSignaledOnInitiatingThreadExit) {
  RegKey key;

  ASSERT_EQ(key.Open(HKEY_CURRENT_USER, root_key().c_str(), KEY_READ),
            ERROR_SUCCESS);

  auto test_task_runner = base::MakeRefCounted<base::TestMockTimeTaskRunner>(
      base::TestMockTimeTaskRunner::Type::kBoundToThread);
  ::testing::StrictMock<base::MockCallback<base::win::RegKey::ChangeCallback>>
      change_cb;

  test_task_runner->PostTask(FROM_HERE,
                             BindOnce(IgnoreResult(&RegKey::StartWatching),
                                      Unretained(&key), change_cb.Get()));

  {
    // Start the watch on a thread that then goes away.
    RegistryWatcherThread watcher_thread(test_task_runner);
    watcher_thread.Start();
    watcher_thread.Join();
  }

  // Termination of the thread should not cause a notification to get sent.
  ASSERT_TRUE(::testing::Mock::VerifyAndClearExpectations(&change_cb));
  test_task_runner->DetachFromThread();
  ASSERT_FALSE(test_task_runner->HasPendingTask());

  // Expect that a notification is sent when a change is made. Exit the run loop
  // when this happens.
  base::RunLoop run_loop;
  EXPECT_CALL(change_cb, Run).WillOnce([&run_loop] { run_loop.Quit(); });

  // Make some change.
  RegKey key2;
  ASSERT_EQ(key2.Open(HKEY_CURRENT_USER, root_key().c_str(),
                      KEY_READ | KEY_SET_VALUE),
            ERROR_SUCCESS);
  ASSERT_TRUE(key2.Valid());
  ASSERT_EQ(key2.WriteValue(L"name", L"data"), ERROR_SUCCESS);

  // Wait for the watcher to be signaled.
  run_loop.Run();
}

TEST_F(RegistryTest, TestMoveConstruct) {
  RegKey key;

  ASSERT_EQ(key.Open(HKEY_CURRENT_USER, root_key().c_str(), KEY_SET_VALUE),
            ERROR_SUCCESS);
  RegKey key2(std::move(key));

  // The old key should be meaningless now.
  EXPECT_EQ(key.Handle(), nullptr);

  // And the new one should work just fine.
  EXPECT_NE(key2.Handle(), nullptr);
  EXPECT_EQ(key2.WriteValue(L"foo", 1U), ERROR_SUCCESS);
}

TEST_F(RegistryTest, TestMoveAssign) {
  RegKey key;
  RegKey key2;
  const wchar_t kFooValueName[] = L"foo";

  ASSERT_EQ(key.Open(HKEY_CURRENT_USER, root_key().c_str(),
                     KEY_SET_VALUE | KEY_QUERY_VALUE),
            ERROR_SUCCESS);
  ASSERT_EQ(key.WriteValue(kFooValueName, 1U), ERROR_SUCCESS);
  ASSERT_EQ(key2.Create(HKEY_CURRENT_USER, (root_key() + L"\\child").c_str(),
                        KEY_SET_VALUE),
            ERROR_SUCCESS);
  key2 = std::move(key);

  // The old key should be meaningless now.
  EXPECT_EQ(key.Handle(), nullptr);

  // And the new one should hold what was the old one.
  EXPECT_NE(key2.Handle(), nullptr);
  DWORD foo = 0;
  ASSERT_EQ(key2.ReadValueDW(kFooValueName, &foo), ERROR_SUCCESS);
  EXPECT_EQ(foo, 1U);
}

// Verify that either the platform, or the API-integration, causes deletion
// attempts via an invalid handle to fail with the expected error code.
TEST_F(RegistryTest, DeleteWithInvalidRegKey) {
  RegKey key;

  static const wchar_t kFooName[] = L"foo";

  EXPECT_EQ(key.DeleteKey(kFooName), ERROR_INVALID_HANDLE);
  EXPECT_EQ(key.DeleteValue(kFooName), ERROR_INVALID_HANDLE);
}

// A test harness for tests of RegKey::DeleteKey; parameterized on whether to
// perform non-recursive or recursive deletes.
class DeleteKeyRegistryTest
    : public RegistryTest,
      public ::testing::WithParamInterface<RegKey::RecursiveDelete> {
 protected:
  DeleteKeyRegistryTest() = default;

 private:
};

// Test that DeleteKey does not follow symbolic links.
TEST_P(DeleteKeyRegistryTest, DoesNotFollowLinks) {
  // Create a subkey that should not be deleted.
  std::wstring target_path = root_key() + L"\\LinkTarget";
  {
    RegKey target;
    ASSERT_EQ(target.Create(HKEY_CURRENT_USER, target_path.c_str(), KEY_WRITE),
              ERROR_SUCCESS);
    ASSERT_EQ(target.WriteValue(L"IsTarget", 1U), ERROR_SUCCESS);
  }

  // Create a link to the above key.
  std::wstring source_path = root_key() + L"\\LinkSource";
  {
    HKEY link_handle = {};
    ASSERT_EQ(RegCreateKeyEx(HKEY_CURRENT_USER, source_path.c_str(), 0, nullptr,
                             REG_OPTION_CREATE_LINK | REG_OPTION_NON_VOLATILE,
                             KEY_WRITE, nullptr, &link_handle, nullptr),
              ERROR_SUCCESS);
    RegKey link(std::exchange(link_handle, HKEY{}));
    ASSERT_TRUE(link.Valid());

    std::wstring user_sid;
    ASSERT_TRUE(GetUserSidString(&user_sid));

    std::wstring value =
        base::StrCat({L"\\Registry\\User\\", user_sid, L"\\", override_path(),
                      L"\\", root_key(), L"\\LinkTarget"});
    ASSERT_EQ(link.WriteValue(L"SymbolicLinkValue", value.data(),
                              value.size() * sizeof(wchar_t), REG_LINK),
              ERROR_SUCCESS);
  }

  // Verify that the link works.
  {
    RegKey link;
    ASSERT_EQ(link.Open(HKEY_CURRENT_USER, source_path.c_str(), KEY_READ),
              ERROR_SUCCESS);
    DWORD value = 0;
    ASSERT_EQ(link.ReadValueDW(L"IsTarget", &value), ERROR_SUCCESS);
    ASSERT_EQ(value, 1U);
  }

  // Now delete the link and ensure that it was deleted, but not the target.
  ASSERT_EQ(RegKey(HKEY_CURRENT_USER, root_key().c_str(), KEY_READ)
                .DeleteKey(L"LinkSource", GetParam()),
            ERROR_SUCCESS);
  {
    RegKey source;
    ASSERT_NE(source.Open(HKEY_CURRENT_USER, source_path.c_str(), KEY_READ),
              ERROR_SUCCESS);
  }
  {
    RegKey target;
    ASSERT_EQ(target.Open(HKEY_CURRENT_USER, target_path.c_str(), KEY_READ),
              ERROR_SUCCESS);
  }
}

INSTANTIATE_TEST_SUITE_P(NonRecursive,
                         DeleteKeyRegistryTest,
                         ::testing::Values(RegKey::RecursiveDelete(false)));
INSTANTIATE_TEST_SUITE_P(Recursive,
                         DeleteKeyRegistryTest,
                         ::testing::Values(RegKey::RecursiveDelete(true)));

// A test harness for tests that use HKLM to test WoW redirection and such.
// TODO(crbug.com/41110299): The tests here that write to the registry are
// disabled because they need work to handle parallel runs of different tests.
class RegistryTestHKLM : public ::testing::Test {
 protected:
  enum : REGSAM {
#if defined(_WIN64)
    kNativeViewMask = KEY_WOW64_64KEY,
    kRedirectedViewMask = KEY_WOW64_32KEY,
#else
    kNativeViewMask = KEY_WOW64_32KEY,
    kRedirectedViewMask = KEY_WOW64_64KEY,
#endif  //  _WIN64
  };

  RegistryTestHKLM()
      : foo_software_key_(std::wstring(L"Software\\") + kRootKey + L"\\Foo") {}

  static bool IsRedirectorPresent() {
#if defined(_WIN64)
    return true;
#else
    return OSInfo::GetInstance()->IsWowX86OnAMD64();
#endif
  }

  const std::wstring foo_software_key_;
};

class RegistryTestHKLMAdmin : public RegistryTestHKLM {
 protected:
  void SetUp() override {
    if (!IsRedirectorPresent()) {
      GTEST_SKIP();
    }
    if (!::IsUserAnAdmin()) {
      GTEST_SKIP();
    }
    // Clean up any stale registry keys.
    for (const REGSAM mask :
         {this->kNativeViewMask, this->kRedirectedViewMask}) {
      RegKey key;
      if (key.Open(HKEY_LOCAL_MACHINE, L"Software", KEY_SET_VALUE | mask) ==
          ERROR_SUCCESS) {
        key.DeleteKey(kRootKey);
      }
    }
  }
};

// This test requires running as an Administrator as it tests redirected
// registry writes to HKLM\Software
// http://msdn.microsoft.com/en-us/library/windows/desktop/aa384253.aspx
TEST_F(RegistryTestHKLMAdmin, Wow64RedirectedFromNative) {
  RegKey key;

  // Test redirected key access from non-redirected.
  ASSERT_EQ(ERROR_SUCCESS,
            key.Create(HKEY_LOCAL_MACHINE, foo_software_key_.c_str(),
                       KEY_WRITE | kRedirectedViewMask));
  ASSERT_NE(ERROR_SUCCESS,
            key.Open(HKEY_LOCAL_MACHINE, foo_software_key_.c_str(), KEY_READ));
  ASSERT_NE(ERROR_SUCCESS,
            key.Open(HKEY_LOCAL_MACHINE, foo_software_key_.c_str(),
                     KEY_READ | kNativeViewMask));

  // Open the non-redirected view of the parent and try to delete the test key.
  ASSERT_EQ(ERROR_SUCCESS,
            key.Open(HKEY_LOCAL_MACHINE, L"Software", KEY_SET_VALUE));
  ASSERT_NE(ERROR_SUCCESS, key.DeleteKey(kRootKey));
  ASSERT_EQ(ERROR_SUCCESS, key.Open(HKEY_LOCAL_MACHINE, L"Software",
                                    KEY_SET_VALUE | kNativeViewMask));
  ASSERT_NE(ERROR_SUCCESS, key.DeleteKey(kRootKey));

  // Open the redirected view and delete the key created above.
  ASSERT_EQ(ERROR_SUCCESS, key.Open(HKEY_LOCAL_MACHINE, L"Software",
                                    KEY_SET_VALUE | kRedirectedViewMask));
  ASSERT_EQ(ERROR_SUCCESS, key.DeleteKey(kRootKey));
}

// Test for the issue found in http://crbug.com/384587 where OpenKey would call
// Close() and reset wow64_access_ flag to 0 and cause a NOTREACHED to hit on a
// subsequent OpenKey call.
TEST_F(RegistryTestHKLM, SameWowFlags) {
  RegKey key;

  ASSERT_EQ(ERROR_SUCCESS, key.Open(HKEY_LOCAL_MACHINE, L"Software",
                                    KEY_READ | KEY_WOW64_64KEY));
  ASSERT_EQ(ERROR_SUCCESS,
            key.OpenKey(L"Microsoft", KEY_READ | KEY_WOW64_64KEY));
  ASSERT_EQ(ERROR_SUCCESS, key.OpenKey(L"Windows", KEY_READ | KEY_WOW64_64KEY));
}

TEST_F(RegistryTestHKLMAdmin, Wow64NativeFromRedirected) {
  RegKey key;

  // Test non-redirected key access from redirected.
  ASSERT_EQ(ERROR_SUCCESS,
            key.Create(HKEY_LOCAL_MACHINE, foo_software_key_.c_str(),
                       KEY_WRITE | kNativeViewMask));
  ASSERT_EQ(ERROR_SUCCESS,
            key.Open(HKEY_LOCAL_MACHINE, foo_software_key_.c_str(), KEY_READ));
  ASSERT_NE(ERROR_SUCCESS,
            key.Open(HKEY_LOCAL_MACHINE, foo_software_key_.c_str(),
                     KEY_READ | kRedirectedViewMask));

  // Open the redirected view of the parent and try to delete the test key
  // from the non-redirected view.
  ASSERT_EQ(ERROR_SUCCESS, key.Open(HKEY_LOCAL_MACHINE, L"Software",
                                    KEY_SET_VALUE | kRedirectedViewMask));
  ASSERT_NE(ERROR_SUCCESS, key.DeleteKey(kRootKey));

  ASSERT_EQ(ERROR_SUCCESS, key.Open(HKEY_LOCAL_MACHINE, L"Software",
                                    KEY_SET_VALUE | kNativeViewMask));
  ASSERT_EQ(ERROR_SUCCESS, key.DeleteKey(kRootKey));
}

}  // namespace base::win
