// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/midi_permission_context.h"
#include "chrome/test/base/testing_profile.h"
#include "components/content_settings/core/common/content_settings.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class MidiPermissionContextTests : public testing::Test {
 public:
  TestingProfile* profile() { return &profile_; }

 private:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
};

// Web MIDI permission status should be allowed only for secure origins.
TEST_F(MidiPermissionContextTests, TestNoSysexAllowedAllOrigins) {
  MidiPermissionContext permission_context(profile());
  GURL insecure_url("http://www.example.com");
  GURL secure_url("https://www.example.com");

  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            permission_context
                .GetPermissionStatus(nullptr /* render_frame_host */,
                                     insecure_url, insecure_url)
                .content_setting);

  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            permission_context
                .GetPermissionStatus(nullptr /* render_frame_host */,
                                     insecure_url, secure_url)
                .content_setting);

  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            permission_context
                .GetPermissionStatus(nullptr /* render_frame_host */,
                                     secure_url, secure_url)
                .content_setting);
}

}  // namespace
