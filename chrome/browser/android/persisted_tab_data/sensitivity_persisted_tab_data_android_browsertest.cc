// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/persisted_tab_data/sensitivity_persisted_tab_data_android.h"

#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/android/android_browser_test.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "components/page_content_annotations/core/page_content_annotations_common.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "url/gurl.h"

namespace {
const char kSensitiveRelUrl[] = "/android/sensitive.html";
const char kNonSensitiveRelUrl[] = "/android/hello.html";
const char kNonSensitiveRelUrl2[] = "/android/second.html";
const page_content_annotations::PageContentAnnotationsResult kSensitiveResult =
    page_content_annotations::PageContentAnnotationsResult::
        CreateContentVisibilityScoreResult(0.1);
const page_content_annotations::PageContentAnnotationsResult
    kNonSensitiveResult = page_content_annotations::
        PageContentAnnotationsResult::CreateContentVisibilityScoreResult(0.7);
const page_content_annotations::PageContentAnnotationsResult
    kNonSensitiveResult2 = page_content_annotations::
        PageContentAnnotationsResult::CreateContentVisibilityScoreResult(0.8);
}  // namespace

class SensitivityPersistedTabDataAndroidBrowserTest
    : public AndroidBrowserTest {
 public:
  SensitivityPersistedTabDataAndroidBrowserTest() = default;

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());
    PersistedTabDataAndroid::OnDeferredStartup();
    content::RunAllTasksUntilIdle();
  }

  content::WebContents* web_contents() {
    return chrome_test_utils::GetActiveWebContents(this);
  }

  void Remove(SensitivityPersistedTabDataAndroid* sptda) { sptda->Remove(); }

  void Save(SensitivityPersistedTabDataAndroid* sptda) { sptda->Save(); }

  void Deserialize(SensitivityPersistedTabDataAndroid* sptda,
                   const std::vector<uint8_t>& data) {
    sptda->Deserialize(data);
  }

  std::unique_ptr<const std::vector<uint8_t>> Serialize(
      SensitivityPersistedTabDataAndroid* sptda) {
    return sptda->Serialize();
  }

  void OnTabClose(TabAndroid* tab_android) {
    PersistedTabDataAndroid::OnTabClose(tab_android);
  }

  void ExistsForTesting(TabAndroid* tab_android,
                        bool expect_exists,
                        base::RunLoop& run_loop) {
    SensitivityPersistedTabDataAndroid::ExistsForTesting(
        tab_android,
        base::BindOnce(
            [](base::OnceClosure done, bool expect_exists, bool exists) {
              EXPECT_EQ(expect_exists, exists);
              std::move(done).Run();
            },
            run_loop.QuitClosure(), expect_exists));
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
  Remove(sptda.get());
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
  std::unique_ptr<const std::vector<uint8_t>> serialized = Serialize(&sptda);
  SensitivityPersistedTabDataAndroid deserialized(tab_android);
  EXPECT_FALSE(deserialized.is_sensitive());
  Deserialize(&deserialized, *serialized.get());
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
  Save(sptda);

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
                       TestOnPageContentAnnotatedSensitivePage) {
  const GURL sensitive_url =
      embedded_test_server()->GetURL("localhost", kSensitiveRelUrl);
  // TODO: handle return value.
  std::ignore = content::NavigateToURL(web_contents(), sensitive_url);

  TabAndroid* tab_android = TabAndroid::FromWebContents(web_contents());
  SensitivityPersistedTabDataAndroid* sptda =
      new SensitivityPersistedTabDataAndroid(tab_android);

  EXPECT_EQ(tab_android->GetURL().spec(), sensitive_url.spec());

  sptda->OnPageContentAnnotated(sensitive_url, kSensitiveResult);
  EXPECT_TRUE(sptda->is_sensitive());
}

IN_PROC_BROWSER_TEST_F(SensitivityPersistedTabDataAndroidBrowserTest,
                       TestOnPageContentAnnotatedNonSensitivePage) {
  const GURL non_sensitive_url =
      embedded_test_server()->GetURL("localhost", kNonSensitiveRelUrl);
  // TODO: handle return value.
  std::ignore = content::NavigateToURL(web_contents(), non_sensitive_url);

  TabAndroid* tab_android = TabAndroid::FromWebContents(web_contents());
  SensitivityPersistedTabDataAndroid* sptda =
      new SensitivityPersistedTabDataAndroid(tab_android);
  EXPECT_EQ(tab_android->GetURL().spec(), non_sensitive_url.spec());

  sptda->OnPageContentAnnotated(non_sensitive_url, kNonSensitiveResult);
  EXPECT_FALSE(sptda->is_sensitive());
}

IN_PROC_BROWSER_TEST_F(SensitivityPersistedTabDataAndroidBrowserTest,
                       TestMultipleAnnotations) {
  base::RunLoop run_loop;

  const GURL sensitive_url =
      embedded_test_server()->GetURL("localhost", kSensitiveRelUrl);
  const GURL non_sensitive_url =
      embedded_test_server()->GetURL("localhost", kNonSensitiveRelUrl);
  const GURL non_sensitive_url2 =
      embedded_test_server()->GetURL("localhost", kNonSensitiveRelUrl2);

  // TODO: handle return value.
  std::ignore = content::NavigateToURL(web_contents(), sensitive_url);
  TabAndroid* tab_android = TabAndroid::FromWebContents(web_contents());
  SensitivityPersistedTabDataAndroid* sptda =
      new SensitivityPersistedTabDataAndroid(tab_android);
  EXPECT_EQ(tab_android->GetURL().spec(), sensitive_url.spec());

  // Annotate both sensitive and non-sensitive tabs
  sptda->OnPageContentAnnotated(non_sensitive_url, kNonSensitiveResult);
  sptda->OnPageContentAnnotated(sensitive_url, kSensitiveResult);
  sptda->OnPageContentAnnotated(non_sensitive_url2, kNonSensitiveResult2);
  tab_android->SetUserData(SensitivityPersistedTabDataAndroid::UserDataKey(),
                           nullptr);

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
                       TestOnComplete) {
  base::RunLoop run_loop[3];
  TabAndroid* tab_android = TabAndroid::FromWebContents(web_contents());
  // Creates a SensitivityPersistedTabDataAndroid and stores on disk.
  SensitivityPersistedTabDataAndroid::From(
      tab_android, base::BindOnce(
                       [](base::OnceClosure done,
                          PersistedTabDataAndroid* persisted_tab_data) {
                         EXPECT_NE(nullptr, persisted_tab_data);
                         std::move(done).Run();
                       },
                       run_loop[0].QuitClosure()));
  run_loop[0].Run();
  ExistsForTesting(tab_android, true, run_loop[1]);
  run_loop[1].Run();
  // Should clean up SensitivityPersistedTabDataAndroid
  OnTabClose(tab_android);
  ExistsForTesting(tab_android, false, run_loop[2]);
  run_loop[2].Run();
}
