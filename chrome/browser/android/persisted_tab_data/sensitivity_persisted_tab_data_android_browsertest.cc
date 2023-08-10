// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/persisted_tab_data/sensitivity_persisted_tab_data_android.h"

#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/android/android_browser_test.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "components/optimization_guide/core/page_content_annotations_common.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "url/gurl.h"

namespace {
const char kSensitiveRelUrl[] = "/android/sensitive.html";
const char kNonSensitiveRelUrl[] = "/android/hello.html";
const char kNonSensitiveRelUrl2[] = "/android/second.html";
const optimization_guide::PageContentAnnotationsResult kSensitiveResult =
    optimization_guide::PageContentAnnotationsResult::
        CreateContentVisibilityScoreResult(0.1);
const optimization_guide::PageContentAnnotationsResult kNonSensitiveResult =
    optimization_guide::PageContentAnnotationsResult::
        CreateContentVisibilityScoreResult(0.7);
const optimization_guide::PageContentAnnotationsResult kNonSensitiveResult2 =
    optimization_guide::PageContentAnnotationsResult::
        CreateContentVisibilityScoreResult(0.8);
}  // namespace

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
  EXPECT_TRUE(deserialized.is_sensitive());
  Deserialize(&deserialized, *serialized.get());
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
  content::NavigateToURL(web_contents(), sensitive_url);

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
  content::NavigateToURL(web_contents(), non_sensitive_url);

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

  content::NavigateToURL(web_contents(), sensitive_url);
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
