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
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
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
#include "ui/base/base_window.h"
#include "ui/base/mojom/window_show_state.mojom.h"
#include "ui/gfx/geometry/rect.h"
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

  // Lets a test ignore activation reported during setup and assert only on the
  // notifications triggered by the code under test.
  void ClearActivationChanges() { activation_changes_.clear(); }

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

// Minimal ui::BaseWindow stub. Only activation is meaningful here; everything
// else is inert.
class FakeBaseWindow : public ui::BaseWindow {
 public:
  void set_active(bool active) { active_ = active; }

  bool IsActive() const override { return active_; }
  bool IsMaximized() const override { return false; }
  bool IsMinimized() const override { return false; }
  bool IsFullscreen() const override { return false; }
  gfx::NativeWindow GetNativeWindow() const override { return {}; }
  gfx::Rect GetRestoredBounds() const override { return gfx::Rect(); }
  ui::mojom::WindowShowState GetRestoredState() const override {
    return ui::mojom::WindowShowState::kNormal;
  }
  gfx::Rect GetBounds() const override { return gfx::Rect(); }
  void Show() override {}
  void Hide() override {}
  bool IsVisible() const override { return true; }
  void ShowInactive() override {}
  void Close() override {}
  void Activate() override { active_ = true; }
  void Deactivate() override { active_ = false; }
  bool CanResize(ui::WindowResizePrecheckResult& result) const override {
    result = ui::WindowResizePrecheckResult::kOk;
    return true;
  }
  void Maximize() override {}
  void Minimize() override {}
  void Restore() override {}
  void SetBounds(const gfx::Rect& bounds) override {}
  void FlashFrame(bool flash) override {}
  ui::ZOrderLevel GetZOrderLevel() const override {
    return ui::ZOrderLevel::kNormal;
  }
  void SetZOrderLevel(ui::ZOrderLevel order) override {}

 private:
  bool active_ = true;
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
        instance_metrics_(&profile_metrics_service_, &profile_) {
    ON_CALL(browser_window_, GetWindow())
        .WillByDefault(testing::Return(&window_));
  }
  ~GlicSidePanelUiAndroidTest() override = default;

  TestingProfile* profile() { return &profile_; }
  FakeGlicUiEmbedderDelegate& delegate() { return delegate_; }
  GlicInstanceMetrics& instance_metrics() { return instance_metrics_; }
  base::WeakPtr<tabs::TabInterface> tab() {
    return tab_weak_factory_.GetWeakPtr();
  }

 protected:
  // Makes the tab report `browser_window_` as the window it belongs to. Needed
  // by any test that exercises embedder window activation, since
  // GlicSidePanelUi reads activation off the browser window. MockTabInterface
  // reports no browser window by default, which makes the activation paths
  // unreachable.
  void AttachBrowserWindowToTab() {
    ON_CALL(mock_tab_, GetBrowserWindowInterface())
        .WillByDefault(testing::Return(&browser_window_));
  }

  FakeBaseWindow& window() { return window_; }
  MockBrowserWindowInterface& browser_window() { return browser_window_; }

