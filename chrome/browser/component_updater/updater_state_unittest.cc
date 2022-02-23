// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/updater_state.h"

#include "base/time/time.h"
#include "base/version.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace component_updater {

class UpdaterStateTest : public testing::Test {
 public:
  UpdaterStateTest() = default;

  UpdaterStateTest(const UpdaterStateTest&) = delete;
  UpdaterStateTest& operator=(const UpdaterStateTest&) = delete;

  ~UpdaterStateTest() override = default;
};

TEST_F(UpdaterStateTest, Serialize) {
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
  EXPECT_STREQ("0", UpdaterState::GetState(false).at("ismachine").c_str());
  EXPECT_STREQ("1", UpdaterState::GetState(true).at("ismachine").c_str());
#else
  EXPECT_TRUE(UpdaterState::GetState(false).empty());
  EXPECT_TRUE(UpdaterState::GetState(true).empty());
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
}

#if (BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)) && \
    BUILDFLAG(GOOGLE_CHROME_BRANDING)
TEST_F(UpdaterStateTest, SerializeChrome) {
  UpdaterState updater_state(false);

  updater_state.state_->updater_name = "the updater";
  updater_state.state_->updater_version = base::Version("1.0");
  updater_state.state_->last_autoupdate_started =
      base::Time::NowFromSystemTime();
  updater_state.state_->last_checked = base::Time::NowFromSystemTime();
  updater_state.state_->is_autoupdate_check_enabled = true;
  updater_state.state_->update_policy = 1;

  auto attributes = updater_state.Serialize();

  // Sanity check all members.
  EXPECT_STREQ("the updater", attributes.at("name").c_str());
  EXPECT_STREQ("1.0", attributes.at("version").c_str());
  EXPECT_STREQ("0", attributes.at("laststarted").c_str());
  EXPECT_STREQ("0", attributes.at("lastchecked").c_str());
  EXPECT_STREQ("1", attributes.at("autoupdatecheckenabled").c_str());
  EXPECT_STREQ("1", attributes.at("updatepolicy").c_str());

#if BUILDFLAG(IS_WIN)
  EXPECT_STREQ("Omaha", UpdaterState::GetState(false).at("name").c_str());
#elif BUILDFLAG(IS_MAC)
  EXPECT_STREQ("Keystone", UpdaterState::GetState(false).at("name").c_str());
#endif  // BUILDFLAG(IS_WIN)

  // Tests some of the remaining values.
  updater_state = UpdaterState(false);

  // Don't serialize an invalid version if it could not be read.
  updater_state.state_->updater_version = base::Version();
  attributes = updater_state.Serialize();
  EXPECT_EQ(0u, attributes.count("version"));

  updater_state.state_->updater_version = base::Version("0.0.0.0");
  attributes = updater_state.Serialize();
  EXPECT_STREQ("0.0.0.0", attributes.at("version").c_str());

  updater_state.state_->last_autoupdate_started =
      base::Time::NowFromSystemTime() - base::Days(15);
  attributes = updater_state.Serialize();
  EXPECT_STREQ("336", attributes.at("laststarted").c_str());

  updater_state.state_->last_autoupdate_started =
      base::Time::NowFromSystemTime() - base::Days(58);
  attributes = updater_state.Serialize();
  EXPECT_STREQ("1344", attributes.at("laststarted").c_str());

  updater_state.state_->last_autoupdate_started =
      base::Time::NowFromSystemTime() - base::Days(90);
  attributes = updater_state.Serialize();
  EXPECT_STREQ("1344", attributes.at("laststarted").c_str());

  // Don't serialize the time if it could not be read.
  updater_state.state_->last_autoupdate_started = base::Time();
  attributes = updater_state.Serialize();
  EXPECT_EQ(0u, attributes.count("laststarted"));

  updater_state.state_->last_checked =
      base::Time::NowFromSystemTime() - base::Days(15);
  attributes = updater_state.Serialize();
  EXPECT_STREQ("336", attributes.at("lastchecked").c_str());

  updater_state.state_->last_checked =
      base::Time::NowFromSystemTime() - base::Days(90);
  attributes = updater_state.Serialize();
  EXPECT_STREQ("1344", attributes.at("lastchecked").c_str());

  // Don't serialize the time if it could not be read (the value is invalid).
  updater_state.state_->last_checked = base::Time();
  attributes = updater_state.Serialize();
  EXPECT_EQ(0u, attributes.count("lastchecked"));

  updater_state.state_->is_autoupdate_check_enabled = false;
  attributes = updater_state.Serialize();
  EXPECT_STREQ("0", attributes.at("autoupdatecheckenabled").c_str());

  updater_state.state_->update_policy = 0;
  attributes = updater_state.Serialize();
  EXPECT_STREQ("0", attributes.at("updatepolicy").c_str());

  updater_state.state_->update_policy = -1;
  attributes = updater_state.Serialize();
  EXPECT_STREQ("-1", attributes.at("updatepolicy").c_str());
}

#endif  // (BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)) &&
        // BUILDFLAG(GOOGLE_CHROME_BRANDING)

}  // namespace component_updater
