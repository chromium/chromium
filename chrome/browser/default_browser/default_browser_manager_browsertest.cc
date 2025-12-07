// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/default_browser/default_browser_manager.h"

#include <string>

#include "base/test/test_future.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/global_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"

#if BUILDFLAG(IS_WIN)
#include "base/test/test_reg_util_win.h"
#include "base/win/registry.h"
#endif  // BUILDFLAG(IS_WIN)

namespace default_browser {

namespace {

#if BUILDFLAG(IS_WIN)
constexpr wchar_t kProgIdValue[] = L"ProgId";

constexpr wchar_t kRegistryPath[] =
    L"Software\\Microsoft\\Windows\\Shell\\Associations\\UrlAssociations\\h"
    L"ttp\\UserChoice";

void CreateDefaultBrowserKey(const std::wstring& prog_id) {
  base::win::RegKey key(HKEY_CURRENT_USER, kRegistryPath,
                        KEY_WRITE | KEY_CREATE_SUB_KEY);
  ASSERT_TRUE(key.Valid());
  ASSERT_EQ(key.WriteValue(kProgIdValue, prog_id.c_str()), ERROR_SUCCESS);
}

void ChangeDefaultBrowserProgId(const std::wstring& new_prog_id) {
  base::win::RegKey key(HKEY_CURRENT_USER, kRegistryPath, KEY_WRITE);
  ASSERT_TRUE(key.Valid());
  ASSERT_EQ(key.WriteValue(kProgIdValue, new_prog_id.c_str()), ERROR_SUCCESS);
}
#endif  // BUILDFLAG(IS_WIN)

}  // namespace

#if BUILDFLAG(IS_WIN)
class DefaultBrowserManagerWinBrowserTest : public InProcessBrowserTest {
 public:
  DefaultBrowserManagerWinBrowserTest() = default;
  ~DefaultBrowserManagerWinBrowserTest() override = default;

  void SetUp() override {
    ASSERT_NO_FATAL_FAILURE(
        registry_override_manager_.OverrideRegistry(HKEY_CURRENT_USER));
    base::win::RegKey key;
    ASSERT_EQ(ERROR_SUCCESS,
              key.Create(HKEY_CURRENT_USER, kRegistryPath, KEY_WRITE));
    InProcessBrowserTest::SetUp();
  }

 protected:
  registry_util::RegistryOverrideManager registry_override_manager_;
};

IN_PROC_BROWSER_TEST_F(DefaultBrowserManagerWinBrowserTest,
                       ChangeIsDetectedAndObserverIsNotified) {
  CreateDefaultBrowserKey(L"ChromeHTML");
  DefaultBrowserManager* manager =
      g_browser_process->GetFeatures()->default_browser_manager();
  ASSERT_TRUE(manager);

  base::test::TestFuture<void> future;
  base::CallbackListSubscription subscription =
      manager->RegisterDefaultBrowserChanged(future.GetRepeatingCallback());
  ChangeDefaultBrowserProgId(L"VanadiumHTML");
  EXPECT_TRUE(future.Wait());
}

IN_PROC_BROWSER_TEST_F(DefaultBrowserManagerWinBrowserTest,
                       SubscriptionDestroyedPreventsCallback) {
  CreateDefaultBrowserKey(L"ChromeHTML");
  DefaultBrowserManager* manager =
      g_browser_process->GetFeatures()->default_browser_manager();
  ASSERT_TRUE(manager);

  base::test::TestFuture<void> future;
  {
    base::CallbackListSubscription subscription =
        manager->RegisterDefaultBrowserChanged(future.GetRepeatingCallback());
  }

  ChangeDefaultBrowserProgId(L"VanadiumHTML");
  content::RunAllTasksUntilIdle();
  EXPECT_FALSE(future.IsReady());
}

IN_PROC_BROWSER_TEST_F(DefaultBrowserManagerWinBrowserTest,
                       AllRegisteredObserversAreNotified) {
  CreateDefaultBrowserKey(L"ChromeHTML");
  DefaultBrowserManager* manager =
      g_browser_process->GetFeatures()->default_browser_manager();
  ASSERT_TRUE(manager);

  base::test::TestFuture<void> future1;
  base::test::TestFuture<void> future2;
  base::CallbackListSubscription subscription1 =
      manager->RegisterDefaultBrowserChanged(future1.GetRepeatingCallback());
  base::CallbackListSubscription subscription2 =
      manager->RegisterDefaultBrowserChanged(future2.GetRepeatingCallback());

  ChangeDefaultBrowserProgId(L"VanadiumHTML");
  EXPECT_TRUE(future1.Wait());
  EXPECT_TRUE(future2.Wait());
}

IN_PROC_BROWSER_TEST_F(DefaultBrowserManagerWinBrowserTest,
                       SubsequentChangesAreAlsoDetected) {
  CreateDefaultBrowserKey(L"ChromeHTML");
  DefaultBrowserManager* manager =
      g_browser_process->GetFeatures()->default_browser_manager();
  ASSERT_TRUE(manager);

  {
    base::test::TestFuture<void> future_change1;
    base::CallbackListSubscription subscription =
        manager->RegisterDefaultBrowserChanged(
            future_change1.GetRepeatingCallback());
    ChangeDefaultBrowserProgId(L"VanadiumHTML");
    ASSERT_TRUE(future_change1.Wait()) << "First change was not detected.";
  }

  {
    base::test::TestFuture<void> future_change2;
    base::CallbackListSubscription subscription =
        manager->RegisterDefaultBrowserChanged(
            future_change2.GetRepeatingCallback());
    ChangeDefaultBrowserProgId(L"ManganeseHTML");
    EXPECT_TRUE(future_change2.Wait()) << "Second change was not detected.";
  }
}

IN_PROC_BROWSER_TEST_F(
    DefaultBrowserManagerWinBrowserTest,
    SubsequentChangesAreAlsoDetectedWithTheSameSubscription) {
  CreateDefaultBrowserKey(L"ChromeHTML");
  DefaultBrowserManager* manager =
      g_browser_process->GetFeatures()->default_browser_manager();
  ASSERT_TRUE(manager);

  int call_count = 0;
  base::RunLoop run_loop1;
  base::RunLoop run_loop2;

  // This callback will be invoked for each default browser change.
  // On the first invocation, it will quit the first `run_loop`.
  // On the second, it will quit the second `run_loop`.
  auto subscription_callback = base::BindRepeating(
      [](int* count, base::RunLoop* loop1, base::RunLoop* loop2) {
        (*count)++;
        if (*count == 1) {
          loop1->Quit();
        } else if (*count == 2) {
          loop2->Quit();
        }
      },
      &call_count, &run_loop1, &run_loop2);

  // Register the single subscription that will be used for the entire test.
  base::CallbackListSubscription subscription =
      manager->RegisterDefaultBrowserChanged(subscription_callback);

  // Trigger the first change.
  ChangeDefaultBrowserProgId(L"VanadiumHTML");
  run_loop1.Run();
  EXPECT_EQ(call_count, 1) << "First change was not detected.";

  // Trigger the second change. The same subscription should be notified again.
  ChangeDefaultBrowserProgId(L"ManganeseHTML");
  run_loop2.Run();
  EXPECT_EQ(call_count, 2) << "Second change was not detected.";
}
#endif  // BUILDFLAG(IS_WIN)

}  // namespace default_browser