 private:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  metrics::ProfileMetricsService profile_metrics_service_;
  Host host_;
  FakeGlicUiEmbedderDelegate delegate_;
  GlicInstanceMetrics instance_metrics_;
  FakeBaseWindow window_;
  testing::NiceMock<MockBrowserWindowInterface> browser_window_;
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

// The user accepted Chrome's pre-prompt and then granted the OS permission:
// the request succeeds and the panel is not reported as backgrounded.
//
// The hop through MediaCaptureDevicesDispatcher (and thus the real OS prompt)
// cannot run in a unit test, so the dispatcher's reply is injected directly.
TEST_F(GlicSidePanelUiAndroidTest,
       MicPermissionDialogAcceptedAndSystemGranted) {
  AttachBrowserWindowToTab();
  window().set_active(true);
  GlicSidePanelUi side_panel_ui(profile(), tab(), delegate(),
                                instance_metrics());
  delegate().ClearActivationChanges();
  side_panel_ui.is_requesting_media_permission_ = true;

  MediaResponseRecorder recorder;
  side_panel_ui.OnMediaAccessPermissionResult(
      /*web_contents=*/nullptr,
      blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE,
      recorder.GetCallback(), blink::mojom::StreamDevicesSet(),
      blink::mojom::MediaStreamRequestResult::OK, /*ui=*/nullptr);

  EXPECT_TRUE(recorder.responded());
  EXPECT_EQ(blink::mojom::MediaStreamRequestResult::OK, recorder.result());
  EXPECT_FALSE(side_panel_ui.is_requesting_media_permission_);
  // The window is active again, so no deactivation should be reported.
  EXPECT_TRUE(delegate().activation_changes().empty());
}

// The user accepted Chrome's pre-prompt but denied the OS prompt: the denial is
// forwarded to the caller and the request is no longer in flight.
TEST_F(GlicSidePanelUiAndroidTest, MicPermissionDialogAcceptedButSystemDenied) {
  AttachBrowserWindowToTab();
  window().set_active(true);
  GlicSidePanelUi side_panel_ui(profile(), tab(), delegate(),
                                instance_metrics());
  delegate().ClearActivationChanges();
  side_panel_ui.is_requesting_media_permission_ = true;

  MediaResponseRecorder recorder;
  side_panel_ui.OnMediaAccessPermissionResult(
      /*web_contents=*/nullptr,
      blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE,
      recorder.GetCallback(), blink::mojom::StreamDevicesSet(),
      blink::mojom::MediaStreamRequestResult::PERMISSION_DENIED,
      /*ui=*/nullptr);

  EXPECT_TRUE(recorder.responded());
  EXPECT_EQ(blink::mojom::MediaStreamRequestResult::PERMISSION_DENIED,
            recorder.result());
  EXPECT_FALSE(side_panel_ui.is_requesting_media_permission_);
  EXPECT_TRUE(delegate().activation_changes().empty());
  // NOTE: the "microphone disabled" snackbar is skipped here because the
  // WebContents is gone; showing it needs a live WindowAndroid and is only
  // reachable from a browser test.
}

// Losing window focus with no permission prompt in flight is a genuine
// backgrounding and must reach the delegate.
TEST_F(GlicSidePanelUiAndroidTest,
       WindowDeactivatedWithoutPermissionRequestNotifiesDelegate) {
  AttachBrowserWindowToTab();
  GlicSidePanelUi side_panel_ui(profile(), tab(), delegate(),
                                instance_metrics());
  delegate().ClearActivationChanges();

  side_panel_ui.OnBrowserDeactivated(&browser_window());

  EXPECT_THAT(delegate().activation_changes(), testing::ElementsAre(false));
}

// A permission prompt takes focus away from the browser window, so the panel
// must not be reported as deactivated while one is showing.
TEST_F(GlicSidePanelUiAndroidTest,
       WindowDeactivatedDuringPermissionRequestIsSuppressed) {
  AttachBrowserWindowToTab();
  GlicSidePanelUi side_panel_ui(profile(), tab(), delegate(),
                                instance_metrics());
  delegate().ClearActivationChanges();
  side_panel_ui.is_requesting_media_permission_ = true;

  side_panel_ui.OnBrowserDeactivated(&browser_window());

  EXPECT_TRUE(delegate().activation_changes().empty());
}

// Deactivation suppressed by the prompt must be delivered once the user
// dismisses Chrome's dialog, if the window really did stay inactive.
TEST_F(GlicSidePanelUiAndroidTest,
       SuppressedDeactivationDeliveredAfterDialogDismissed) {
  AttachBrowserWindowToTab();
  GlicSidePanelUi side_panel_ui(profile(), tab(), delegate(),
                                instance_metrics());
  delegate().ClearActivationChanges();
  side_panel_ui.is_requesting_media_permission_ = true;
  side_panel_ui.OnBrowserDeactivated(&browser_window());
  ASSERT_TRUE(delegate().activation_changes().empty());

  window().set_active(false);
  MediaResponseRecorder recorder;
  side_panel_ui.OnMicPermissionDialogResult(
      /*web_contents=*/nullptr, MakeAudioRequest(), recorder.GetCallback(),
      /*allowed=*/false);

  EXPECT_EQ(blink::mojom::MediaStreamRequestResult::PERMISSION_DENIED,
            recorder.result());
  EXPECT_FALSE(side_panel_ui.is_requesting_media_permission_);
  EXPECT_THAT(delegate().activation_changes(), testing::ElementsAre(false));
}

// Same catch-up, but on the path where the OS prompt ran and the window never
// came back to the foreground.
TEST_F(GlicSidePanelUiAndroidTest,
       SuppressedDeactivationDeliveredAfterSystemPromptCompletes) {
  AttachBrowserWindowToTab();
  GlicSidePanelUi side_panel_ui(profile(), tab(), delegate(),
                                instance_metrics());
  delegate().ClearActivationChanges();
  side_panel_ui.is_requesting_media_permission_ = true;
  side_panel_ui.OnBrowserDeactivated(&browser_window());
  ASSERT_TRUE(delegate().activation_changes().empty());

  window().set_active(false);
  MediaResponseRecorder recorder;
  side_panel_ui.OnMediaAccessPermissionResult(
      /*web_contents=*/nullptr,
      blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE,
      recorder.GetCallback(), blink::mojom::StreamDevicesSet(),
      blink::mojom::MediaStreamRequestResult::OK, /*ui=*/nullptr);

  EXPECT_THAT(delegate().activation_changes(), testing::ElementsAre(false));
}

// If the window is active again once the prompt is gone, the suppressed
// deactivation was spurious and must not be replayed.
TEST_F(GlicSidePanelUiAndroidTest,
       NoDeactivationWhenWindowRegainsFocusAfterPrompt) {
  AttachBrowserWindowToTab();
  GlicSidePanelUi side_panel_ui(profile(), tab(), delegate(),
                                instance_metrics());
  delegate().ClearActivationChanges();
  side_panel_ui.is_requesting_media_permission_ = true;
  side_panel_ui.OnBrowserDeactivated(&browser_window());
  ASSERT_TRUE(delegate().activation_changes().empty());

  window().set_active(true);
  MediaResponseRecorder recorder;
  side_panel_ui.OnMediaAccessPermissionResult(
      /*web_contents=*/nullptr,
      blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE,
      recorder.GetCallback(), blink::mojom::StreamDevicesSet(),
      blink::mojom::MediaStreamRequestResult::OK, /*ui=*/nullptr);

  EXPECT_TRUE(delegate().activation_changes().empty());
}

// Another window losing focus says nothing about the window hosting the panel.
TEST_F(GlicSidePanelUiAndroidTest,
       DeactivationOfUnrelatedBrowserWindowIgnored) {
  AttachBrowserWindowToTab();
  GlicSidePanelUi side_panel_ui(profile(), tab(), delegate(),
                                instance_metrics());
  delegate().ClearActivationChanges();

  testing::NiceMock<MockBrowserWindowInterface> other_browser_window;
  side_panel_ui.OnBrowserDeactivated(&other_browser_window);

  EXPECT_TRUE(delegate().activation_changes().empty());
}

}  // namespace glic
