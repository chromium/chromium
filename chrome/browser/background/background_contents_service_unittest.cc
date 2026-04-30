// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/background/background_contents_service.h"

#include <memory>
#include <string>

#include "base/command_line.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/run_until.h"
#include "base/threading/thread_restrictions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/background/background_contents.h"
#include "chrome/browser/background/background_contents_service_factory.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/extension_test_util.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/common/extension.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

class MockBackgroundContents : public BackgroundContents {
 public:
  MockBackgroundContents(BackgroundContentsService* service,
                         const std::string& id)
      : service_(service), appid_(id) {}
  explicit MockBackgroundContents(BackgroundContentsService* service)
      : MockBackgroundContents(service, "app_id") {}

  void Navigate(GURL url) {
    url_ = url;
    service_->OnBackgroundContentsNavigated(this);
  }
  const GURL& GetURL() const override { return url_; }

  void MockClose(Profile* profile) {
    service_->OnBackgroundContentsClosed(this);
  }

  MockBackgroundContents(const MockBackgroundContents&) = delete;
  MockBackgroundContents& operator=(const MockBackgroundContents&) = delete;

  ~MockBackgroundContents() override = default;

  BackgroundContentsService* service() { return service_; }

  const std::string& appid() { return appid_; }

 private:
  GURL url_;

  raw_ptr<BackgroundContentsService> service_;

  // The ID of our parent application
  std::string appid_;
};

class BackgroundContentsServiceTest : public testing::Test {
 public:
  BackgroundContentsServiceTest() = default;

  BackgroundContentsServiceTest(const BackgroundContentsServiceTest&) = delete;
  BackgroundContentsServiceTest& operator=(
      const BackgroundContentsServiceTest&) = delete;

  ~BackgroundContentsServiceTest() override = default;

  void SetUp() override {
    testing::Test::SetUp();
    BackgroundContentsService::DisableCloseBalloonForTesting(true);
    profile_manager_ =
        TestingBrowserProcess::GetGlobal()->SetUpGlobalFeaturesForTesting(
            /*profile_manager=*/true);
    profile_ = profile_manager_->CreateTestingProfile("default");

    extensions::TestExtensionSystem* system =
        static_cast<extensions::TestExtensionSystem*>(
            extensions::ExtensionSystem::Get(profile_));
    system->CreateExtensionService(base::CommandLine::ForCurrentProcess(),
                                   base::FilePath(), false);
  }

  void TearDown() override {
    profile_ = nullptr;
    profile_manager_ = nullptr;
    TestingBrowserProcess::GetGlobal()->TearDownGlobalFeaturesForTesting();
    BackgroundContentsService::DisableCloseBalloonForTesting(false);
    testing::Test::TearDown();
  }

  const base::DictValue& GetPrefs(Profile* profile) {
    return profile->GetPrefs()->GetDict(prefs::kRegisteredBackgroundContents);
  }

  // Returns the stored pref URL for the passed app id.
  std::string GetPrefURLForApp(Profile* profile, const std::string& appid) {
    const base::DictValue& pref = GetPrefs(profile);
    const base::DictValue* value = pref.FindDict(appid);
    EXPECT_TRUE(value);
    const std::string* url = value->FindString("url");
    return url ? *url : std::string();
  }

  MockBackgroundContents* AddToService(
      std::unique_ptr<MockBackgroundContents> contents) {
    MockBackgroundContents* contents_ptr = contents.get();
    contents_ptr->service()->AddBackgroundContents(
        std::move(contents), contents_ptr->appid(), "background");
    return contents_ptr;
  }

 protected:
  content::BrowserTaskEnvironment task_environment_{
      content::BrowserTaskEnvironment::TimeSource::MOCK_TIME};
  raw_ptr<TestingProfileManager> profile_manager_ = nullptr;
  raw_ptr<TestingProfile> profile_ = nullptr;
};

TEST_F(BackgroundContentsServiceTest, Create) {
  // Check for creation and leaks.
  BackgroundContentsService service(profile_);
}

TEST_F(BackgroundContentsServiceTest, BackgroundContentsUrlAdded) {
  BackgroundContentsService service(profile_);

  GURL orig_url;
  GURL url("http://a/");
  GURL url2("http://a/");
  {
    auto owned_contents = std::make_unique<MockBackgroundContents>(&service);
    EXPECT_EQ(0U, GetPrefs(profile_).size());
    auto* contents = AddToService(std::move(owned_contents));

    contents->Navigate(url);
    EXPECT_EQ(1U, GetPrefs(profile_).size());
    EXPECT_EQ(url.spec(), GetPrefURLForApp(profile_, contents->appid()));

    // Navigate the contents to a new url, should not change url.
    contents->Navigate(url2);
    EXPECT_EQ(1U, GetPrefs(profile_).size());
    EXPECT_EQ(url.spec(), GetPrefURLForApp(profile_, contents->appid()));
  }
  // Contents are deleted, url should persist.
  EXPECT_EQ(1U, GetPrefs(profile_).size());
}

