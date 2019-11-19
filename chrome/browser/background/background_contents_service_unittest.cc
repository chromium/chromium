// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/background/background_contents_service.h"

#include <memory>
#include <string>

#include "base/command_line.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "build/build_config.h"
#include "chrome/browser/background/background_contents.h"
#include "chrome/browser/background/background_contents_service_factory.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/notifications/notification_display_service_tester.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/common/extensions/extension_test_util.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "content/public/browser/notification_service.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_thread.h"
#include "extensions/common/extension.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"
#include "ui/message_center/public/cpp/notification.h"
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

  ~MockBackgroundContents() override = default;

  BackgroundContentsService* service() { return service_; }

  const std::string& appid() { return appid_; }

 private:
  GURL url_;

  BackgroundContentsService* service_;

  // The ID of our parent application
  std::string appid_;

  DISALLOW_COPY_AND_ASSIGN(MockBackgroundContents);
};

class BackgroundContentsServiceTest : public testing::Test {
 public:
  BackgroundContentsServiceTest() = default;
  ~BackgroundContentsServiceTest() override = default;

  void SetUp() override {
    command_line_.reset(new base::CommandLine(base::CommandLine::NO_PROGRAM));
    BackgroundContentsService::DisableCloseBalloonForTesting(true);
  }

  void TearDown() override {
    BackgroundContentsService::DisableCloseBalloonForTesting(false);
  }

  const base::DictionaryValue* GetPrefs(Profile* profile) {
    return profile->GetPrefs()->GetDictionary(
        prefs::kRegisteredBackgroundContents);
  }

  // Returns the stored pref URL for the passed app id.
  std::string GetPrefURLForApp(Profile* profile, const std::string& appid) {
    const base::DictionaryValue* pref = GetPrefs(profile);
    EXPECT_TRUE(pref->HasKey(appid));
    const base::DictionaryValue* value;
    pref->GetDictionaryWithoutPathExpansion(appid, &value);
    std::string url;
    value->GetString("url", &url);
    return url;
  }

  MockBackgroundContents* AddToService(
      std::unique_ptr<MockBackgroundContents> contents) {
    MockBackgroundContents* contents_ptr = contents.get();
    contents_ptr->service()->AddBackgroundContents(
        std::move(contents), contents_ptr->appid(), "background");
    return contents_ptr;
  }

  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<base::CommandLine> command_line_;

 private:
  DISALLOW_COPY_AND_ASSIGN(BackgroundContentsServiceTest);
};

class BackgroundContentsServiceNotificationTest
    : public BrowserWithTestWindowTest {
 public:
  BackgroundContentsServiceNotificationTest() {}
  ~BackgroundContentsServiceNotificationTest() override {}

  // Overridden from testing::Test
  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();
    display_service_ =
        std::make_unique<NotificationDisplayServiceTester>(profile());
  }

 protected:
  // Creates crash notification for the specified extension and returns
  // the created one.
  const message_center::Notification CreateCrashNotification(
      scoped_refptr<extensions::Extension> extension) {
    std::string notification_id = BackgroundContentsService::
        GetNotificationDelegateIdForExtensionForTesting(extension->id());
    BackgroundContentsService::ShowBalloonForTesting(extension.get(),
                                                     profile());
    base::RunLoop run_loop;
    display_service_->SetNotificationAddedClosure(run_loop.QuitClosure());
    run_loop.Run();
    display_service_->SetNotificationAddedClosure(base::RepeatingClosure());
    return *display_service_->GetNotification(notification_id);
  }

  std::unique_ptr<NotificationDisplayServiceTester> display_service_;

 private:
  DISALLOW_COPY_AND_ASSIGN(BackgroundContentsServiceNotificationTest);
};

TEST_F(BackgroundContentsServiceTest, Create) {
  // Check for creation and leaks.
  TestingProfile profile;
  BackgroundContentsService service(&profile, command_line_.get());
}

TEST_F(BackgroundContentsServiceTest, BackgroundContentsUrlAdded) {
  TestingProfile profile;
  BackgroundContentsService service(&profile, command_line_.get());

  GURL orig_url;
  GURL url("http://a/");
  GURL url2("http://a/");
  {
    std::unique_ptr<MockBackgroundContents> owned_contents(
        new MockBackgroundContents(&service));
    EXPECT_EQ(0U, GetPrefs(&profile)->size());
    auto* contents = AddToService(std::move(owned_contents));

    contents->Navigate(url);
    EXPECT_EQ(1U, GetPrefs(&profile)->size());
    EXPECT_EQ(url.spec(), GetPrefURLForApp(&profile, contents->appid()));

    // Navigate the contents to a new url, should not change url.
    contents->Navigate(url2);
    EXPECT_EQ(1U, GetPrefs(&profile)->size());
    EXPECT_EQ(url.spec(), GetPrefURLForApp(&profile, contents->appid()));
  }
  // Contents are deleted, url should persist.
  EXPECT_EQ(1U, GetPrefs(&profile)->size());
}

TEST_F(BackgroundContentsServiceTest, BackgroundContentsUrlAddedAndClosed) {
  TestingProfile profile;
  BackgroundContentsService service(&profile, command_line_.get());

  GURL url("http://a/");
  auto owned_contents = std::make_unique<MockBackgroundContents>(&service);
  EXPECT_EQ(0U, GetPrefs(&profile)->size());
  auto* contents = AddToService(std::move(owned_contents));
  contents->Navigate(url);
  EXPECT_EQ(1U, GetPrefs(&profile)->size());
  EXPECT_EQ(url.spec(), GetPrefURLForApp(&profile, contents->appid()));

  // Fake a window closed by script.
  contents->MockClose(&profile);
  EXPECT_EQ(0U, GetPrefs(&profile)->size());
}

