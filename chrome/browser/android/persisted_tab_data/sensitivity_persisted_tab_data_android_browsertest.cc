// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/persisted_tab_data/sensitivity_persisted_tab_data_android.h"

#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/android/android_browser_test.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

class SensitivityPersistedTabDataAndroidBrowserTest
    : public AndroidBrowserTest {
 public:
  SensitivityPersistedTabDataAndroidBrowserTest() = default;

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  content::WebContents* web_contents() {
    return chrome_test_utils::GetActiveWebContents(this);
  }

 private:
  Profile* profile() {
    auto* web_contents = chrome_test_utils::GetActiveWebContents(this);
    return Profile::FromBrowserContext(web_contents->GetBrowserContext());
  }
};

IN_PROC_BROWSER_TEST_F(SensitivityPersistedTabDataAndroidBrowserTest,
                       TestUserData) {
  base::RunLoop run_loop;
  TabAndroid* tab_android = TabAndroid::FromWebContents(web_contents());
  std::unique_ptr<SensitivityPersistedTabDataAndroid> sptda =
      std::make_unique<SensitivityPersistedTabDataAndroid>(tab_android);
  sptda->set_is_sensitive(true);
  sptda->Remove();
  tab_android->SetUserData(SensitivityPersistedTabDataAndroid::UserDataKey(),
                           std::move(sptda));
  SensitivityPersistedTabDataAndroid::From(
      tab_android,
      base::BindOnce(
          [](base::OnceClosure done,
             PersistedTabDataAndroid* persisted_tab_data) {
            EXPECT_TRUE(static_cast<SensitivityPersistedTabDataAndroid*>(
                            persisted_tab_data)
                            ->is_sensitive());
            std::move(done).Run();
          },
          run_loop.QuitClosure()));
  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(SensitivityPersistedTabDataAndroidBrowserTest,
                       TestSerialize) {
  TabAndroid* tab_android = TabAndroid::FromWebContents(web_contents());
  SensitivityPersistedTabDataAndroid sptda(tab_android);
  sptda.set_is_sensitive(true);
  std::unique_ptr<const std::vector<uint8_t>> serialized = sptda.Serialize();
  SensitivityPersistedTabDataAndroid deserialized(tab_android);
  deserialized.Deserialize(*serialized.get());
  EXPECT_TRUE(deserialized.is_sensitive());
}

IN_PROC_BROWSER_TEST_F(SensitivityPersistedTabDataAndroidBrowserTest,
                       TestSavedPTD) {
  base::RunLoop run_loop;
  TabAndroid* tab_android = TabAndroid::FromWebContents(web_contents());
  SensitivityPersistedTabDataAndroid* sptda =
      new SensitivityPersistedTabDataAndroid(tab_android);
  sptda->set_is_sensitive(true);
  tab_android->SetUserData(SensitivityPersistedTabDataAndroid::UserDataKey(),
                           nullptr);
  sptda->Save();

  SensitivityPersistedTabDataAndroid::From(
      tab_android,
      base::BindOnce(
          [](base::OnceClosure done,
             PersistedTabDataAndroid* persisted_tab_data) {
            EXPECT_TRUE(static_cast<SensitivityPersistedTabDataAndroid*>(
                            persisted_tab_data)
                            ->is_sensitive());
            std::move(done).Run();
          },
          run_loop.QuitClosure()));
  run_loop.Run();
}
