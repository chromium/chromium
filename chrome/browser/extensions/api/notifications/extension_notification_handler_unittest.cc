// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/notifications/extension_notification_handler.h"

#include <optional>
#include <string>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/common/extension_id.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {
namespace {

static const char kChromeExtensionOrigin[] =
    "chrome-extension://gclcddgeeaknflkijpcbplmhbkonmlij/";
static const char kChromeExtensionId[] = "gclcddgeeaknflkijpcbplmhbkonmlij";
static const char kChromeNotificationId[] =
    "gclcddgeeaknflkijpcbplmhbkonmlij-id1";

class TestExtensionNotificationHandler : public ExtensionNotificationHandler {
 public:
  // Set expected arguments for this test handler.
  void SetTestExpectations(const ExtensionId& extension_id,
                           const std::string& event_name,
                           size_t param_count) {
    extension_id_ = extension_id;
    event_name_ = event_name;
    param_count_ = param_count;
  }

 protected:
  void SendEvent(Profile* profile,
                 const ExtensionId& extension_id,
                 events::HistogramValue histogram_value,
                 const std::string& event_name,
                 EventRouter::UserGestureState user_gesture,
                 base::Value::List args) final {
    EXPECT_EQ(event_name_, event_name);
    EXPECT_EQ(extension_id_, extension_id);
    EXPECT_EQ(param_count_, args.size());
  }

 private:
  ExtensionId extension_id_;
  std::string event_name_;
  size_t param_count_;
};
}  // namespace

class ExtensionNotificationHandlerTest : public testing::Test {
 public:
  ExtensionNotificationHandlerTest() {}

 private:
  content::BrowserTaskEnvironment task_environment_;
};

TEST_F(ExtensionNotificationHandlerTest, CloseHandler) {
  EXPECT_TRUE(true);
  TestingProfile::Builder builder;
  std::unique_ptr<TestingProfile> profile = builder.Build();

  TestExtensionNotificationHandler handler;
  handler.SetTestExpectations(kChromeExtensionId, "notifications.onClosed", 2);
  handler.OnClose(profile.get(), GURL(kChromeExtensionOrigin),
                  kChromeNotificationId, false /* by_user */,
                  base::DoNothing());
}

TEST_F(ExtensionNotificationHandlerTest, ClickHandler) {
  EXPECT_TRUE(true);
  TestingProfile::Builder builder;
  std::unique_ptr<TestingProfile> profile = builder.Build();

  TestExtensionNotificationHandler handler;
  handler.SetTestExpectations(kChromeExtensionId, "notifications.onClicked", 1);
  handler.OnClick(profile.get(), GURL(kChromeExtensionOrigin),
                  kChromeNotificationId, std::nullopt /* action_index */,
                  std::nullopt /* reply */, base::DoNothing());
}

TEST_F(ExtensionNotificationHandlerTest, ClickHandlerButton) {
  EXPECT_TRUE(true);
  TestingProfile::Builder builder;
  std::unique_ptr<TestingProfile> profile = builder.Build();

  TestExtensionNotificationHandler handler;
  handler.SetTestExpectations(kChromeExtensionId,
                              "notifications.onButtonClicked", 2);
  handler.OnClick(profile.get(), GURL(kChromeExtensionOrigin),
                  kChromeNotificationId, 1 /* action_index */,
                  std::nullopt /* reply */, base::DoNothing());
}

}  // namespace extensions
