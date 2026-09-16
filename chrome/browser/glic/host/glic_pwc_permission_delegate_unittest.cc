// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/host/glic_pwc_permission_delegate.h"

#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/permission_result.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/permissions/permission_status.mojom.h"

namespace glic {
namespace {

using PermissionStatus = blink::mojom::PermissionStatus;
using PermissionStatusSource = content::PermissionStatusSource;

class GlicPwcPermissionDelegateTest : public testing::Test {
 public:
  GlicPwcPermissionDelegateTest() : delegate_(&profile_) {}

  TestingProfile* profile() { return &profile_; }
  GlicPwcPermissionDelegate* delegate() { return &delegate_; }

 private:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  GlicPwcPermissionDelegate delegate_;
};

TEST_F(GlicPwcPermissionDelegateTest, MediaStreamMic_GrantedWhenPrefEnabled) {
  profile()->GetPrefs()->SetBoolean(prefs::kGlicMicrophoneEnabled, true);

  std::optional<content::PermissionResult> result =
      delegate()->GetPermissionStatus(/*render_frame_host=*/nullptr,
                                      ContentSettingsType::MEDIASTREAM_MIC);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(PermissionStatus::GRANTED, result->status);
  EXPECT_EQ(PermissionStatusSource::UNSPECIFIED, result->source);
}

TEST_F(GlicPwcPermissionDelegateTest, MediaStreamMic_DeniedWhenPrefDisabled) {
  profile()->GetPrefs()->SetBoolean(prefs::kGlicMicrophoneEnabled, false);

  std::optional<content::PermissionResult> result =
      delegate()->GetPermissionStatus(/*render_frame_host=*/nullptr,
                                      ContentSettingsType::MEDIASTREAM_MIC);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(PermissionStatus::DENIED, result->status);
  EXPECT_EQ(PermissionStatusSource::UNSPECIFIED, result->source);
}

TEST_F(GlicPwcPermissionDelegateTest, Geolocation_GrantedWhenPrefEnabled) {
  profile()->GetPrefs()->SetBoolean(prefs::kGlicGeolocationEnabled, true);

  std::optional<content::PermissionResult> result =
      delegate()->GetPermissionStatus(/*render_frame_host=*/nullptr,
                                      ContentSettingsType::GEOLOCATION);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(PermissionStatus::GRANTED, result->status);
  EXPECT_EQ(PermissionStatusSource::UNSPECIFIED, result->source);

  std::optional<content::PermissionResult> options_result =
      delegate()->GetPermissionStatus(
          /*render_frame_host=*/nullptr,
          ContentSettingsType::GEOLOCATION_WITH_OPTIONS);
  ASSERT_TRUE(options_result.has_value());
  EXPECT_EQ(PermissionStatus::GRANTED, options_result->status);
  EXPECT_EQ(PermissionStatusSource::UNSPECIFIED, options_result->source);
}

TEST_F(GlicPwcPermissionDelegateTest, Geolocation_DeniedWhenPrefDisabled) {
  profile()->GetPrefs()->SetBoolean(prefs::kGlicGeolocationEnabled, false);

  std::optional<content::PermissionResult> result =
      delegate()->GetPermissionStatus(/*render_frame_host=*/nullptr,
                                      ContentSettingsType::GEOLOCATION);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(PermissionStatus::DENIED, result->status);
  EXPECT_EQ(PermissionStatusSource::UNSPECIFIED, result->source);

  std::optional<content::PermissionResult> options_result =
      delegate()->GetPermissionStatus(
          /*render_frame_host=*/nullptr,
          ContentSettingsType::GEOLOCATION_WITH_OPTIONS);
  ASSERT_TRUE(options_result.has_value());
  EXPECT_EQ(PermissionStatus::DENIED, options_result->status);
  EXPECT_EQ(PermissionStatusSource::UNSPECIFIED, options_result->source);
}

TEST_F(GlicPwcPermissionDelegateTest, Clipboard_GrantedAlways) {
  std::optional<content::PermissionResult> read_write =
      delegate()->GetPermissionStatus(
          /*render_frame_host=*/nullptr,
          ContentSettingsType::CLIPBOARD_READ_WRITE);
  ASSERT_TRUE(read_write.has_value());
  EXPECT_EQ(PermissionStatus::GRANTED, read_write->status);
  EXPECT_EQ(PermissionStatusSource::UNSPECIFIED, read_write->source);

  std::optional<content::PermissionResult> sanitized_write =
      delegate()->GetPermissionStatus(
          /*render_frame_host=*/nullptr,
          ContentSettingsType::CLIPBOARD_SANITIZED_WRITE);
  ASSERT_TRUE(sanitized_write.has_value());
  EXPECT_EQ(PermissionStatus::GRANTED, sanitized_write->status);
  EXPECT_EQ(PermissionStatusSource::UNSPECIFIED, sanitized_write->source);
}

TEST_F(GlicPwcPermissionDelegateTest, OtherTypes_ReturnsNullopt) {
  EXPECT_FALSE(
      delegate()
          ->GetPermissionStatus(/*render_frame_host=*/nullptr,
                                ContentSettingsType::MEDIASTREAM_CAMERA)
          .has_value());
  EXPECT_FALSE(delegate()
                   ->GetPermissionStatus(/*render_frame_host=*/nullptr,
                                         ContentSettingsType::NOTIFICATIONS)
                   .has_value());
}

TEST_F(GlicPwcPermissionDelegateTest,
       NullProfile_ReturnsNulloptForUnsupportedTypesAndDeniedForPrefs) {
  GlicPwcPermissionDelegate null_delegate(nullptr);
  EXPECT_FALSE(null_delegate
                   .GetPermissionStatus(/*render_frame_host=*/nullptr,
                                        ContentSettingsType::MEDIASTREAM_CAMERA)
                   .has_value());
  EXPECT_FALSE(null_delegate
                   .GetPermissionStatus(/*render_frame_host=*/nullptr,
                                        ContentSettingsType::NOTIFICATIONS)
                   .has_value());

  std::optional<content::PermissionResult> mic =
      null_delegate.GetPermissionStatus(/*render_frame_host=*/nullptr,
                                        ContentSettingsType::MEDIASTREAM_MIC);
  ASSERT_TRUE(mic.has_value());
  EXPECT_EQ(PermissionStatus::DENIED, mic->status);

  std::optional<content::PermissionResult> geo =
      null_delegate.GetPermissionStatus(/*render_frame_host=*/nullptr,
                                        ContentSettingsType::GEOLOCATION);
  ASSERT_TRUE(geo.has_value());
  EXPECT_EQ(PermissionStatus::DENIED, geo->status);

  std::optional<content::PermissionResult> clipboard =
      null_delegate.GetPermissionStatus(
          /*render_frame_host=*/nullptr,
          ContentSettingsType::CLIPBOARD_READ_WRITE);
  ASSERT_TRUE(clipboard.has_value());
  EXPECT_EQ(PermissionStatus::GRANTED, clipboard->status);
}

}  // namespace
}  // namespace glic
