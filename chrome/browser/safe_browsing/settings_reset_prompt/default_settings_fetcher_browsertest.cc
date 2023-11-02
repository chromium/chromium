// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/settings_reset_prompt/default_settings_fetcher.h"

#include <memory>

#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "chrome/browser/profile_resetter/brandcoded_default_settings.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace safe_browsing {
namespace {

class DefaultSettingsFetcherTest : public InProcessBrowserTest {
 public:
  void FetchedSettings(std::unique_ptr<BrandcodedDefaultSettings> settings) {
    EXPECT_FALSE(settings_);
    EXPECT_FALSE(fetched_settings_called);

    fetched_settings_called = true;
    settings_ = std::move(settings);
  }

 protected:
  bool fetched_settings_called = false;
  std::unique_ptr<BrandcodedDefaultSettings> settings_;
};

IN_PROC_BROWSER_TEST_F(DefaultSettingsFetcherTest, FetchingSettingsSucceeded) {
  // The default settings that we will pretend were fetched. Keep the raw
  // pointer here so that we can compare it to what was passed to the callback.
  BrandcodedDefaultSettings* default_settings = new BrandcodedDefaultSettings();

  DefaultSettingsFetcher::FetchDefaultSettingsForTesting(
      base::BindOnce(&DefaultSettingsFetcherTest::FetchedSettings,
                     base::Unretained(this)),
      base::WrapUnique(default_settings));

  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(fetched_settings_called);
  EXPECT_EQ(default_settings, settings_.get());
}

IN_PROC_BROWSER_TEST_F(DefaultSettingsFetcherTest, FetchingSettingsFailed) {
  // Pretend that fetching default settings failed by passing in a nullptr to
  // |FetchDefaultSettingsForTesting()|. The callback should still receive
  // default-constructed default settings.
  DefaultSettingsFetcher::FetchDefaultSettingsForTesting(
      base::BindOnce(&DefaultSettingsFetcherTest::FetchedSettings,
                     base::Unretained(this)),
      nullptr);

  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(fetched_settings_called);
  EXPECT_TRUE(settings_);
}

}  // namespace
}  // namespace safe_browsing
