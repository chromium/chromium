// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/widget/glic_side_panel_ui_android.h"

#include <memory>
#include <vector>

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/glic/host/host.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/browser/glic/service/glic_ui_embedder.h"
#include "chrome/browser/glic/service/metrics/glic_instance_metrics.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/testing_profile.h"
#include "components/metrics/profile_metrics_service.h"
#include "components/tabs/public/mock_tab_interface.h"
#include "content/public/browser/media_stream_request.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/drop_data.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/mediastream/media_stream_request.h"
#include "third_party/blink/public/common/page/drag_operation.h"
#include "third_party/blink/public/mojom/mediastream/media_stream.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace glic {
namespace {

class FakeGlicUiEmbedderDelegate : public GlicUiEmbedder::Delegate {
 public:
  explicit FakeGlicUiEmbedderDelegate(Host& host) : host_(host) {}
  ~FakeGlicUiEmbedderDelegate() override = default;

  void OnEmbedderWindowActivationChanged(bool has_focus) override {
    activation_changes_.push_back(has_focus);
  }
  void SwitchConversation(
      const ShowOptions& options,
      glic::mojom::ConversationInfoPtr info,
      mojom::WebClientHandler::SwitchConversationCallback callback) override {}
  void DidCloseFor(EmbedderKey key, EmbedderCloseReason reason) override {}
  Host& host() override { return *host_; }
  void Show(ShowOptions options) override {}
  void Detach(tabs::TabInterface& tab) override {}
  void Attach(tabs::TabHandle tab) override {}
  void NotifyPanelStateChanged() override {}

  const std::vector<bool>& activation_changes() const {
    return activation_changes_;
  }

 private:
  raw_ref<Host> host_;
  std::vector<bool> activation_changes_;
};

// Records the arguments the media request was answered with.
class MediaResponseRecorder {
 public:
  content::MediaResponseCallback GetCallback() {
    return base::BindOnce(&MediaResponseRecorder::OnResponse,
                          base::Unretained(this));
  }

  bool responded() const { return responded_; }
  blink::mojom::MediaStreamRequestResult result() const { return result_; }

 private:
  void OnResponse(const blink::mojom::StreamDevicesSet& stream_devices_set,
                  blink::mojom::MediaStreamRequestResult result,
                  std::unique_ptr<content::MediaStreamUI> ui) {
    responded_ = true;
    result_ = result;
  }

  bool responded_ = false;
  blink::mojom::MediaStreamRequestResult result_ =
      blink::mojom::MediaStreamRequestResult::OK;
};

content::MediaStreamRequest MakeAudioRequest() {
  return content::MediaStreamRequest(
      /*render_process_id=*/0, /*render_frame_id=*/0, /*page_request_id=*/0,
      url::Origin::Create(GURL("https://example.com")), /*user_gesture=*/true,
      blink::MEDIA_DEVICE_ACCESS, /*requested_audio_device_ids=*/{},
      /*requested_video_device_ids=*/{},
      blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE,
      blink::mojom::MediaStreamType::NO_SERVICE, /*disable_local_echo=*/false,
      /*request_pan_tilt_zoom_permission=*/false,
      /*captured_surface_control_active=*/false);
}

}  // namespace

class GlicSidePanelUiAndroidTest : public testing::Test {
 public:
  GlicSidePanelUiAndroidTest()
      : host_(&profile_, nullptr, nullptr, nullptr),
        delegate_(host_),
        instance_metrics_(&profile_metrics_service_, &profile_) {}
  ~GlicSidePanelUiAndroidTest() override = default;

  TestingProfile* profile() { return &profile_; }
  FakeGlicUiEmbedderDelegate& delegate() { return delegate_; }
  GlicInstanceMetrics& instance_metrics() { return instance_metrics_; }
  base::WeakPtr<tabs::TabInterface> tab() {
    return tab_weak_factory_.GetWeakPtr();
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  metrics::ProfileMetricsService profile_metrics_service_;
  Host host_;
  FakeGlicUiEmbedderDelegate delegate_;
  GlicInstanceMetrics instance_metrics_;
  tabs::MockTabInterface mock_tab_;
  base::WeakPtrFactory<tabs::TabInterface> tab_weak_factory_{&mock_tab_};
};

TEST_F(GlicSidePanelUiAndroidTest, CanDragEnter) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures({features::kGlicDragAndDropFileUpload,
                                 features::kGlicDragAndDropFileUploadAndroid},
                                {});

  GlicSidePanelUi side_panel_ui(profile(), base::WeakPtr<tabs::TabInterface>(),
                                delegate(), instance_metrics());

  content::DropData drop_data;
  blink::DragOperationsMask ops = blink::kDragOperationCopy;

  // Empty DropData should be rejected.
  EXPECT_FALSE(side_panel_ui.CanDragEnter(nullptr, drop_data, ops));

  // DropData with files should be accepted.
  drop_data.filenames.emplace_back(
      base::FilePath(FILE_PATH_LITERAL("test.txt")),
      base::FilePath(FILE_PATH_LITERAL("test.txt")));
  EXPECT_TRUE(side_panel_ui.CanDragEnter(nullptr, drop_data, ops));

  // DropData with file_system_files should be accepted.
  drop_data.filenames.clear();
  drop_data.file_system_files.emplace_back(
      GURL("filesystem:http://example.com/test.txt"), 100, "test.txt");
  EXPECT_TRUE(side_panel_ui.CanDragEnter(nullptr, drop_data, ops));