TEST_F(BackgroundContentsServiceTest, BackgroundContentsUrlAddedAndClosed) {
  BackgroundContentsService service(profile_);

  GURL url("http://a/");
  auto owned_contents = std::make_unique<MockBackgroundContents>(&service);
  EXPECT_EQ(0U, GetPrefs(profile_).size());
  auto* contents = AddToService(std::move(owned_contents));
  contents->Navigate(url);
  EXPECT_EQ(1U, GetPrefs(profile_).size());
  EXPECT_EQ(url.spec(), GetPrefURLForApp(profile_, contents->appid()));

  // Fake a window closed by script.
  contents->MockClose(profile_);
  EXPECT_EQ(0U, GetPrefs(profile_).size());
}

// Test what happens if a BackgroundContents shuts down (say, due to a renderer
// crash) then is restarted. Should not persist URL twice.
TEST_F(BackgroundContentsServiceTest, RestartBackgroundContents) {
  BackgroundContentsService service(profile_);

  GURL url("http://a/");
  {
    MockBackgroundContents* contents = AddToService(
        std::make_unique<MockBackgroundContents>(&service, "appid"));
    contents->Navigate(url);
    EXPECT_EQ(1U, GetPrefs(profile_).size());
    EXPECT_EQ(url.spec(), GetPrefURLForApp(profile_, contents->appid()));
  }
  // Contents deleted, url should be persisted.
  EXPECT_EQ(1U, GetPrefs(profile_).size());

  {
    // Reopen the BackgroundContents to the same URL, we should not register the
    // URL again.
    MockBackgroundContents* contents = AddToService(
        std::make_unique<MockBackgroundContents>(&service, "appid"));
    contents->Navigate(url);
    EXPECT_EQ(1U, GetPrefs(profile_).size());
  }
}

// Ensures that BackgroundContentsService properly tracks the association
// between a BackgroundContents and its parent extension, including
// unregistering the BC when the extension is uninstalled.
TEST_F(BackgroundContentsServiceTest, TestApplicationIDLinkage) {
  BackgroundContentsService service(profile_);

  EXPECT_EQ(nullptr, service.GetAppBackgroundContents("appid"));
  MockBackgroundContents* contents =
      AddToService(std::make_unique<MockBackgroundContents>(&service, "appid"));
  MockBackgroundContents* contents2 = AddToService(
      std::make_unique<MockBackgroundContents>(&service, "appid2"));
  EXPECT_EQ(contents, service.GetAppBackgroundContents(contents->appid()));
  EXPECT_EQ(contents2, service.GetAppBackgroundContents(contents2->appid()));
  EXPECT_EQ(0U, GetPrefs(profile_).size());

  // Navigate the contents, then make sure the one associated with the extension
  // is unregistered.
  GURL url("http://a/");
  GURL url2("http://b/");
  contents->Navigate(url);
  EXPECT_EQ(1U, GetPrefs(profile_).size());
  contents2->Navigate(url2);
  EXPECT_EQ(2U, GetPrefs(profile_).size());
  service.ShutdownAssociatedBackgroundContents("appid");
  EXPECT_FALSE(service.IsTracked(contents));
  EXPECT_EQ(nullptr, service.GetAppBackgroundContents("appid"));
  EXPECT_EQ(1U, GetPrefs(profile_).size());
  EXPECT_EQ(url2.spec(), GetPrefURLForApp(profile_, contents2->appid()));
}

// Regression test for crash. See http://crbug.com/477597409
TEST_F(BackgroundContentsServiceTest, RestartForceInstalledExtensionOnCrash) {
  auto service = std::make_unique<BackgroundContentsService>(profile_);

  // Create a trivial extension.
  scoped_refptr<extensions::Extension> extension;
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    extension = extension_test_util::LoadManifest("app", "manifest.json");
  }
  ASSERT_TRUE(extension.get());

  // Calling restart will post a task (with a 3 second delay).
  service->RestartForceInstalledExtensionOnCrash(extension.get());

  // Before the task runs, delete the service (which in production is owned by
  // the profile).
  service.reset();

  // Fast forward the task environment by 3 seconds so the restart task runs.
  task_environment_.FastForwardBy(base::Seconds(3));

  // No crash.
}

// Test that ensures that background contents are correctly restored from
// preferences, specifically checking that the URL and frame name are not
// swapped. Regression test for crbug.com/499051898.
TEST_F(BackgroundContentsServiceTest, RestoreFromPrefs) {
  BackgroundContentsService service(profile_);

  // Manually set up the preference.
  const std::string appid = "appid";
  const GURL expected_url("http://www.google.com/test");

  {
    ScopedDictPrefUpdate update(profile_->GetPrefs(),
                                prefs::kRegisteredBackgroundContents);
    base::DictValue dict;
    dict.Set("url", expected_url.spec());
    dict.Set("name", "test_frame");
    update->Set(appid, std::move(dict));
  }

  // Load the background contents for the extension.
  service.LoadBackgroundContentsForExtension(appid);

  BackgroundContents* contents = service.GetAppBackgroundContents(appid);
  ASSERT_TRUE(contents);
  EXPECT_EQ(expected_url, contents->GetInitialURLForTesting());
}
