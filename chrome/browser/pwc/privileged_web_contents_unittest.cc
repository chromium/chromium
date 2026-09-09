// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/pwc/privileged_web_contents.h"

#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/pwc/pwc_component_policy.h"
#include "chrome/browser/pwc/pwc_features.mojom-features.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/input/native_web_keyboard_event.h"
#include "content/public/browser/file_select_listener.h"
#include "content/public/browser/media_stream_request.h"
#include "content/public/browser/preloading.h"
#include "content/public/browser/preloading_trigger_type.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/common/drop_data.h"
#include "content/public/test/test_renderer_host.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "third_party/blink/public/common/page/drag_operation.h"
#include "third_party/blink/public/mojom/choosers/file_chooser.mojom.h"
#include "third_party/blink/public/mojom/mediastream/media_stream.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace pwc {
namespace {

std::unique_ptr<FixedPwcPolicyDelegate> MakeTestDelegate() {
  return std::make_unique<FixedPwcPolicyDelegate>(
      std::vector<url::Origin>{
          url::Origin::Create(GURL("https://pwc-test.example.com"))},
      std::vector<url::Origin>{
          url::Origin::Create(GURL("https://pwc-test.example.com"))});
}

class PrivilegedWebContentsTest : public ChromeRenderViewHostTestHarness {
 public:
  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(
        mojom::features::kPrivilegedWebContents);
    ChromeRenderViewHostTestHarness::SetUp();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(PrivilegedWebContentsTest, CreateOwnsAWebContents) {
  std::unique_ptr<PrivilegedWebContents> pwc = PrivilegedWebContents::Create(
      PrivilegedComponent::kTestComponent, profile(), MakeTestDelegate());
  ASSERT_TRUE(pwc);
  ASSERT_TRUE(pwc->web_contents());
  EXPECT_EQ(pwc->web_contents()->GetBrowserContext(), profile());
  EXPECT_EQ(pwc->component(), PrivilegedComponent::kTestComponent);
  EXPECT_EQ(pwc->policy().component(), PrivilegedComponent::kTestComponent);
  EXPECT_EQ(pwc->web_contents()->GetDelegate(), pwc.get());
}

TEST_F(PrivilegedWebContentsTest, UsesDefaultStoragePartition) {
  std::unique_ptr<PrivilegedWebContents> pwc = PrivilegedWebContents::Create(
      PrivilegedComponent::kTestComponent, profile(), MakeTestDelegate());
  content::RenderProcessHost* process =
      pwc->web_contents()->GetPrimaryMainFrame()->GetProcess();
  EXPECT_EQ(process->GetStoragePartition(),
            profile()->GetDefaultStoragePartition());
}

TEST_F(PrivilegedWebContentsTest, FromWebContentsRoundTrips) {
  std::unique_ptr<PrivilegedWebContents> pwc = PrivilegedWebContents::Create(
      PrivilegedComponent::kTestComponent, profile(), MakeTestDelegate());
  EXPECT_EQ(PrivilegedWebContents::FromWebContents(pwc->web_contents()),
            pwc.get());
}

TEST_F(PrivilegedWebContentsTest, FromWebContentsIsNullForOrdinaryContents) {
  // The harness's own WebContents is not owned by a PrivilegedWebContents.
  EXPECT_EQ(PrivilegedWebContents::FromWebContents(web_contents()), nullptr);
  EXPECT_EQ(PrivilegedWebContents::FromWebContents(nullptr), nullptr);
}

TEST_F(PrivilegedWebContentsTest, DisablesPrerendering) {
  std::unique_ptr<PrivilegedWebContents> pwc = PrivilegedWebContents::Create(
      PrivilegedComponent::kTestComponent, profile(), MakeTestDelegate());
  content::WebContents* web_contents = pwc->web_contents();
  // A privileged WebContents never supports prerendering: a prerendered page is
  // activated into the primary main frame without running navigation throttles,
  // which would let an off-allowlist page bypass PwcNavigationThrottle.
  EXPECT_EQ(
      content::PreloadingEligibility::kPreloadingUnsupportedByWebContents,
      web_contents->GetDelegate()->IsPrerender2Supported(
          *web_contents, content::PreloadingTriggerType::kSpeculationRule));
}

TEST_F(PrivilegedWebContentsTest, DestructionIsClean) {
  std::unique_ptr<PrivilegedWebContents> pwc = PrivilegedWebContents::Create(
      PrivilegedComponent::kTestComponent, profile(), MakeTestDelegate());
  pwc.reset();
  // No crash, and unrelated WebContents are unaffected.
  EXPECT_EQ(PrivilegedWebContents::FromWebContents(web_contents()), nullptr);
}

class TestFileSelectListener : public content::FileSelectListener {
 public:
  TestFileSelectListener() = default;

