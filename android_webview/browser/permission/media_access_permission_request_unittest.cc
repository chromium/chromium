// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/permission/media_access_permission_request.h"

#include <memory>

#include "android_webview/browser/aw_context_permissions_delegate.h"
#include "android_webview/browser/aw_permission_manager.h"
#include "android_webview/common/aw_features.h"
#include "base/functional/bind.h"
#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/mediastream/media_stream_request.h"
#include "third_party/blink/public/mojom/mediastream/media_stream.mojom-shared.h"
#include "third_party/blink/public/mojom/mediastream/media_stream.mojom.h"

namespace android_webview {

class TestMediaAccessPermissionRequest : public MediaAccessPermissionRequest {
 public:
  TestMediaAccessPermissionRequest(
      const content::MediaStreamRequest& request,
      content::MediaResponseCallback callback,
      const blink::MediaStreamDevices& audio_devices,
      const blink::MediaStreamDevices& video_devices,
      AwPermissionManager& aw_permission_manager_,
      bool can_cache_file_url_permissions_)
      : MediaAccessPermissionRequest(request,
                                     std::move(callback),
                                     aw_permission_manager_,
                                     can_cache_file_url_permissions_) {
    audio_test_devices_ = audio_devices;
    video_test_devices_ = video_devices;
  }
};

class MockContextPermissionDelegate : public AwContextPermissionsDelegate {
 public:
  MockContextPermissionDelegate() = default;
  PermissionStatus GetGeolocationPermission(
      const GURL& requesting_origin) const override {
    return PermissionStatus::ASK;
  }
};

class MediaAccessPermissionRequestTest : public testing::Test {
 protected:
  void SetUp() override {
    audio_device_id_ = "audio";
    video_device_id_ = "video";
    first_audio_device_id_ = "audio1";
    first_video_device_id_ = "video1";
  }

  std::unique_ptr<TestMediaAccessPermissionRequest> CreateRequest(
      std::string audio_id,
      std::string video_id) {
    blink::MediaStreamDevices audio_devices;
    audio_devices.push_back(blink::MediaStreamDevice(
        blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE,
        first_audio_device_id_, "a2"));
    audio_devices.push_back(blink::MediaStreamDevice(
        blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE, audio_device_id_,
        "a1"));

    blink::MediaStreamDevices video_devices;
    video_devices.push_back(blink::MediaStreamDevice(
        blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE,
        first_video_device_id_, "v2"));
    video_devices.push_back(blink::MediaStreamDevice(
        blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE, video_device_id_,
        "v1"));

    GURL origin("https://www.google.com");
    content::MediaStreamRequest request(
        0, 0, 0, url::Origin::Create(origin), false,
        blink::MEDIA_GENERATE_STREAM, {audio_id}, {video_id},
        blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE,
        blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE,
        false /* disable_local_echo */,
        false /* request_pan_tilt_zoom_permission */,
        false /* captured_surface_control_active */);

    std::unique_ptr<TestMediaAccessPermissionRequest> permission_request;
    permission_request = std::make_unique<TestMediaAccessPermissionRequest>(
        request,
        base::BindOnce(&MediaAccessPermissionRequestTest::Callback,
                       base::Unretained(this)),
        audio_devices, video_devices, aw_permission_manager_, false);
    return permission_request;
  }

  std::string audio_device_id_;
  std::string video_device_id_;
  std::string first_audio_device_id_;
  std::string first_video_device_id_;
  blink::MediaStreamDevices devices_;
  blink::mojom::MediaStreamRequestResult result_;
  MockContextPermissionDelegate mock_permission_delegate_;
  AwPermissionManager aw_permission_manager_{mock_permission_delegate_};
  base::test::ScopedFeatureList feature_list_;

