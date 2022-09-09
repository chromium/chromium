// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/reduce_accept_language/reduce_accept_language_factory.h"

#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "components/language/core/browser/language_prefs.h"
#include "components/language/core/browser/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/reduce_accept_language/browser/reduce_accept_language_service.h"
#include "components/reduce_accept_language/browser/reduce_accept_language_service_test_util.h"
#include "content/public/test/browser_task_environment.h"
#include "url/gurl.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using reduce_accept_language::test::ReduceAcceptLanguageServiceTester;

namespace reduce_accept_language {

class ReduceAcceptLanguageServiceTest : public testing::Test {
 public:
  ReduceAcceptLanguageServiceTest()
      : profile_(std::make_unique<TestingProfile>()) {}

  ReduceAcceptLanguageServiceTest(const ReduceAcceptLanguageServiceTest&) =
      delete;
  ReduceAcceptLanguageServiceTest& operator=(
      const ReduceAcceptLanguageServiceTest&) = delete;

  void SetUp() override {
    service_tester_ = std::make_unique<ReduceAcceptLanguageServiceTester>(
        settings_map(), service(), prefs());
    language::LanguagePrefs(prefs()).SetUserSelectedLanguagesList(
        {"en", "ja", "it"});
  }

  TestingProfile* profile() { return profile_.get(); }

  HostContentSettingsMap* settings_map() {
    return HostContentSettingsMapFactory::GetForProfile(profile());
  }

  PrefService* prefs() { return profile()->GetPrefs(); }

  ReduceAcceptLanguageService* service() {
    return ReduceAcceptLanguageFactory::GetForProfile(profile());
  }

  content::BrowserTaskEnvironment* task_environment() {
    return &task_environment_;
  }

  ReduceAcceptLanguageServiceTester* tester() { return service_tester_.get(); }

 private:
  std::unique_ptr<ReduceAcceptLanguageServiceTester> service_tester_;
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  std::unique_ptr<TestingProfile> profile_;
};

TEST_F(ReduceAcceptLanguageServiceTest, GetAcceptLanguageList) {
  tester()->VerifyFetchAcceptLanguageList({"en", "ja", "it"});
  ReduceAcceptLanguageService incognito_service(settings_map(), prefs(), true);
  // Verify incognito mode only has first accept language.
  EXPECT_EQ(std::vector<std::string>{"en"},
            incognito_service.GetUserAcceptLanguages());
}

TEST_F(ReduceAcceptLanguageServiceTest, PersistLanguageFail) {
  tester()->VerifyPersistFail(GURL("ws://example.com/"), "Zh-CN");
}

TEST_F(ReduceAcceptLanguageServiceTest,
       PersistLanguageSuccessJavaScriptNotEnabled) {
  tester()->VerifyPersistSuccessOnJavaScriptDisable(
      GURL("https://example.com/"), "Zh-CN");
}

TEST_F(ReduceAcceptLanguageServiceTest, PersistLanguageSuccess) {
  tester()->VerifyPersistSuccess(GURL("https://example.com/"), "Zh-CN");
  task_environment()->RunUntilIdle();
}

TEST_F(ReduceAcceptLanguageServiceTest, PersistLanguageMultipleHosts) {
  tester()->VerifyPersistMultipleHostsSuccess(
      {GURL("https://example1.com/"), GURL("https://example2.com/"),
       GURL("http://example.com/")},
      {"en-US", "es-MX", "zh-CN"});
}

}  // namespace reduce_accept_language