  bool file_selected() const { return file_selected_; }
  bool canceled() const { return canceled_; }

  void FileSelected(std::vector<blink::mojom::FileChooserFileInfoPtr> files,
                    const base::FilePath& base_dir,
                    blink::mojom::FileChooserParams::Mode mode) override {
    file_selected_ = true;
  }

  void FileSelectionCanceled() override { canceled_ = true; }

 protected:
  ~TestFileSelectListener() override = default;

 private:
  bool file_selected_ = false;
  bool canceled_ = false;
};

class TestEmbedderDelegate : public PrivilegedWebContents::EmbedderDelegate {
 public:
  bool HandleKeyboardEvent(
      content::WebContents* source,
      const input::NativeWebKeyboardEvent& event) override {
    last_keyboard_source_ = source;
    last_event_type_ = event.GetType();
    keyboard_event_count_++;
    return handle_keyboard_return_value_;
  }

  void ContentsZoomChange(bool zoom_in) override {
    last_zoom_in_ = zoom_in;
    zoom_change_count_++;
  }

  void RequestMediaAccessPermission(
      content::WebContents* web_contents,
      const content::MediaStreamRequest& request,
      content::MediaResponseCallback callback) override {
    last_media_request_source_ = web_contents;
    last_media_request_ = request;
    media_request_count_++;
    if (should_drop_media_callback_) {
      return;
    }
    std::move(callback).Run(blink::mojom::StreamDevicesSet(),
                            media_request_result_,
                            /*ui=*/nullptr);
  }

  bool CheckMediaAccessPermission(content::RenderFrameHost* render_frame_host,
                                  const url::Origin& security_origin,
                                  blink::mojom::MediaStreamType type) override {
    last_media_check_rfh_ = render_frame_host;
    last_media_check_origin_ = security_origin;
    last_media_check_type_ = type;
    media_check_count_++;
    return check_media_access_return_value_;
  }

  void RunFileChooser(content::RenderFrameHost* render_frame_host,
                      scoped_refptr<content::FileSelectListener> listener,
                      const blink::mojom::FileChooserParams& params) override {
    last_file_chooser_rfh_ = render_frame_host;
    last_file_chooser_mode_ = params.mode;
    file_chooser_count_++;
    if (should_drop_file_chooser_listener_) {
      return;
    }
    if (should_select_file_) {
      listener->FileSelected({}, base::FilePath(), params.mode);
    } else {
      listener->FileSelectionCanceled();
    }
  }

  bool CanDragEnter(content::WebContents* source,
                    const content::DropData& data,
                    blink::DragOperationsMask operations_allowed) override {
    last_drag_source_ = source;
    last_drag_operations_allowed_ = operations_allowed;
    drag_enter_count_++;
    return can_drag_enter_return_value_;
  }

  raw_ptr<content::WebContents> last_keyboard_source_ = nullptr;
  std::optional<blink::WebInputEvent::Type> last_event_type_;
  int keyboard_event_count_ = 0;
  bool handle_keyboard_return_value_ = true;
  std::optional<bool> last_zoom_in_;
  int zoom_change_count_ = 0;

  raw_ptr<content::WebContents> last_media_request_source_ = nullptr;
  std::optional<content::MediaStreamRequest> last_media_request_;
  int media_request_count_ = 0;
  bool should_drop_media_callback_ = false;
  blink::mojom::MediaStreamRequestResult media_request_result_ =
      blink::mojom::MediaStreamRequestResult::OK;

  raw_ptr<content::RenderFrameHost> last_media_check_rfh_ = nullptr;
  std::optional<url::Origin> last_media_check_origin_;
  std::optional<blink::mojom::MediaStreamType> last_media_check_type_;
  int media_check_count_ = 0;
  bool check_media_access_return_value_ = true;

  raw_ptr<content::RenderFrameHost> last_file_chooser_rfh_ = nullptr;
  std::optional<blink::mojom::FileChooserParams::Mode> last_file_chooser_mode_;
  int file_chooser_count_ = 0;
  bool should_drop_file_chooser_listener_ = false;
  bool should_select_file_ = false;

