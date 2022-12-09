// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/notifications/idle_app_name_notification_view.h"

#include "base/command_line.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "extensions/common/manifest_constants.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash {

namespace {

const char kTestAppName[] = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";

}  // namespace

class IdleAppNameNotificationViewTest : public BrowserWithTestWindowTest {
 public:
  IdleAppNameNotificationViewTest()
      : BrowserWithTestWindowTest(Browser::TYPE_NORMAL) {}

  IdleAppNameNotificationViewTest(const IdleAppNameNotificationViewTest&) =
      delete;
  IdleAppNameNotificationViewTest& operator=(
      const IdleAppNameNotificationViewTest&) = delete;

  ~IdleAppNameNotificationViewTest() override {}

  void SetUp() override {
    // Add the application switch.
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        ::switches::kAppId, kTestAppName);

    BrowserWithTestWindowTest::SetUp();

    base::Value::Dict manifest;
    manifest.Set(extensions::manifest_keys::kName, "Test");
    manifest.Set(extensions::manifest_keys::kVersion, "1");
    manifest.Set(extensions::manifest_keys::kManifestVersion, 2);
    manifest.Set(extensions::manifest_keys::kDescription, "Test app");
    manifest.SetByDottedPath("author.email", "Someone");

    std::string error;
    correct_extension_ = extensions::Extension::Create(
        base::FilePath(), extensions::mojom::ManifestLocation::kUnpacked,
        manifest, extensions::Extension::NO_FLAGS, kTestAppName, &error);
    base::Value::Dict manifest2;
    manifest2.Set(extensions::manifest_keys::kName, "Test");
    manifest2.Set(extensions::manifest_keys::kVersion, "1");
    manifest2.Set(extensions::manifest_keys::kDescription, "Test app");

    incorrect_extension_ = extensions::Extension::Create(
        base::FilePath(), extensions::mojom::ManifestLocation::kUnpacked,
        manifest2, extensions::Extension::NO_FLAGS, kTestAppName, &error);
  }

  void TearDown() override {
    // The destruction of the widget might be a delayed task.
    base::RunLoop().RunUntilIdle();
    BrowserWithTestWindowTest::TearDown();
  }

  extensions::Extension* correct_extension() {
    return correct_extension_.get();
  }
  extensions::Extension* incorrect_extension() {
    return incorrect_extension_.get();
  }

 private:
  // Extensions to test with.
  scoped_refptr<extensions::Extension> correct_extension_;
  scoped_refptr<extensions::Extension> incorrect_extension_;
};

// Check that creating and immediate destroying does not crash (and closes the
// message).
TEST_F(IdleAppNameNotificationViewTest, CheckTooEarlyDestruction) {
  // Create a message which is visible for 10ms and fades in/out for 5ms.
  std::make_unique<IdleAppNameNotificationView>(10, 5, correct_extension());
}

// Check that the message gets created and it destroys itself after time.
TEST_F(IdleAppNameNotificationViewTest, CheckSelfDestruction) {
  // Create a message which is visible for 10ms and fades in/out for 5ms.
  auto message =
      std::make_unique<IdleAppNameNotificationView>(10, 5, correct_extension());
  EXPECT_TRUE(message->IsVisible());

  // Wait now for some time and see that it closes itself again.
  for (int i = 0; i < 50 && message->IsVisible(); i++) {
    sleep(1);
    base::RunLoop().RunUntilIdle();
  }
  EXPECT_FALSE(message->IsVisible());
}

// Check that the shown text for a correct application is correct.
TEST_F(IdleAppNameNotificationViewTest, CheckCorrectApp) {
  // Create a message which is visible for 10ms and fades in/out for 5ms.
  auto message =
      std::make_unique<IdleAppNameNotificationView>(10, 5, correct_extension());
  std::u16string text = message->GetShownTextForTest();
  // Check that the string is the application name.
  std::u16string name = u"Test";
  EXPECT_EQ(name, text.substr(0, name.length()));
}

// Check that an invalid app gets shown accordingly.
TEST_F(IdleAppNameNotificationViewTest, CheckInvalidApp) {
  // Create a message which is visible for 10ms and fades in/out for 5ms.
  auto message = std::make_unique<IdleAppNameNotificationView>(10, 5, nullptr);
  std::u16string text = message->GetShownTextForTest();
  std::u16string error = l10n_util::GetStringUTF16(
      IDS_IDLE_APP_NAME_UNKNOWN_APPLICATION_NOTIFICATION);
  EXPECT_EQ(error, text);
}

}  // namespace ash