 private:
  void Callback(const blink::mojom::StreamDevicesSet& stream_devices_set,
                blink::mojom::MediaStreamRequestResult result,
                std::unique_ptr<content::MediaStreamUI> ui) {
    devices_ = blink::ToMediaStreamDevicesList(stream_devices_set);
    result_ = result;
  }
};

TEST_F(MediaAccessPermissionRequestTest, TestGrantPermissionRequest) {
  std::unique_ptr<TestMediaAccessPermissionRequest> request =
      CreateRequest(audio_device_id_, video_device_id_);
  request->NotifyRequestResult(true);

  EXPECT_EQ(2u, devices_.size());
  EXPECT_EQ(blink::mojom::MediaStreamRequestResult::OK, result_);

  bool audio_exist = false;
  bool video_exist = false;
  for (blink::MediaStreamDevices::iterator i = devices_.begin();
       i != devices_.end(); ++i) {
    if (i->type == blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE &&
        i->id == audio_device_id_) {
      audio_exist = true;
    } else if (i->type == blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE &&
               i->id == video_device_id_) {
      video_exist = true;
    }
  }
  EXPECT_TRUE(audio_exist);
  EXPECT_TRUE(video_exist);
}

TEST_F(MediaAccessPermissionRequestTest, TestGrantPermissionRequestWithoutID) {
  std::unique_ptr<TestMediaAccessPermissionRequest> request =
      CreateRequest(std::string(), std::string());
  request->NotifyRequestResult(true);

  EXPECT_EQ(2u, devices_.size());
  EXPECT_EQ(blink::mojom::MediaStreamRequestResult::OK, result_);

  bool audio_exist = false;
  bool video_exist = false;
  for (blink::MediaStreamDevices::iterator i = devices_.begin();
       i != devices_.end(); ++i) {
    if (i->type == blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE &&
        i->id == first_audio_device_id_) {
      audio_exist = true;
    } else if (i->type == blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE &&
               i->id == first_video_device_id_) {
      video_exist = true;
    }
  }
  EXPECT_TRUE(audio_exist);
  EXPECT_TRUE(video_exist);
}

TEST_F(MediaAccessPermissionRequestTest, TestDenyPermissionRequest) {
  std::unique_ptr<TestMediaAccessPermissionRequest> request =
      CreateRequest(std::string(), std::string());
  request->NotifyRequestResult(false);
  EXPECT_TRUE(devices_.empty());
  EXPECT_EQ(blink::mojom::MediaStreamRequestResult::PERMISSION_DENIED, result_);
}

TEST_F(MediaAccessPermissionRequestTest,
       TestGrantedPermissionRequestCachesResult) {
  url::Origin origin = url::Origin::Create(GURL("https://www.google.com"));
  EXPECT_FALSE(
      aw_permission_manager_.ShouldShowEnumerateDevicesAudioLabels(origin));
  EXPECT_FALSE(
      aw_permission_manager_.ShouldShowEnumerateDevicesVideoLabels(origin));
  std::unique_ptr<TestMediaAccessPermissionRequest> request =
      CreateRequest(audio_device_id_, video_device_id_);
  request->NotifyRequestResult(true);
  EXPECT_TRUE(
      aw_permission_manager_.ShouldShowEnumerateDevicesAudioLabels(origin));
  EXPECT_TRUE(
      aw_permission_manager_.ShouldShowEnumerateDevicesVideoLabels(origin));
}

TEST_F(MediaAccessPermissionRequestTest,
       TestGrantedPermissionRequestWithoutCacheFailsEnumerateDevices) {
  feature_list_.InitAndDisableFeature(features::kWebViewEnumerateDevicesCache);
  url::Origin origin = url::Origin::Create(GURL("https://www.google.com"));
  EXPECT_FALSE(
      aw_permission_manager_.ShouldShowEnumerateDevicesAudioLabels(origin));
  EXPECT_FALSE(
      aw_permission_manager_.ShouldShowEnumerateDevicesVideoLabels(origin));
  std::unique_ptr<TestMediaAccessPermissionRequest> request =
      CreateRequest(audio_device_id_, video_device_id_);
  request->NotifyRequestResult(true);
  EXPECT_FALSE(
      aw_permission_manager_.ShouldShowEnumerateDevicesAudioLabels(origin));
  EXPECT_FALSE(
      aw_permission_manager_.ShouldShowEnumerateDevicesVideoLabels(origin));
}
}  // namespace android_webview
