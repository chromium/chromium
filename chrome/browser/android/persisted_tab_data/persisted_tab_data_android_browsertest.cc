// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/persisted_tab_data/persisted_tab_data_android.h"

#include <deque>

#include "chrome/browser/android/persisted_tab_data/test/bar_persisted_tab_data.h"
#include "chrome/browser/android/persisted_tab_data/test/foo_persisted_tab_data.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/android/tab_model/tab_model.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list.h"
#include "chrome/test/base/android/android_browser_test.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

namespace {
const int32_t INITIAL_VALUE = 42;
const int32_t CHANGED_VALUE = 52;
}  // namespace

class PersistedTabDataAndroidBrowserTest : public AndroidBrowserTest {
 public:
  PersistedTabDataAndroidBrowserTest() = default;

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());

    // Create a second Tab.
    TabModel* tab_model =
        TabModelList::GetTabModelForWebContents(web_contents());
    ASSERT_EQ(1, tab_model->GetTabCount());
    std::unique_ptr<content::WebContents> contents =
        content::WebContents::Create(
            content::WebContents::CreateParams(profile()));
    content::WebContents* second_web_contents = contents.release();
    tab_model->CreateTab(tab_android(), second_web_contents, /*select=*/true);
    ASSERT_EQ(2, tab_model->GetTabCount());
  }

  TabAndroid* tab_android() {
    return TabAndroid::FromWebContents(web_contents());
  }

  TabAndroid* another_tab() {
    TabModel* tab_model =
        TabModelList::GetTabModelForWebContents(web_contents());
    int another_tab_index = tab_model->GetTabAt(0) == tab_android() ? 1 : 0;
    return tab_model->GetTabAt(another_tab_index);
  }

  void FooExistsForTesting(TabAndroid* tab_android,
                           bool expect_exists,
                           base::RunLoop& run_loop) {
    FooPersistedTabDataAndroid::ExistsForTesting(
        tab_android,
        base::BindOnce(
            [](base::OnceClosure done, bool expect_exists, bool exists) {
              EXPECT_EQ(expect_exists, exists);
              std::move(done).Run();
            },
            run_loop.QuitClosure(), expect_exists));
  }

  void BarExistsForTesting(TabAndroid* tab_android,
                           bool expect_exists,
                           base::RunLoop& run_loop) {
    BarPersistedTabDataAndroid::ExistsForTesting(
        tab_android,
        base::BindOnce(
            [](base::OnceClosure done, bool expect_exists, bool exists) {
              EXPECT_EQ(expect_exists, exists);
              std::move(done).Run();
            },
            run_loop.QuitClosure(), expect_exists));
  }

  void OnTabClose(TabAndroid* tab_android) {
    PersistedTabDataAndroid::OnTabClose(tab_android);
  }

  void OnDeferredStartup() { PersistedTabDataAndroid::OnDeferredStartup(); }

  const base::circular_deque<PersistedTabDataAndroidDeferredRequest>&
  GetDeferredRequests() {
    return PersistedTabDataAndroid::GetDeferredRequests();
  }

 private:
  content::WebContents* web_contents() {
    return chrome_test_utils::GetActiveWebContents(this);
  }

  Profile* profile() {
    auto* web_contents = chrome_test_utils::GetActiveWebContents(this);
    return Profile::FromBrowserContext(web_contents->GetBrowserContext());
  }
};

IN_PROC_BROWSER_TEST_F(PersistedTabDataAndroidBrowserTest, TestRemoveAll) {
  OnDeferredStartup();
  // Save FooPersistedTabDataAndroid and BarPersistedTabDataAndroid for
  // tab_android()
  FooPersistedTabDataAndroid foo_persisted_tab_data_android(tab_android());
  foo_persisted_tab_data_android.SetValue(42);
  BarPersistedTabDataAndroid bar_persisted_tab_data_android(tab_android());
  bar_persisted_tab_data_android.SetValue(true);

  // FooPersistedTabDataAndroid and BarPersistedTabDataAndroid should both
  // be in storage now for tab_android()
  base::RunLoop run_loop[4];
  FooExistsForTesting(tab_android(), true, run_loop[0]);
  run_loop[0].Run();
  BarExistsForTesting(tab_android(), true, run_loop[1]);
  run_loop[1].Run();

  OnTabClose(tab_android());

  // FooPersistedTabDataAndroid and BarPersistedTabDataAndroid should both
  // be removed following a Tab close of tab_android().
  FooExistsForTesting(tab_android(), false, run_loop[2]);
  run_loop[2].Run();
  BarExistsForTesting(tab_android(), false, run_loop[3]);
  run_loop[3].Run();
}