  // DropData with URL should be rejected (OS file drops only on Android).
  drop_data.file_system_files.clear();
  drop_data.url_infos.emplace_back(GURL("https://example.com"),
                                   std::u16string());
  EXPECT_FALSE(side_panel_ui.CanDragEnter(nullptr, drop_data, ops));

  // DropData with text should be rejected.
  drop_data.url_infos.clear();
  drop_data.text = u"test text";
  EXPECT_FALSE(side_panel_ui.CanDragEnter(nullptr, drop_data, ops));

  // DropData with html should be rejected.
  drop_data.text.reset();
  drop_data.html = u"<b>test html</b>";
  EXPECT_FALSE(side_panel_ui.CanDragEnter(nullptr, drop_data, ops));
}

TEST_F(GlicSidePanelUiAndroidTest, CanDragEnter_FeatureDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(features::kGlicDragAndDropFileUpload);

  GlicSidePanelUi side_panel_ui(profile(), base::WeakPtr<tabs::TabInterface>(),
                                delegate(), instance_metrics());

  content::DropData drop_data;
  drop_data.filenames.emplace_back(
      base::FilePath(FILE_PATH_LITERAL("test.txt")),
      base::FilePath(FILE_PATH_LITERAL("test.txt")));
  blink::DragOperationsMask ops = blink::kDragOperationCopy;

  // Should be rejected because kGlicDragAndDropFileUpload is disabled.
  EXPECT_FALSE(side_panel_ui.CanDragEnter(nullptr, drop_data, ops));
}

TEST_F(GlicSidePanelUiAndroidTest, CanDragEnter_AndroidKillSwitchDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kGlicDragAndDropFileUpload},
      /*disabled_features=*/{features::kGlicDragAndDropFileUploadAndroid});

  GlicSidePanelUi side_panel_ui(profile(), base::WeakPtr<tabs::TabInterface>(),
                                delegate(), instance_metrics());

  content::DropData drop_data;
  drop_data.filenames.emplace_back(
      base::FilePath(FILE_PATH_LITERAL("test.txt")),
      base::FilePath(FILE_PATH_LITERAL("test.txt")));
  blink::DragOperationsMask ops = blink::kDragOperationCopy;

  // Should be rejected because kGlicDragAndDropFileUploadAndroid is disabled.
  EXPECT_FALSE(side_panel_ui.CanDragEnter(nullptr, drop_data, ops));
}

// Rejecting Chrome's microphone pre-prompt should fail the request without
// ever reaching the OS permission prompt.
TEST_F(GlicSidePanelUiAndroidTest, MicPermissionDialogDenied) {
  GlicSidePanelUi side_panel_ui(profile(), base::WeakPtr<tabs::TabInterface>(),
                                delegate(), instance_metrics());
  side_panel_ui.is_requesting_media_permission_ = true;

  MediaResponseRecorder recorder;
  side_panel_ui.OnMicPermissionDialogResult(
      /*web_contents=*/nullptr, MakeAudioRequest(), recorder.GetCallback(),
      /*allowed=*/false);

  EXPECT_TRUE(recorder.responded());
  EXPECT_EQ(blink::mojom::MediaStreamRequestResult::PERMISSION_DENIED,
            recorder.result());
  EXPECT_FALSE(side_panel_ui.is_requesting_media_permission_);
}

// Accepting the pre-prompt after the WebContents is gone should fail the
// request rather than continuing to the OS prompt.
TEST_F(GlicSidePanelUiAndroidTest,
       MicPermissionDialogAcceptedWithDeadWebContents) {
  GlicSidePanelUi side_panel_ui(profile(), base::WeakPtr<tabs::TabInterface>(),
                                delegate(), instance_metrics());
  side_panel_ui.is_requesting_media_permission_ = true;

  MediaResponseRecorder recorder;
  side_panel_ui.OnMicPermissionDialogResult(
      /*web_contents=*/nullptr, MakeAudioRequest(), recorder.GetCallback(),
      /*allowed=*/true);

  EXPECT_TRUE(recorder.responded());
  EXPECT_EQ(
      blink::mojom::MediaStreamRequestResult::FAILED_DUE_TO_SHUTDOWN_OTHER,
      recorder.result());
  EXPECT_FALSE(side_panel_ui.is_requesting_media_permission_);
}

// A permission prompt takes focus away from the browser window, so the panel
// must not be reported as deactivated while one is showing.
TEST_F(GlicSidePanelUiAndroidTest,
       DeactivationSuppressedDuringPermissionRequest) {
  GlicSidePanelUi side_panel_ui(profile(), tab(), delegate(),
                                instance_metrics());

  // MockTabInterface returns null for GetBrowserWindowInterface(), so null is
  // the browser this tab belongs to for the purposes of this test.
  side_panel_ui.is_requesting_media_permission_ = true;
  side_panel_ui.OnBrowserDeactivated(nullptr);
  EXPECT_TRUE(delegate().activation_changes().empty());

  side_panel_ui.is_requesting_media_permission_ = false;
  side_panel_ui.OnBrowserDeactivated(nullptr);
  EXPECT_THAT(delegate().activation_changes(), testing::ElementsAre(false));
}

}  // namespace glic
