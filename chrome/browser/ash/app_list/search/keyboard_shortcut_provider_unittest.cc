// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/keyboard_shortcut_provider.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/app_list/search/chrome_search_result.h"
#include "chrome/browser/ash/app_list/search/test/test_search_controller.h"
#include "chrome/test/base/chrome_ash_test_base.h"
#include "chrome/test/base/testing_profile.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/devices/device_data_manager_test_api.h"

namespace app_list::test {

namespace {
constexpr double kResultRelevanceThreshold = 0.89;
}

class KeyboardShortcutProviderTest : public ChromeAshTestBase {
 public:
  KeyboardShortcutProviderTest() = default;

 protected:
  void SetUp() override {
    ChromeAshTestBase::SetUp();
    // A DCHECK inside a KSV metadata utility function relies on device lists
    // being complete.
    ui::DeviceDataManagerTestApi().OnDeviceListsComplete();

    profile_ = std::make_unique<TestingProfile>();
    search_controller_ = std::make_unique<TestSearchController>();
    auto provider = std::make_unique<KeyboardShortcutProvider>(profile_.get());
    provider_ = provider.get();
    search_controller_->AddProvider(std::move(provider));

    Wait();
  }

  void Wait() { task_environment()->RunUntilIdle(); }

  const SearchProvider::Results& results() {
    return search_controller_->last_results();
  }

  void StartSearch(const std::u16string& query) {
    search_controller_->StartSearch(query);
  }

  std::unique_ptr<Profile> profile_;
  std::unique_ptr<TestSearchController> search_controller_;
  KeyboardShortcutProvider* provider_ = nullptr;
};

// Make search queries which yield shortcut results with shortcut key
// combinations of differing length and format. Check that the top result has a
// high relevance score, and correctly set title and accessible name.
TEST_F(KeyboardShortcutProviderTest, Search) {
  // Result format: Single Key
  StartSearch(u"overview mode");
  Wait();

  ASSERT_FALSE(results().empty());
  EXPECT_EQ(results()[0]->title(), u"Overview mode");
  EXPECT_GT(results()[0]->relevance(), kResultRelevanceThreshold);
  EXPECT_EQ(results()[0]->accessible_name(),
            u"Overview mode, Shortcuts, Overview mode key");

  // Result format: Modifier + Key
  StartSearch(u"lock");
  Wait();

  ASSERT_FALSE(results().empty());
  EXPECT_EQ(results()[0]->title(), u"Lock screen");
  EXPECT_GT(results()[0]->relevance(), kResultRelevanceThreshold);
  EXPECT_EQ(results()[0]->accessible_name(),
            u"Lock screen, Shortcuts, Search+ l");

  // Result format: Modifier1 + Modifier2 + Key
  StartSearch(u"previous tab");
  Wait();

  ASSERT_FALSE(results().empty());
  EXPECT_EQ(results()[0]->title(), u"Go to previous tab");
  EXPECT_GT(results()[0]->relevance(), kResultRelevanceThreshold);
  EXPECT_EQ(results()[0]->accessible_name(),
            u"Go to previous tab, Shortcuts, Ctrl+ Shift+ Tab");

  // Result format: Modifier1 + Key1 or Modifier2 + Key2
  StartSearch(u"focus address");
  Wait();

  ASSERT_FALSE(results().empty());
  EXPECT_EQ(results()[0]->title(), u"Focus address bar");
  EXPECT_GT(results()[0]->relevance(), kResultRelevanceThreshold);
  EXPECT_EQ(results()[0]->accessible_name(),
            u"Focus address bar, Shortcuts, Ctrl+ l or Alt+ d");

  // Result format: Custom template string which embeds a Modifier and a Key.
  StartSearch(u"switch quickly between windows");
  Wait();

  ASSERT_FALSE(results().empty());
  EXPECT_EQ(results()[0]->title(), u"Switch quickly between windows");
  EXPECT_GT(results()[0]->relevance(), kResultRelevanceThreshold);
  EXPECT_EQ(
      results()[0]->accessible_name(),
      u"Switch quickly between windows, Shortcuts, Press and hold Alt, tap Tab "
      u"until you get to the window you want to open, then release.");

  // Result format: Special case result for Take screenshot/recording.
  StartSearch(u"take screenshot");
  Wait();

  ASSERT_FALSE(results().empty());
  EXPECT_EQ(results()[0]->title(), u"Take screenshot/recording");
  EXPECT_GT(results()[0]->relevance(), kResultRelevanceThreshold);
  EXPECT_EQ(results()[0]->accessible_name(),
            u"Take screenshot/recording, Shortcuts, Capture mode key or Ctrl+ "
            u"Shift+ Overview mode key");

  // Result format: Order variation result for Dim keyboard.
  StartSearch(u"keyboard dim");
  Wait();

  ASSERT_FALSE(results().empty());
  EXPECT_EQ(results()[0]->title(),
            u"Dim keyboard (for backlit keyboards only)");
  EXPECT_GT(results()[0]->relevance(), kResultRelevanceThreshold);
  EXPECT_EQ(results()[0]->accessible_name(),
            u"Dim keyboard (for backlit keyboards only), Shortcuts, Alt+ "
            u"BrightnessDown");

  // Result format: special case result for Open emoji picker.
  StartSearch(u"emoji");
  Wait();

  ASSERT_FALSE(results().empty());
  EXPECT_EQ(results()[0]->title(), u"Open Emoji Picker");
  EXPECT_GT(results()[0]->relevance(), kResultRelevanceThreshold);
  EXPECT_EQ(results()[0]->accessible_name(),
            u"Open Emoji Picker, Shortcuts, Shift+ Search+ Space");
}

}  // namespace app_list::test