  raw_ptr<content::WebContents, DisableDanglingPtrDetection> last_drag_source_ =
      nullptr;
  std::optional<blink::DragOperationsMask> last_drag_operations_allowed_;
  int drag_enter_count_ = 0;
  bool can_drag_enter_return_value_ = true;
};

content::MediaResponseCallback BindResultToFuture(
    base::test::TestFuture<blink::mojom::MediaStreamRequestResult>& future) {
  return base::BindOnce(
      [](base::OnceCallback<void(blink::mojom::MediaStreamRequestResult)> cb,
         const blink::mojom::StreamDevicesSet& stream_devices_set,
         blink::mojom::MediaStreamRequestResult result,
         std::unique_ptr<content::MediaStreamUI> ui) {
        std::move(cb).Run(result);
      },
      future.GetCallback());
}

TEST_F(PrivilegedWebContentsTest, EmbedderDelegateDefaultsToNull) {
  std::unique_ptr<PrivilegedWebContents> pwc = PrivilegedWebContents::Create(
      PrivilegedComponent::kTestComponent, profile(), MakeTestDelegate());
  EXPECT_EQ(pwc->embedder_delegate(), nullptr);
}

TEST_F(PrivilegedWebContentsTest, SetEmbedderDelegateUpdatesDelegate) {
  std::unique_ptr<PrivilegedWebContents> pwc = PrivilegedWebContents::Create(
      PrivilegedComponent::kTestComponent, profile(), MakeTestDelegate());
  TestEmbedderDelegate delegate;

  pwc->SetEmbedderDelegate(&delegate);
  EXPECT_EQ(pwc->embedder_delegate(), &delegate);

  pwc->SetEmbedderDelegate(nullptr);
  EXPECT_EQ(pwc->embedder_delegate(), nullptr);
}

TEST_F(PrivilegedWebContentsTest, ForwardsKeyboardEventToEmbedderDelegate) {
  std::unique_ptr<PrivilegedWebContents> pwc = PrivilegedWebContents::Create(
      PrivilegedComponent::kTestComponent, profile(), MakeTestDelegate());
  TestEmbedderDelegate delegate;
  content::WebContents* web_contents = pwc->web_contents();
  input::NativeWebKeyboardEvent event(blink::WebInputEvent::Type::kRawKeyDown,
                                      blink::WebInputEvent::kNoModifiers,
                                      base::TimeTicks::Now());

  // Returns false when no embedder delegate is set.
  EXPECT_FALSE(pwc->web_contents()->GetDelegate()->HandleKeyboardEvent(
      web_contents, event));

  pwc->SetEmbedderDelegate(&delegate);

  delegate.handle_keyboard_return_value_ = true;
  EXPECT_TRUE(pwc->web_contents()->GetDelegate()->HandleKeyboardEvent(
      web_contents, event));
  EXPECT_EQ(delegate.last_keyboard_source_, web_contents);
  EXPECT_EQ(delegate.last_event_type_, blink::WebInputEvent::Type::kRawKeyDown);
  EXPECT_EQ(delegate.keyboard_event_count_, 1);

  delegate.handle_keyboard_return_value_ = false;
  EXPECT_FALSE(pwc->web_contents()->GetDelegate()->HandleKeyboardEvent(
      web_contents, event));
  EXPECT_EQ(delegate.keyboard_event_count_, 2);

  // Clearing the delegate stops forwarding.
  pwc->SetEmbedderDelegate(nullptr);
  EXPECT_FALSE(pwc->web_contents()->GetDelegate()->HandleKeyboardEvent(
      web_contents, event));
  EXPECT_EQ(delegate.keyboard_event_count_, 2);
}

TEST_F(PrivilegedWebContentsTest,
       ForwardsContentsZoomChangeToEmbedderDelegate) {
  std::unique_ptr<PrivilegedWebContents> pwc = PrivilegedWebContents::Create(
      PrivilegedComponent::kTestComponent, profile(), MakeTestDelegate());
  TestEmbedderDelegate delegate;

  // Does not crash when no embedder delegate is set.
  pwc->web_contents()->GetDelegate()->ContentsZoomChange(/*zoom_in=*/true);

  pwc->SetEmbedderDelegate(&delegate);

  pwc->web_contents()->GetDelegate()->ContentsZoomChange(/*zoom_in=*/true);
  EXPECT_EQ(delegate.zoom_change_count_, 1);
  EXPECT_EQ(delegate.last_zoom_in_, true);

  pwc->web_contents()->GetDelegate()->ContentsZoomChange(/*zoom_in=*/false);
  EXPECT_EQ(delegate.zoom_change_count_, 2);
  EXPECT_EQ(delegate.last_zoom_in_, false);

  // Clearing the delegate stops forwarding.
  pwc->SetEmbedderDelegate(nullptr);
  pwc->web_contents()->GetDelegate()->ContentsZoomChange(/*zoom_in=*/true);
  EXPECT_EQ(delegate.zoom_change_count_, 2);
}

TEST_F(PrivilegedWebContentsTest,
       ForwardsRequestMediaAccessPermissionToEmbedderDelegate) {
  std::unique_ptr<PrivilegedWebContents> pwc = PrivilegedWebContents::Create(
      PrivilegedComponent::kTestComponent, profile(), MakeTestDelegate());
  TestEmbedderDelegate delegate;
  content::WebContents* web_contents = pwc->web_contents();
  content::RenderFrameHost* main_rfh = web_contents->GetPrimaryMainFrame();
  content::MediaStreamRequest request(
      main_rfh->GetProcess()->GetDeprecatedID(), main_rfh->GetRoutingID(),
      /*page_request_id=*/0,
      url::Origin::Create(GURL("https://pwc.example.com")),
      /*user_gesture=*/true, blink::MEDIA_DEVICE_ACCESS,
      /*requested_audio_device_ids=*/{}, /*requested_video_device_ids=*/{},
      blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE,
      blink::mojom::MediaStreamType::NO_SERVICE,
      /*disable_local_echo=*/false,
      /*request_pan_tilt_zoom_permission=*/false,
      /*captured_surface_control_active=*/false);

  // When no embedder delegate is set, callback runs with NOT_SUPPORTED.
  {
    base::test::TestFuture<blink::mojom::MediaStreamRequestResult> future;
    pwc->web_contents()->GetDelegate()->RequestMediaAccessPermission(
        web_contents, request, BindResultToFuture(future));
    EXPECT_EQ(future.Get(),
              blink::mojom::MediaStreamRequestResult::NOT_SUPPORTED);
  }

  // Setting the delegate forwards the request and preserves parameters.
  pwc->SetEmbedderDelegate(&delegate);
  delegate.media_request_result_ = blink::mojom::MediaStreamRequestResult::OK;
  {
    base::test::TestFuture<blink::mojom::MediaStreamRequestResult> future;
    pwc->web_contents()->GetDelegate()->RequestMediaAccessPermission(
        web_contents, request, BindResultToFuture(future));
    EXPECT_EQ(delegate.media_request_count_, 1);
    EXPECT_EQ(delegate.last_media_request_source_, web_contents);
    ASSERT_TRUE(delegate.last_media_request_.has_value());
    EXPECT_EQ(delegate.last_media_request_->security_origin,
              request.security_origin);
    EXPECT_EQ(delegate.last_media_request_->audio_type, request.audio_type);
    EXPECT_EQ(delegate.last_media_request_->video_type, request.video_type);
    EXPECT_EQ(future.Get(), blink::mojom::MediaStreamRequestResult::OK);
  }

  // If the embedder delegate drops the callback, PWC's safe wrapper runs
  // the callback with NOT_SUPPORTED to avoid hanging the renderer.
  {
    delegate.should_drop_media_callback_ = true;
    base::test::TestFuture<blink::mojom::MediaStreamRequestResult> future;
    pwc->web_contents()->GetDelegate()->RequestMediaAccessPermission(
        web_contents, request, BindResultToFuture(future));
    EXPECT_EQ(delegate.media_request_count_, 2);
    EXPECT_EQ(future.Get(),
              blink::mojom::MediaStreamRequestResult::NOT_SUPPORTED);
    delegate.should_drop_media_callback_ = false;
  }

  // Clearing the delegate reverts to default NOT_SUPPORTED.
  pwc->SetEmbedderDelegate(nullptr);
  {
    base::test::TestFuture<blink::mojom::MediaStreamRequestResult> future;
    pwc->web_contents()->GetDelegate()->RequestMediaAccessPermission(
        web_contents, request, BindResultToFuture(future));
    EXPECT_EQ(delegate.media_request_count_, 2);
    EXPECT_EQ(future.Get(),
              blink::mojom::MediaStreamRequestResult::NOT_SUPPORTED);
  }
}

TEST_F(PrivilegedWebContentsTest,
       RequestMediaAccessPermission_RejectsNonPrimaryMainFrame) {
  NavigateAndCommit(GURL("https://example.com"));
  std::unique_ptr<PrivilegedWebContents> pwc = PrivilegedWebContents::Create(
      PrivilegedComponent::kTestComponent, profile(), MakeTestDelegate());
  TestEmbedderDelegate delegate;
  pwc->SetEmbedderDelegate(&delegate);
  content::WebContents* pwc_contents = pwc->web_contents();

  // 1. Rejects a subframe (not in primary main frame).
  content::RenderFrameHost* subframe =
      content::RenderFrameHostTester::For(main_rfh())->AppendChild("subframe");
  ASSERT_TRUE(subframe);
  content::MediaStreamRequest subframe_request(
      subframe->GetProcess()->GetDeprecatedID(), subframe->GetRoutingID(),
      /*page_request_id=*/0,
      url::Origin::Create(GURL("https://pwc.example.com")),
      /*user_gesture=*/true, blink::MEDIA_DEVICE_ACCESS,
      /*requested_audio_device_ids=*/{}, /*requested_video_device_ids=*/{},
      blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE,
      blink::mojom::MediaStreamType::NO_SERVICE,
      /*disable_local_echo=*/false,
      /*request_pan_tilt_zoom_permission=*/false,
      /*captured_surface_control_active=*/false);
  {
    base::test::TestFuture<blink::mojom::MediaStreamRequestResult> future;
    pwc_contents->GetDelegate()->RequestMediaAccessPermission(
        pwc_contents, subframe_request, BindResultToFuture(future));
    EXPECT_EQ(future.Get(),
              blink::mojom::MediaStreamRequestResult::NOT_SUPPORTED);
    EXPECT_EQ(delegate.media_request_count_, 0);
  }

  // 2. Rejects invalid IDs (no RFH found).
  content::MediaStreamRequest invalid_request(
      /*render_process_id=*/99999, /*render_frame_id=*/99999,
      /*page_request_id=*/0,
      url::Origin::Create(GURL("https://pwc.example.com")),
      /*user_gesture=*/true, blink::MEDIA_DEVICE_ACCESS,
      /*requested_audio_device_ids=*/{}, /*requested_video_device_ids=*/{},
      blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE,
      blink::mojom::MediaStreamType::NO_SERVICE,
      /*disable_local_echo=*/false,
      /*request_pan_tilt_zoom_permission=*/false,
      /*captured_surface_control_active=*/false);
  {
    base::test::TestFuture<blink::mojom::MediaStreamRequestResult> future;
    pwc_contents->GetDelegate()->RequestMediaAccessPermission(
        pwc_contents, invalid_request, BindResultToFuture(future));
    EXPECT_EQ(future.Get(),
              blink::mojom::MediaStreamRequestResult::NOT_SUPPORTED);
    EXPECT_EQ(delegate.media_request_count_, 0);
  }

  // 3. Rejects RFH from a different WebContents even if it is a primary main
  // frame.
  content::RenderFrameHost* unrelated_rfh = main_rfh();
  content::MediaStreamRequest unrelated_request(
      unrelated_rfh->GetProcess()->GetDeprecatedID(),
      unrelated_rfh->GetRoutingID(),
      /*page_request_id=*/0,
      url::Origin::Create(GURL("https://pwc.example.com")),
      /*user_gesture=*/true, blink::MEDIA_DEVICE_ACCESS,
      /*requested_audio_device_ids=*/{}, /*requested_video_device_ids=*/{},
      blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE,
      blink::mojom::MediaStreamType::NO_SERVICE,
      /*disable_local_echo=*/false,
      /*request_pan_tilt_zoom_permission=*/false,
      /*captured_surface_control_active=*/false);
  {
    base::test::TestFuture<blink::mojom::MediaStreamRequestResult> future;
    pwc_contents->GetDelegate()->RequestMediaAccessPermission(
        pwc_contents, unrelated_request, BindResultToFuture(future));
    EXPECT_EQ(future.Get(),
              blink::mojom::MediaStreamRequestResult::NOT_SUPPORTED);
    EXPECT_EQ(delegate.media_request_count_, 0);
  }
}

TEST_F(PrivilegedWebContentsTest,
       RequestMediaAccessPermission_RejectionIsAsynchronous) {
  std::unique_ptr<PrivilegedWebContents> pwc = PrivilegedWebContents::Create(
      PrivilegedComponent::kTestComponent, profile(), MakeTestDelegate());
  TestEmbedderDelegate delegate;
  content::WebContents* pwc_contents = pwc->web_contents();

  content::MediaStreamRequest invalid_request(
      /*render_process_id=*/99999, /*render_frame_id=*/99999,
      /*page_request_id=*/0,
      url::Origin::Create(GURL("https://pwc.example.com")),
      /*user_gesture=*/true, blink::MEDIA_DEVICE_ACCESS,
      /*requested_audio_device_ids=*/{}, /*requested_video_device_ids=*/{},
      blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE,
      blink::mojom::MediaStreamType::NO_SERVICE,
      /*disable_local_echo=*/false,
      /*request_pan_tilt_zoom_permission=*/false,
      /*captured_surface_control_active=*/false);

  base::test::TestFuture<blink::mojom::MediaStreamRequestResult> future;
  pwc_contents->GetDelegate()->RequestMediaAccessPermission(
      pwc_contents, invalid_request, BindResultToFuture(future));

  // The callback must not be invoked synchronously.
  EXPECT_FALSE(future.IsReady());

  // Wait for the asynchronous dispatch using future.Get().
  EXPECT_EQ(future.Get(),
            blink::mojom::MediaStreamRequestResult::NOT_SUPPORTED);
  EXPECT_TRUE(future.IsReady());
}

TEST_F(PrivilegedWebContentsTest,
       ForwardsCheckMediaAccessPermissionToEmbedderDelegate) {
  std::unique_ptr<PrivilegedWebContents> pwc = PrivilegedWebContents::Create(
      PrivilegedComponent::kTestComponent, profile(), MakeTestDelegate());
  TestEmbedderDelegate delegate;
  content::RenderFrameHost* rfh = pwc->web_contents()->GetPrimaryMainFrame();
  url::Origin origin = url::Origin::Create(GURL("https://pwc.example.com"));

  // Returns false when no embedder delegate is set.
  EXPECT_FALSE(pwc->web_contents()->GetDelegate()->CheckMediaAccessPermission(
      rfh, origin, blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE));

  // Null render frame host is rejected outright.
  EXPECT_FALSE(pwc->web_contents()->GetDelegate()->CheckMediaAccessPermission(
      nullptr, origin, blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE));

  pwc->SetEmbedderDelegate(&delegate);
  delegate.check_media_access_return_value_ = true;
  EXPECT_TRUE(pwc->web_contents()->GetDelegate()->CheckMediaAccessPermission(
      rfh, origin, blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE));
  EXPECT_EQ(delegate.media_check_count_, 1);
  EXPECT_EQ(delegate.last_media_check_rfh_, rfh);
  EXPECT_EQ(delegate.last_media_check_origin_, origin);
  EXPECT_EQ(delegate.last_media_check_type_,
            blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE);

  delegate.check_media_access_return_value_ = false;
  EXPECT_FALSE(pwc->web_contents()->GetDelegate()->CheckMediaAccessPermission(
      rfh, origin, blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE));
  EXPECT_EQ(delegate.media_check_count_, 2);

  // Clearing the delegate stops forwarding and returns false.
  pwc->SetEmbedderDelegate(nullptr);
  EXPECT_FALSE(pwc->web_contents()->GetDelegate()->CheckMediaAccessPermission(
      rfh, origin, blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE));
  EXPECT_EQ(delegate.media_check_count_, 2);
}

TEST_F(PrivilegedWebContentsTest,
       CheckMediaAccessPermission_RejectsNonPrimaryMainFrame) {
  NavigateAndCommit(GURL("https://example.com"));
  std::unique_ptr<PrivilegedWebContents> pwc = PrivilegedWebContents::Create(
      PrivilegedComponent::kTestComponent, profile(), MakeTestDelegate());
  TestEmbedderDelegate delegate;
  pwc->SetEmbedderDelegate(&delegate);
  url::Origin origin = url::Origin::Create(GURL("https://pwc.example.com"));

  // 1. Subframe is rejected.
  content::RenderFrameHost* subframe =
      content::RenderFrameHostTester::For(main_rfh())->AppendChild("subframe");
  ASSERT_TRUE(subframe);
  EXPECT_FALSE(pwc->web_contents()->GetDelegate()->CheckMediaAccessPermission(
      subframe, origin, blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE));
  EXPECT_EQ(delegate.media_check_count_, 0);

  // 2. Unrelated WebContents RFH is rejected.
  content::RenderFrameHost* unrelated_rfh = main_rfh();
  EXPECT_FALSE(pwc->web_contents()->GetDelegate()->CheckMediaAccessPermission(
      unrelated_rfh, origin,
      blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE));
  EXPECT_EQ(delegate.media_check_count_, 0);
}

TEST_F(PrivilegedWebContentsTest, ForwardsRunFileChooserToEmbedderDelegate) {
  // Declare pwc before delegate so that delegate (and its
  // last_file_chooser_rfh_ pointer) is destroyed first, preventing a dangling
  // pointer.
  std::unique_ptr<PrivilegedWebContents> pwc = PrivilegedWebContents::Create(
      PrivilegedComponent::kTestComponent, profile(), MakeTestDelegate());
  TestEmbedderDelegate delegate;
  content::RenderFrameHost* rfh = pwc->web_contents()->GetPrimaryMainFrame();
  blink::mojom::FileChooserParams params;
  params.mode = blink::mojom::FileChooserParams::Mode::kOpen;

  // 1. When no embedder delegate is set, cancels file selection.
  {
    auto listener = base::MakeRefCounted<TestFileSelectListener>();
    pwc->web_contents()->GetDelegate()->RunFileChooser(rfh, listener, params);
    EXPECT_TRUE(listener->canceled());
    EXPECT_FALSE(listener->file_selected());
  }

  pwc->SetEmbedderDelegate(&delegate);

  // 2. Embedder delegate handles and cancels file selection.
  {
    delegate.should_select_file_ = false;
    auto listener = base::MakeRefCounted<TestFileSelectListener>();
    pwc->web_contents()->GetDelegate()->RunFileChooser(rfh, listener, params);
    EXPECT_TRUE(listener->canceled());
    EXPECT_FALSE(listener->file_selected());
    EXPECT_EQ(delegate.file_chooser_count_, 1);
    EXPECT_EQ(delegate.last_file_chooser_rfh_, rfh);
    EXPECT_EQ(delegate.last_file_chooser_mode_,
              blink::mojom::FileChooserParams::Mode::kOpen);
  }

  // 3. Embedder delegate handles and selects files.
  {
    delegate.should_select_file_ = true;
    auto listener = base::MakeRefCounted<TestFileSelectListener>();
    pwc->web_contents()->GetDelegate()->RunFileChooser(rfh, listener, params);
    EXPECT_TRUE(listener->file_selected());
    EXPECT_FALSE(listener->canceled());
    EXPECT_EQ(delegate.file_chooser_count_, 2);
  }

  // 4. Embedder delegate drops listener without running;
  // ScopedFileSelectListener ensures FileSelectionCanceled is still invoked.
  {
    delegate.should_drop_file_chooser_listener_ = true;
    auto listener = base::MakeRefCounted<TestFileSelectListener>();
    pwc->web_contents()->GetDelegate()->RunFileChooser(rfh, listener, params);
    EXPECT_TRUE(listener->canceled());
    EXPECT_FALSE(listener->file_selected());
    EXPECT_EQ(delegate.file_chooser_count_, 3);
  }

  // 5. Clearing the delegate stops forwarding and cancels selection.
  pwc->SetEmbedderDelegate(nullptr);
  {
    auto listener = base::MakeRefCounted<TestFileSelectListener>();
    pwc->web_contents()->GetDelegate()->RunFileChooser(rfh, listener, params);
    EXPECT_TRUE(listener->canceled());
    EXPECT_FALSE(listener->file_selected());
    EXPECT_EQ(delegate.file_chooser_count_, 3);
  }
}

TEST_F(PrivilegedWebContentsTest, RunFileChooser_RejectsNonPrimaryMainFrame) {
  NavigateAndCommit(GURL("https://example.com"));
  std::unique_ptr<PrivilegedWebContents> pwc = PrivilegedWebContents::Create(
      PrivilegedComponent::kTestComponent, profile(), MakeTestDelegate());
  TestEmbedderDelegate delegate;
  pwc->SetEmbedderDelegate(&delegate);
  blink::mojom::FileChooserParams params;

  // 1. Subframe is rejected and cancelled.
  content::RenderFrameHost* subframe =
      content::RenderFrameHostTester::For(main_rfh())->AppendChild("subframe");
  ASSERT_TRUE(subframe);
  {
    auto listener = base::MakeRefCounted<TestFileSelectListener>();
    pwc->web_contents()->GetDelegate()->RunFileChooser(subframe, listener,
                                                       params);
    EXPECT_TRUE(listener->canceled());
    EXPECT_FALSE(listener->file_selected());
    EXPECT_EQ(delegate.file_chooser_count_, 0);
  }

  // 2. Unrelated WebContents RFH is rejected and cancelled.
  content::RenderFrameHost* unrelated_rfh = main_rfh();
  {
    auto listener = base::MakeRefCounted<TestFileSelectListener>();
    pwc->web_contents()->GetDelegate()->RunFileChooser(unrelated_rfh, listener,
                                                       params);
    EXPECT_TRUE(listener->canceled());
    EXPECT_FALSE(listener->file_selected());
    EXPECT_EQ(delegate.file_chooser_count_, 0);
  }

  // 3. Null RFH is rejected and cancelled.
  {
    auto listener = base::MakeRefCounted<TestFileSelectListener>();
    pwc->web_contents()->GetDelegate()->RunFileChooser(nullptr, listener,
                                                       params);
    EXPECT_TRUE(listener->canceled());
    EXPECT_FALSE(listener->file_selected());
    EXPECT_EQ(delegate.file_chooser_count_, 0);
  }
}

TEST_F(PrivilegedWebContentsTest, DefaultEmbedderDelegateMethods) {
  PrivilegedWebContents::EmbedderDelegate default_delegate;
  input::NativeWebKeyboardEvent event(blink::WebInputEvent::Type::kRawKeyDown,
                                      blink::WebInputEvent::kNoModifiers,
                                      base::TimeTicks::Now());

  EXPECT_FALSE(default_delegate.HandleKeyboardEvent(web_contents(), event));
  EXPECT_FALSE(default_delegate.HandleKeyboardEvent(/*source=*/nullptr, event));

  // Default ContentsZoomChange is a no-op that does not crash.
  default_delegate.ContentsZoomChange(/*zoom_in=*/true);
  default_delegate.ContentsZoomChange(/*zoom_in=*/false);

  // Default RequestMediaAccessPermission invokes callback with NOT_SUPPORTED.
  content::MediaStreamRequest request(
      /*render_process_id=*/0, /*render_frame_id=*/0, /*page_request_id=*/0,
      url::Origin::Create(GURL("https://pwc.example.com")),
      /*user_gesture=*/true, blink::MEDIA_DEVICE_ACCESS,
      /*requested_audio_device_ids=*/{}, /*requested_video_device_ids=*/{},
      blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE,
      blink::mojom::MediaStreamType::NO_SERVICE,
      /*disable_local_echo=*/false,
      /*request_pan_tilt_zoom_permission=*/false,
      /*captured_surface_control_active=*/false);
  base::test::TestFuture<blink::mojom::MediaStreamRequestResult> future;
  default_delegate.RequestMediaAccessPermission(web_contents(), request,
                                                BindResultToFuture(future));
  EXPECT_EQ(future.Get(),
            blink::mojom::MediaStreamRequestResult::NOT_SUPPORTED);

  // Default CheckMediaAccessPermission returns false.
  EXPECT_FALSE(default_delegate.CheckMediaAccessPermission(
      main_rfh(), url::Origin::Create(GURL("https://pwc.example.com")),
      blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE));

  // Default RunFileChooser invokes FileSelectionCanceled.
  {
    auto listener = base::MakeRefCounted<TestFileSelectListener>();
    blink::mojom::FileChooserParams file_params;
    default_delegate.RunFileChooser(main_rfh(), listener, file_params);
    EXPECT_TRUE(listener->canceled());
    EXPECT_FALSE(listener->file_selected());
  }

  // Default CanDragEnter returns false.
  content::DropData drop_data;
  EXPECT_FALSE(default_delegate.CanDragEnter(web_contents(), drop_data,
                                             blink::kDragOperationCopy));
}

TEST_F(PrivilegedWebContentsTest, ForwardsCanDragEnterToEmbedderDelegate) {
  std::unique_ptr<PrivilegedWebContents> pwc = PrivilegedWebContents::Create(
      PrivilegedComponent::kTestComponent, profile(), MakeTestDelegate());
  TestEmbedderDelegate delegate;
  content::WebContents* pwc_contents = pwc->web_contents();
  content::DropData drop_data;
  blink::DragOperationsMask ops = blink::kDragOperationCopy;

  // 1. When no embedder delegate is set, returns false.
  EXPECT_FALSE(
      pwc_contents->GetDelegate()->CanDragEnter(pwc_contents, drop_data, ops));

  pwc->SetEmbedderDelegate(&delegate);

  // 2. Embedder delegate handles and returns true.
  delegate.can_drag_enter_return_value_ = true;
  EXPECT_TRUE(
      pwc_contents->GetDelegate()->CanDragEnter(pwc_contents, drop_data, ops));
  EXPECT_EQ(delegate.drag_enter_count_, 1);
  EXPECT_EQ(delegate.last_drag_source_, pwc_contents);
  EXPECT_EQ(delegate.last_drag_operations_allowed_, ops);

  // 3. Embedder delegate handles and returns false.
  delegate.can_drag_enter_return_value_ = false;
  EXPECT_FALSE(
      pwc_contents->GetDelegate()->CanDragEnter(pwc_contents, drop_data, ops));
  EXPECT_EQ(delegate.drag_enter_count_, 2);

  // 4. Clearing the delegate stops forwarding and returns false.
  pwc->SetEmbedderDelegate(nullptr);
  EXPECT_FALSE(
      pwc_contents->GetDelegate()->CanDragEnter(pwc_contents, drop_data, ops));
  EXPECT_EQ(delegate.drag_enter_count_, 2);
}

TEST_F(PrivilegedWebContentsTest, CanDragEnter_RejectsNonMatchingWebContents) {
  std::unique_ptr<PrivilegedWebContents> pwc = PrivilegedWebContents::Create(
      PrivilegedComponent::kTestComponent, profile(), MakeTestDelegate());
  TestEmbedderDelegate delegate;
  pwc->SetEmbedderDelegate(&delegate);
  content::DropData drop_data;
  blink::DragOperationsMask ops = blink::kDragOperationCopy;

  // 1. Unrelated WebContents is rejected.
  content::WebContents* unrelated_contents = web_contents();
  EXPECT_FALSE(pwc->web_contents()->GetDelegate()->CanDragEnter(
      unrelated_contents, drop_data, ops));
  EXPECT_EQ(delegate.drag_enter_count_, 0);

  // 2. Null WebContents is rejected.
  EXPECT_FALSE(pwc->web_contents()->GetDelegate()->CanDragEnter(
      nullptr, drop_data, ops));
  EXPECT_EQ(delegate.drag_enter_count_, 0);
}

}  // namespace
}  // namespace pwc