// Test what happens if a BackgroundContents shuts down (say, due to a renderer
// crash) then is restarted. Should not persist URL twice.
TEST_F(BackgroundContentsServiceTest, RestartBackgroundContents) {
  TestingProfile profile;
  BackgroundContentsService service(&profile, command_line_.get());

  GURL url("http://a/");
  {
    MockBackgroundContents* contents = AddToService(
        std::make_unique<MockBackgroundContents>(&service, "appid"));
    contents->Navigate(url);
    EXPECT_EQ(1U, GetPrefs(&profile)->size());
    EXPECT_EQ(url.spec(), GetPrefURLForApp(&profile, contents->appid()));
  }
  // Contents deleted, url should be persisted.
  EXPECT_EQ(1U, GetPrefs(&profile)->size());

  {
    // Reopen the BackgroundContents to the same URL, we should not register the
    // URL again.
    MockBackgroundContents* contents = AddToService(
        std::make_unique<MockBackgroundContents>(&service, "appid"));
    contents->Navigate(url);
    EXPECT_EQ(1U, GetPrefs(&profile)->size());
  }
}

// Ensures that BackgroundContentsService properly tracks the association
// between a BackgroundContents and its parent extension, including
// unregistering the BC when the extension is uninstalled.
TEST_F(BackgroundContentsServiceTest, TestApplicationIDLinkage) {
  TestingProfile profile;
  BackgroundContentsService service(&profile, command_line_.get());

  EXPECT_EQ(NULL, service.GetAppBackgroundContents("appid"));
  MockBackgroundContents* contents =
      AddToService(std::make_unique<MockBackgroundContents>(&service, "appid"));
  MockBackgroundContents* contents2 = AddToService(
      std::make_unique<MockBackgroundContents>(&service, "appid2"));
  EXPECT_EQ(contents, service.GetAppBackgroundContents(contents->appid()));
  EXPECT_EQ(contents2, service.GetAppBackgroundContents(contents2->appid()));
  EXPECT_EQ(0U, GetPrefs(&profile)->size());

  // Navigate the contents, then make sure the one associated with the extension
  // is unregistered.
  GURL url("http://a/");
  GURL url2("http://b/");
  contents->Navigate(url);
  EXPECT_EQ(1U, GetPrefs(&profile)->size());
  contents2->Navigate(url2);
  EXPECT_EQ(2U, GetPrefs(&profile)->size());
  service.ShutdownAssociatedBackgroundContents("appid");
  EXPECT_FALSE(service.IsTracked(contents));
  EXPECT_EQ(NULL, service.GetAppBackgroundContents("appid"));
  EXPECT_EQ(1U, GetPrefs(&profile)->size());
  EXPECT_EQ(url2.spec(), GetPrefURLForApp(&profile, contents2->appid()));
}

TEST_F(BackgroundContentsServiceNotificationTest, TestShowBalloon) {
  scoped_refptr<extensions::Extension> extension =
      extension_test_util::LoadManifest("image_loading_tracker", "app.json");
  ASSERT_TRUE(extension.get());
  ASSERT_TRUE(extension->GetManifestData("icons"));

  const message_center::Notification notification =
      CreateCrashNotification(extension);
  EXPECT_FALSE(notification.icon().IsEmpty());
}

TEST_F(BackgroundContentsServiceNotificationTest, TestShowBalloonShutdown) {
  scoped_refptr<extensions::Extension> extension =
      extension_test_util::LoadManifest("image_loading_tracker", "app.json");
  ASSERT_TRUE(extension.get());
  ASSERT_TRUE(extension->GetManifestData("icons"));

  std::string notification_id = BackgroundContentsService::
      GetNotificationDelegateIdForExtensionForTesting(extension->id());

  static_cast<TestingBrowserProcess*>(g_browser_process)->SetShuttingDown(true);
  BackgroundContentsService::ShowBalloonForTesting(extension.get(), profile());
  base::RunLoop().RunUntilIdle();
  static_cast<TestingBrowserProcess*>(g_browser_process)
      ->SetShuttingDown(false);

  EXPECT_FALSE(display_service_->GetNotification(notification_id));
}

// Verify if a test notification can show the default extension icon for
// a crash notification for an extension without icon.
TEST_F(BackgroundContentsServiceNotificationTest, TestShowBalloonNoIcon) {
  // Extension manifest file with no 'icon' field.
  scoped_refptr<extensions::Extension> extension =
      extension_test_util::LoadManifest("app", "manifest.json");
  ASSERT_TRUE(extension.get());
  ASSERT_FALSE(extension->GetManifestData("icons"));

  const message_center::Notification notification =
      CreateCrashNotification(extension);
  EXPECT_FALSE(notification.icon().IsEmpty());
}

TEST_F(BackgroundContentsServiceNotificationTest, TestShowTwoBalloons) {
  TestingProfile profile;
  scoped_refptr<extensions::Extension> extension =
      extension_test_util::LoadManifest("app", "manifest.json");
  ASSERT_TRUE(extension.get());
  CreateCrashNotification(extension);
  CreateCrashNotification(extension);

  ASSERT_EQ(1u, display_service_
                    ->GetDisplayedNotificationsForType(
                        NotificationHandler::Type::TRANSIENT)
                    .size());
}