IN_PROC_BROWSER_TEST_F(PersistedTabDataAndroidBrowserTest,
                       TestRemoveAllMultipleTabs) {
  OnDeferredStartup();
  // Save FooPersistedTabDataAndroid and BarPersistedTabDataAndroid for
  // tab_android() and another_tab().
  FooPersistedTabDataAndroid foo_persisted_tab_data_android(tab_android());
  foo_persisted_tab_data_android.SetValue(42);
  BarPersistedTabDataAndroid bar_persisted_tab_data_android(tab_android());
  bar_persisted_tab_data_android.SetValue(true);
  FooPersistedTabDataAndroid another_tab_foo_persisted_tab_data_android(
      another_tab());
  another_tab_foo_persisted_tab_data_android.SetValue(32);
  BarPersistedTabDataAndroid another_tab_bar_persisted_tab_data_android(
      another_tab());
  another_tab_bar_persisted_tab_data_android.SetValue(false);

  // FooPersistedTabDataAndroid and BarPersistedTabDataAndroid should both
  // exist for tab_android() and another_tab().
  base::RunLoop run_loop[8];
  FooExistsForTesting(tab_android(), true, run_loop[0]);
  run_loop[0].Run();
  FooExistsForTesting(another_tab(), true, run_loop[1]);
  run_loop[1].Run();
  BarExistsForTesting(tab_android(), true, run_loop[2]);
  run_loop[2].Run();
  BarExistsForTesting(another_tab(), true, run_loop[3]);
  run_loop[3].Run();

  OnTabClose(tab_android());

  // FooPersistedTabDataAndroid and BarPersistedTabDataAndroid should
  // both be removed following a Tab close of tab_android(). However,
  // another_tab() is still open so FooPersistedTabDataAndroid and
  // BarPersistedTabDataAndroid should still be in storage for
  // another_tab().
  FooExistsForTesting(tab_android(), false, run_loop[4]);
  run_loop[4].Run();
  BarExistsForTesting(tab_android(), false, run_loop[5]);
  run_loop[5].Run();
  FooExistsForTesting(another_tab(), true, run_loop[6]);
  run_loop[6].Run();
  BarExistsForTesting(another_tab(), true, run_loop[7]);
  run_loop[7].Run();
}

IN_PROC_BROWSER_TEST_F(PersistedTabDataAndroidBrowserTest,
                       TestCachedCallbacks) {
  OnDeferredStartup();
  content::RunAllTasksUntilIdle();

  if (!tab_android()->GetUserData(FooPersistedTabDataAndroid::UserDataKey())) {
    tab_android()->SetUserData(
        FooPersistedTabDataAndroid::UserDataKey(),
        std::make_unique<FooPersistedTabDataAndroid>(tab_android()));
  }

  FooPersistedTabDataAndroid* foo_persisted_tab_data_android =
      static_cast<FooPersistedTabDataAndroid*>(tab_android()->GetUserData(
          FooPersistedTabDataAndroid::UserDataKey()));
  foo_persisted_tab_data_android->SetValue(INITIAL_VALUE);

  base::RunLoop run_loop;
  FooPersistedTabDataAndroid::From(
      tab_android(),
      base::BindOnce([](PersistedTabDataAndroid* persisted_tab_data) {
        FooPersistedTabDataAndroid* foo_ptd =
            static_cast<FooPersistedTabDataAndroid*>(persisted_tab_data);
        EXPECT_EQ(INITIAL_VALUE, foo_ptd->value());
        foo_ptd->SetValue(CHANGED_VALUE);
      }));
  FooPersistedTabDataAndroid::From(
      tab_android(),
      base::BindOnce(
          [](base::OnceClosure done,
             PersistedTabDataAndroid* persisted_tab_data) {
            FooPersistedTabDataAndroid* foo_ptd =
                static_cast<FooPersistedTabDataAndroid*>(persisted_tab_data);
            EXPECT_EQ(CHANGED_VALUE, foo_ptd->value());
            std::move(done).Run();
          },
          run_loop.QuitClosure()));
  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(PersistedTabDataAndroidBrowserTest,
                       TestDeferredStartup) {
  FooPersistedTabDataAndroid foo_persisted_tab_data_android(tab_android());
  foo_persisted_tab_data_android.SetValue(42);
  BarPersistedTabDataAndroid bar_persisted_tab_data_android(tab_android());
  bar_persisted_tab_data_android.SetValue(true);
  FooPersistedTabDataAndroid another_tab_foo_persisted_tab_data_android(
      another_tab());
  another_tab_foo_persisted_tab_data_android.SetValue(32);
  BarPersistedTabDataAndroid another_tab_bar_persisted_tab_data_android(
      another_tab());
  another_tab_bar_persisted_tab_data_android.SetValue(false);

  base::RunLoop run_loop[4];
  int beginning_deferred_request_size = GetDeferredRequests().size();
  FooPersistedTabDataAndroid::From(
      tab_android(), base::BindOnce(
                         [](base::OnceClosure done,
                            PersistedTabDataAndroid* persisted_tab_data) {
                           std::move(done).Run();
                         },
                         run_loop[0].QuitClosure()));
  BarPersistedTabDataAndroid::From(
      tab_android(), base::BindOnce(
                         [](base::OnceClosure done,
                            PersistedTabDataAndroid* persisted_tab_data) {
                           std::move(done).Run();
                         },
                         run_loop[1].QuitClosure()));
  // Requests should be stored for deferred startup.
  EXPECT_EQ(beginning_deferred_request_size + 2u, GetDeferredRequests().size());
  OnDeferredStartup();
  run_loop[0].Run();
  run_loop[1].Run();
  // Deferred requests should have been executed.
  EXPECT_EQ(0u, GetDeferredRequests().size());
  FooPersistedTabDataAndroid::From(
      another_tab(), base::BindOnce(
                         [](base::OnceClosure done,
                            PersistedTabDataAndroid* persisted_tab_data) {
                           std::move(done).Run();
                         },
                         run_loop[2].QuitClosure()));
  BarPersistedTabDataAndroid::From(
      another_tab(), base::BindOnce(
                         [](base::OnceClosure done,
                            PersistedTabDataAndroid* persisted_tab_data) {
                           std::move(done).Run();
                         },
                         run_loop[3].QuitClosure()));
  // Should no longer be added to deferred startup queue because
  // deferred startup has happened.
  EXPECT_EQ(0u, GetDeferredRequests().size());
  run_loop[2].Run();
  run_loop[3].Run();
}
