// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/widget/glic_side_panel_ui_android.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/unguessable_token.h"
#include "chrome/browser/glic/android/glic_helper_android.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/glic_profile_manager.h"
#include "chrome/browser/glic/host/host.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/public/glic_keyed_service_factory.h"
#include "chrome/browser/glic/public/glic_side_panel_coordinator.h"
#include "chrome/browser/glic/service/glic_ui_embedder.h"
#include "chrome/browser/glic/service/metrics/glic_instance_metrics.h"
#include "chrome/browser/glic/widget/glic_co_browse_views.h"
#include "chrome/browser/glic/widget/glic_inactive_side_panel_ui_android.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/metrics/profile_metrics_service.h"
#include "components/prefs/pref_service.h"
#include "components/tabs/public/mock_tab_interface.h"
#include "content/public/browser/media_stream_request.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/drop_data.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/mock_permission_manager.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/mediastream/media_stream_request.h"
#include "third_party/blink/public/common/page/drag_operation.h"
#include "third_party/blink/public/mojom/mediastream/media_stream.mojom.h"
#include "ui/android/view_android.h"
#include "ui/android/window_android.h"
#include "ui/base/base_window.h"
#include "ui/base/mojom/window_show_state.mojom.h"
#include "ui/gfx/geometry/rect.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace glic {
namespace {

// Records StopMicrophone() requests instead of forwarding them to a web client.
class FakeHost : public Host {
 public:
  explicit FakeHost(Profile* profile)
      : Host(profile,
             /*sharing_manager_provider=*/nullptr,
             /*glic_instance=*/nullptr,
             /*instance_delegate=*/nullptr) {}

  void StopMicrophone(base::OnceClosure done) override {
    ++stop_microphone_count_;
    std::move(done).Run();
  }

  bool IsShowingNativeLoading() const override {
    return showing_native_loading_;
  }
  void set_showing_native_loading(bool showing) {
    showing_native_loading_ = showing;
  }

  int stop_microphone_count() const { return stop_microphone_count_; }

 private:
  int stop_microphone_count_ = 0;
  bool showing_native_loading_ = false;
};

class FakeGlicSidePanelCoordinator : public GlicSidePanelCoordinator {
 public:
  explicit FakeGlicSidePanelCoordinator(tabs::TabInterface* tab)
      : GlicSidePanelCoordinator(tab) {}
  ~FakeGlicSidePanelCoordinator() override = default;

  // GlicSidePanelCoordinator:
  void Show(const ShowOptions& options) override { state_ = State::kShown; }
  void Close(const CloseOptions& options) override {
    if (co_browse_views_) {
      co_browse_views_->OnPanelClosed();
    }
    state_ = State::kClosed;
  }
  bool IsShowing() const override { return state_ == State::kShown; }
  State state() override { return state_; }
  bool SupportsPeek() const override { return true; }
  base::CallbackListSubscription AddStateCallback(
      base::RepeatingCallback<void(State state)> callback) override {
    return state_callbacks_.Add(std::move(callback));
  }
  void SetCoBrowseViews(
      std::unique_ptr<GlicCoBrowseViews> co_browse_views) override {
    co_browse_views_ = std::move(co_browse_views);
  }
  GlicCoBrowseViews* GetCoBrowseViews() const override {
    return co_browse_views_.get();
  }
  int GetPreferredWidth() override { return 400; }
  bool IsGlicSidePanelActive() override { return state_ != State::kClosed; }

  bool is_loading() const {
    return co_browse_views_ ? co_browse_views_->is_loading_for_testing()
                            : false;
  }
  const content::WebContents* last_web_contents() const {
    return co_browse_views_ ? co_browse_views_->web_contents_for_testing()
                            : nullptr;
  }

 private:
  State state_ = State::kShown;
  std::unique_ptr<GlicCoBrowseViews> co_browse_views_;
  base::RepeatingCallbackList<void(State)> state_callbacks_;
};

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
  void UpdatePanelState() override {}
  void AcceptNativeFre() override { ++accept_count_; }

  int accept_count() const { return accept_count_; }

  const std::vector<bool>& activation_changes() const {
    return activation_changes_;
  }

  // Lets a test ignore activation reported during setup and assert only on the
  // notifications triggered by the code under test.
  void ClearActivationChanges() { activation_changes_.clear(); }

 private:
  raw_ref<Host> host_;
  std::vector<bool> activation_changes_;
  int accept_count_ = 0;
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

class FakeMicPermissionUi : public MicPermissionUi {
 public:
  void ShowMicPermissionDialog(
      ui::WindowAndroid* window_android,
      base::OnceCallback<void(bool)> callback) override {
    ++dialog_count_;
    std::move(callback).Run(dialog_result_);
  }
  bool HasMicOsPermission(ui::WindowAndroid* window_android) override {
    return has_os_permission_;
  }
  void RequestMicOsPermission(
      ui::WindowAndroid* window_android,
      base::OnceCallback<void(bool)> callback) override {
    ++os_prompt_count_;
    std::move(callback).Run(os_prompt_result_);
  }
  void ShowMicDisabledSnackbar(ui::WindowAndroid* window_android) override {
    ++snackbar_count_;
  }

  void set_dialog_result(bool allowed) { dialog_result_ = allowed; }
  void set_has_os_permission(bool granted) { has_os_permission_ = granted; }
  void set_os_prompt_result(bool granted) { os_prompt_result_ = granted; }

  int dialog_count() const { return dialog_count_; }
  int os_prompt_count() const { return os_prompt_count_; }
  int snackbar_count() const { return snackbar_count_; }

 private:
  bool dialog_result_ = true;
  bool has_os_permission_ = true;
  bool os_prompt_result_ = true;
  int dialog_count_ = 0;
  int os_prompt_count_ = 0;
  int snackbar_count_ = 0;
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
      : host_(&profile_),
        delegate_(host_),
        instance_metrics_(&profile_metrics_service_, &profile_) {
    ON_CALL(browser_window_, GetWindow())
        .WillByDefault(testing::Return(&window_));
    ON_CALL(mock_tab_, GetProfile()).WillByDefault(testing::Return(&profile_));
  }
  ~GlicSidePanelUiAndroidTest() override = default;

  TestingProfile* profile() { return &profile_; }

  void SetFreCompleted(bool completed) {
    profile()->GetPrefs()->SetInteger(
        prefs::kGlicCompletedFre,
        static_cast<int>(completed ? prefs::FreStatus::kCompleted
                                   : prefs::FreStatus::kNotStarted));
  }
  FakeHost& host() { return host_; }
  FakeGlicUiEmbedderDelegate& delegate() { return delegate_; }
  GlicInstanceMetrics& instance_metrics() { return instance_metrics_; }
  base::WeakPtr<tabs::TabInterface> tab() {
    return tab_weak_factory_.GetWeakPtr();
  }
  tabs::MockTabInterface& mock_tab() { return mock_tab_; }

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

  std::unique_ptr<content::WebContents> CreateWebContentsWithWindow() {
    if (!window_android_) {
      window_android_ = ui::WindowAndroid::CreateForTesting();
    }
    auto web_contents =
        content::WebContentsTester::CreateTestWebContents(&profile_, nullptr);
    window_android_->get()->AddChild(web_contents->GetNativeView());
    return web_contents;
  }

  FakeBaseWindow& window() { return window_; }
  MockBrowserWindowInterface& browser_window() { return browser_window_; }

 private:
  content::BrowserTaskEnvironment task_environment_;
  content::RenderViewHostTestEnabler rvh_test_enabler_;
  std::unique_ptr<ui::WindowAndroid::ScopedWindowAndroidForTesting>
      window_android_;
  TestingProfile profile_;
  metrics::ProfileMetricsService profile_metrics_service_;
  FakeHost host_;
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
  SetFreCompleted(true);

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

TEST_F(GlicSidePanelUiAndroidTest, CanDragEnter_RejectedBeforeFreCompleted) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures({features::kGlicDragAndDropFileUpload,
                                 features::kGlicDragAndDropFileUploadAndroid},
                                {});

  GlicSidePanelUi side_panel_ui(profile(), base::WeakPtr<tabs::TabInterface>(),
                                delegate(), instance_metrics());

  content::DropData drop_data;
  drop_data.filenames.emplace_back(
      base::FilePath(FILE_PATH_LITERAL("test.txt")),
      base::FilePath(FILE_PATH_LITERAL("test.txt")));
  blink::DragOperationsMask ops = blink::kDragOperationCopy;

  SetFreCompleted(false);
  EXPECT_FALSE(side_panel_ui.CanDragEnter(nullptr, drop_data, ops));

  SetFreCompleted(true);
  EXPECT_TRUE(side_panel_ui.CanDragEnter(nullptr, drop_data, ops));
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

// A Chrome web drag (one carrying a `drag_id`) is accepted only while
// kGlicWebDragAndDropFileUpload is enabled; OS file drags are unaffected.
TEST_F(GlicSidePanelUiAndroidTest, CanDragEnter_WebDrag) {
  SetFreCompleted(true);
  for (bool web_drag_enabled : {true, false}) {
    SCOPED_TRACE(web_drag_enabled);
    base::test::ScopedFeatureList feature_list;
    std::vector<base::test::FeatureRef> enabled_features = {
        features::kGlicDragAndDropFileUpload,
        features::kGlicDragAndDropFileUploadAndroid};
    std::vector<base::test::FeatureRef> disabled_features;
    (web_drag_enabled ? enabled_features : disabled_features)
        .push_back(features::kGlicWebDragAndDropFileUpload);
    feature_list.InitWithFeatures(enabled_features, disabled_features);

    GlicSidePanelUi side_panel_ui(profile(),
                                  base::WeakPtr<tabs::TabInterface>(),
                                  delegate(), instance_metrics());

    content::DropData drop_data;
    drop_data.filenames.emplace_back(
        base::FilePath(FILE_PATH_LITERAL("image.png")),
        base::FilePath(FILE_PATH_LITERAL("image.png")));
    blink::DragOperationsMask ops = blink::kDragOperationCopy;

    // An OS file drag is accepted regardless of the web drag flag.
    EXPECT_TRUE(side_panel_ui.CanDragEnter(nullptr, drop_data, ops));

    // The same payload tagged as a Chrome web drag follows the web drag flag.
    drop_data.drag_id = base::UnguessableToken::Create();
    EXPECT_EQ(side_panel_ui.CanDragEnter(nullptr, drop_data, ops),
              web_drag_enabled);
  }
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

// Leaving Chrome (e.g. for an incoming call) must ask the web client to stop
// listening, since voice input is foreground only.
TEST_F(GlicSidePanelUiAndroidTest, WindowDeactivatedStopsMicrophone) {
  base::test::ScopedFeatureList feature_list{features::kGlicVoice};
  AttachBrowserWindowToTab();
  GlicSidePanelUi side_panel_ui(profile(), tab(), delegate(),
                                instance_metrics());

  side_panel_ui.OnBrowserDeactivated(&browser_window());

  EXPECT_EQ(1, host().stop_microphone_count());
}

// A permission prompt takes focus away from the browser window, so the panel
// must not be reported as deactivated while one is showing.
TEST_F(GlicSidePanelUiAndroidTest,
       WindowDeactivatedDuringPermissionRequestIsSuppressed) {
  base::test::ScopedFeatureList feature_list{features::kGlicVoice};
  AttachBrowserWindowToTab();
  GlicSidePanelUi side_panel_ui(profile(), tab(), delegate(),
                                instance_metrics());
  delegate().ClearActivationChanges();
  side_panel_ui.is_requesting_media_permission_ = true;

  side_panel_ui.OnBrowserDeactivated(&browser_window());

  EXPECT_TRUE(delegate().activation_changes().empty());
  // The prompt is how the user turns the microphone on, so it must not stop it.
  EXPECT_EQ(0, host().stop_microphone_count());
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
  base::test::ScopedFeatureList feature_list{features::kGlicVoice};
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
  // Activation can lag the end of the OS prompt, so the catch-up does not stop
  // the microphone the user just granted. Page visibility covers a real
  // backgrounding.
  EXPECT_EQ(0, host().stop_microphone_count());
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
  base::test::ScopedFeatureList feature_list{features::kGlicVoice};
  AttachBrowserWindowToTab();
  GlicSidePanelUi side_panel_ui(profile(), tab(), delegate(),
                                instance_metrics());
  delegate().ClearActivationChanges();

  testing::NiceMock<MockBrowserWindowInterface> other_browser_window;
  side_panel_ui.OnBrowserDeactivated(&other_browser_window);

  EXPECT_TRUE(delegate().activation_changes().empty());
  EXPECT_EQ(0, host().stop_microphone_count());
}

// Every way the sheet stops being fully shown (closed, peeked, or hidden by a
// tab switch) must stop the microphone.
TEST_F(GlicSidePanelUiAndroidTest, PanelHiddenStopsMicrophone) {
  base::test::ScopedFeatureList feature_list{features::kGlicVoice};
  for (auto state : {GlicSidePanelCoordinator::State::kClosed,
                     GlicSidePanelCoordinator::State::kPeek,
                     GlicSidePanelCoordinator::State::kBackgrounded}) {
    GlicSidePanelUi side_panel_ui(profile(), tab(), delegate(),
                                  instance_metrics());
    int count_before = host().stop_microphone_count();

    side_panel_ui.SidePanelStateChanged(state);

    EXPECT_EQ(count_before + 1, host().stop_microphone_count())
        << "state=" << static_cast<int>(state);
  }
}

TEST_F(GlicSidePanelUiAndroidTest, PanelShownDoesNotStopMicrophone) {
  base::test::ScopedFeatureList feature_list{features::kGlicVoice};
  GlicSidePanelUi side_panel_ui(profile(), tab(), delegate(),
                                instance_metrics());

  side_panel_ui.SidePanelStateChanged(GlicSidePanelCoordinator::State::kShown);

  EXPECT_EQ(0, host().stop_microphone_count());
}

// With GlicVoice disabled, neither leaving Chrome nor hiding the sheet asks the
// web client to stop the microphone.
TEST_F(GlicSidePanelUiAndroidTest, GlicVoiceDisabledDoesNotStopMicrophone) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(features::kGlicVoice);
  AttachBrowserWindowToTab();
  {
    GlicSidePanelUi side_panel_ui(profile(), tab(), delegate(),
                                  instance_metrics());
    side_panel_ui.OnBrowserDeactivated(&browser_window());
    EXPECT_THAT(delegate().activation_changes(), testing::Contains(false));
  }
  for (auto state : {GlicSidePanelCoordinator::State::kClosed,
                     GlicSidePanelCoordinator::State::kPeek,
                     GlicSidePanelCoordinator::State::kBackgrounded}) {
    GlicSidePanelUi side_panel_ui(profile(), tab(), delegate(),
                                  instance_metrics());
    side_panel_ui.SidePanelStateChanged(state);
  }

  EXPECT_EQ(0, host().stop_microphone_count());
}

// If the panel went away while the OS prompt was up, the request fails without
// being forwarded.
TEST_F(GlicSidePanelUiAndroidTest,
       MicOsPermissionResultRejectsWhenWebContentsGone) {
  GlicSidePanelUi side_panel_ui(profile(), base::WeakPtr<tabs::TabInterface>(),
                                delegate(), instance_metrics());
  side_panel_ui.is_requesting_media_permission_ = true;

  MediaResponseRecorder recorder;
  side_panel_ui.OnMicOsPermissionResult(
      /*web_contents=*/nullptr, MakeAudioRequest(), recorder.GetCallback(),
      /*granted=*/true);

  EXPECT_TRUE(recorder.responded());
  EXPECT_EQ(
      blink::mojom::MediaStreamRequestResult::FAILED_DUE_TO_SHUTDOWN_OTHER,
      recorder.result());
  EXPECT_FALSE(side_panel_ui.is_requesting_media_permission_);
}

// When the Glic microphone setting is off and Android has already granted the
// mic, Chrome's dialog is shown and accepting it skips the Android prompt.
//
// The hop through MediaCaptureDevicesDispatcher cannot complete in a unit test,
// so the dispatcher's reply and pref update are tested separately in
// MicPermissionDialogAcceptedAndSystemGrantedUpdatesMicPref.
TEST_F(GlicSidePanelUiAndroidTest,
       MicPermissionDialogAcceptedWhenMicSettingDisabledSkipsOsPrompt) {
  base::test::ScopedFeatureList feature_list{features::kGlicVoice};
  profile()->GetPrefs()->SetBoolean(prefs::kGlicMicrophoneEnabled, false);

  FakeMicPermissionUi mic_permission_ui;
  GlicSidePanelUi side_panel_ui(profile(), base::WeakPtr<tabs::TabInterface>(),
                                delegate(), instance_metrics());
  side_panel_ui.SetMicPermissionUiForTesting(&mic_permission_ui);

  auto web_contents = CreateWebContentsWithWindow();
  MediaResponseRecorder recorder;
  side_panel_ui.RequestMediaAccessPermission(
      web_contents.get(), MakeAudioRequest(), recorder.GetCallback());

  EXPECT_EQ(1, mic_permission_ui.dialog_count());
  EXPECT_EQ(0, mic_permission_ui.os_prompt_count());
}

// Granting microphone access when GlicVoice is enabled and the setting is off
// updates the Glic microphone pref to true and records the user action once.
TEST_F(GlicSidePanelUiAndroidTest,
       MicPermissionDialogAcceptedAndSystemGrantedUpdatesMicPref) {
  base::test::ScopedFeatureList feature_list{features::kGlicVoice};
  profile()->GetPrefs()->SetBoolean(prefs::kGlicMicrophoneEnabled, false);
  base::UserActionTester user_action_tester;

  GlicSidePanelUi side_panel_ui(profile(), base::WeakPtr<tabs::TabInterface>(),
                                delegate(), instance_metrics());
  side_panel_ui.is_requesting_media_permission_ = true;

  MediaResponseRecorder recorder;
  side_panel_ui.OnMediaAccessPermissionResult(
      /*web_contents=*/nullptr,
      blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE,
      recorder.GetCallback(), blink::mojom::StreamDevicesSet(),
      blink::mojom::MediaStreamRequestResult::OK, /*ui=*/nullptr);

  EXPECT_TRUE(recorder.responded());
  EXPECT_EQ(blink::mojom::MediaStreamRequestResult::OK, recorder.result());
  EXPECT_TRUE(profile()->GetPrefs()->GetBoolean(prefs::kGlicMicrophoneEnabled));
  EXPECT_EQ(
      1, user_action_tester.GetActionCount("GlicMicrophonePermissionEnabled"));

  // Subsequent grants when the setting is already enabled do not re-record the
  // action.
  MediaResponseRecorder second_recorder;
  side_panel_ui.OnMediaAccessPermissionResult(
      /*web_contents=*/nullptr,
      blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE,
      second_recorder.GetCallback(), blink::mojom::StreamDevicesSet(),
      blink::mojom::MediaStreamRequestResult::OK, /*ui=*/nullptr);
  EXPECT_EQ(
      1, user_action_tester.GetActionCount("GlicMicrophonePermissionEnabled"));
}

// Declining Chrome's dialog when the Glic microphone setting is off rejects the
// request without prompting the OS or enabling the setting.
TEST_F(GlicSidePanelUiAndroidTest,
       MicSettingDisabledDialogDeclinedRejectsWithoutEnablingMic) {
  base::test::ScopedFeatureList feature_list{features::kGlicVoice};
  profile()->GetPrefs()->SetBoolean(prefs::kGlicMicrophoneEnabled, false);

  FakeMicPermissionUi mic_permission_ui;
  mic_permission_ui.set_dialog_result(false);
  GlicSidePanelUi side_panel_ui(profile(), base::WeakPtr<tabs::TabInterface>(),
                                delegate(), instance_metrics());
  side_panel_ui.SetMicPermissionUiForTesting(&mic_permission_ui);

  auto web_contents = CreateWebContentsWithWindow();
  MediaResponseRecorder recorder;
  side_panel_ui.RequestMediaAccessPermission(
      web_contents.get(), MakeAudioRequest(), recorder.GetCallback());

  EXPECT_EQ(1, mic_permission_ui.dialog_count());
  EXPECT_EQ(0, mic_permission_ui.os_prompt_count());
  EXPECT_TRUE(recorder.responded());
  EXPECT_EQ(blink::mojom::MediaStreamRequestResult::PERMISSION_DENIED,
            recorder.result());
  EXPECT_FALSE(
      profile()->GetPrefs()->GetBoolean(prefs::kGlicMicrophoneEnabled));
}

// When the Glic microphone setting is off and the dialog cannot be shown (e.g.
// no WindowAndroid), the request is rejected rather than forwarded to the
// system check.
TEST_F(GlicSidePanelUiAndroidTest,
       MicSettingDisabledRejectsWhenDialogCannotBeShown) {
  base::test::ScopedFeatureList feature_list{features::kGlicVoice};
  profile()->GetPrefs()->SetBoolean(prefs::kGlicMicrophoneEnabled, false);

  GlicSidePanelUi side_panel_ui(profile(), base::WeakPtr<tabs::TabInterface>(),
                                delegate(), instance_metrics());

  MediaResponseRecorder recorder;
  side_panel_ui.RequestMediaAccessPermission(
      /*web_contents=*/nullptr, MakeAudioRequest(), recorder.GetCallback());

  EXPECT_TRUE(recorder.responded());
  EXPECT_EQ(blink::mojom::MediaStreamRequestResult::PERMISSION_DENIED,
            recorder.result());
  EXPECT_FALSE(
      profile()->GetPrefs()->GetBoolean(prefs::kGlicMicrophoneEnabled));
}

// When both the Glic microphone setting and the Android permission are enabled,
// no dialog or OS prompt is shown.
TEST_F(GlicSidePanelUiAndroidTest,
       MicSettingEnabledWithOsPermissionSkipsDialog) {
  base::test::ScopedFeatureList feature_list{features::kGlicVoice};
  profile()->GetPrefs()->SetBoolean(prefs::kGlicMicrophoneEnabled, true);

  FakeMicPermissionUi mic_permission_ui;
  GlicSidePanelUi side_panel_ui(profile(), base::WeakPtr<tabs::TabInterface>(),
                                delegate(), instance_metrics());
  side_panel_ui.SetMicPermissionUiForTesting(&mic_permission_ui);

  auto web_contents = CreateWebContentsWithWindow();
  MediaResponseRecorder recorder;
  side_panel_ui.RequestMediaAccessPermission(
      web_contents.get(), MakeAudioRequest(), recorder.GetCallback());

  EXPECT_EQ(0, mic_permission_ui.dialog_count());
  EXPECT_EQ(0, mic_permission_ui.os_prompt_count());
}

// Denying the OS permission prompt leaves the Glic microphone pref false.
TEST_F(GlicSidePanelUiAndroidTest,
       MicPermissionDialogAcceptedButSystemDeniedDoesNotUpdateMicPref) {
  base::test::ScopedFeatureList feature_list{features::kGlicVoice};
  profile()->GetPrefs()->SetBoolean(prefs::kGlicMicrophoneEnabled, false);

  GlicSidePanelUi side_panel_ui(profile(), base::WeakPtr<tabs::TabInterface>(),
                                delegate(), instance_metrics());
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
  EXPECT_FALSE(
      profile()->GetPrefs()->GetBoolean(prefs::kGlicMicrophoneEnabled));
}

// CheckMediaAccessPermission returns false for audio capture when the Glic
// microphone setting is off, and passes through when it is on.
TEST_F(GlicSidePanelUiAndroidTest, CheckMediaAccessPermissionChecksMicSetting) {
  base::test::ScopedFeatureList feature_list{features::kGlicVoice};
  auto permission_manager =
      std::make_unique<testing::NiceMock<content::MockPermissionManager>>();
  ON_CALL(*permission_manager, GetPermissionResultForCurrentDocument)
      .WillByDefault(testing::Return(
          content::PermissionResult(blink::mojom::PermissionStatus::GRANTED)));
  profile()->SetPermissionControllerDelegate(std::move(permission_manager));

  auto web_contents =
      content::WebContentsTester::CreateTestWebContents(profile(), nullptr);
  const GURL kUrl("https://example.com");
  content::WebContentsTester::For(web_contents.get())->NavigateAndCommit(kUrl);

  GlicSidePanelUi side_panel_ui(profile(), base::WeakPtr<tabs::TabInterface>(),
                                delegate(), instance_metrics());

  profile()->GetPrefs()->SetBoolean(prefs::kGlicMicrophoneEnabled, false);
  EXPECT_FALSE(side_panel_ui.CheckMediaAccessPermission(
      web_contents->GetPrimaryMainFrame(), url::Origin::Create(kUrl),
      blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE));

  profile()->GetPrefs()->SetBoolean(prefs::kGlicMicrophoneEnabled, true);
  EXPECT_TRUE(side_panel_ui.CheckMediaAccessPermission(
      web_contents->GetPrimaryMainFrame(), url::Origin::Create(kUrl),
      blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE));
}

TEST_F(GlicSidePanelUiAndroidTest,
       ForwardsNativeLoadingStateToSidePanelCoordinator) {
  FakeGlicSidePanelCoordinator coordinator(&mock_tab());
  host().set_showing_native_loading(true);

  {
    GlicSidePanelUi side_panel_ui(profile(), tab(), delegate(),
                                  instance_metrics());
    EXPECT_TRUE(coordinator.is_loading());

    side_panel_ui.NativeLoadingStateChanged(false);
    EXPECT_FALSE(coordinator.is_loading());

    host().set_showing_native_loading(true);
    side_panel_ui.ContentsCreated();
    EXPECT_TRUE(coordinator.is_loading());
  }

  // Destroying `GlicSidePanelUi` clears loading state on the coordinator.
  EXPECT_FALSE(coordinator.is_loading());

  // Transitioning from active to inactive side panel UI clears loading state
  // and detaches `WebContents` on the shared `GlicCoBrowseViews`.
  auto active_ui = std::make_unique<GlicSidePanelUi>(
      profile(), tab(), delegate(), instance_metrics());
  ASSERT_TRUE(coordinator.is_loading());
  auto inactive_ui =
      GlicInactiveSidePanelUi::CreateForVisibleTab(tab(), delegate());
  active_ui.reset();
  EXPECT_FALSE(coordinator.is_loading());
  EXPECT_EQ(nullptr, coordinator.last_web_contents());
}

TEST_F(GlicSidePanelUiAndroidTest, RoutesNativeFreAccept) {
  AttachBrowserWindowToTab();
  FakeGlicSidePanelCoordinator coordinator(tab().get());

  auto side_panel_ui = std::make_unique<GlicSidePanelUi>(
      profile(), tab(), delegate(), instance_metrics());

  ASSERT_TRUE(coordinator.GetCoBrowseViews());
  coordinator.GetCoBrowseViews()->OnNativeFreAccepted();
  EXPECT_EQ(delegate().accept_count(), 1);

  side_panel_ui.reset();
  if (coordinator.GetCoBrowseViews()) {
    coordinator.GetCoBrowseViews()->OnNativeFreAccepted();
  }
  EXPECT_EQ(delegate().accept_count(), 1);
}

TEST_F(GlicSidePanelUiAndroidTest,
       CoBrowseViewsPreservedAcrossActiveInactiveTransitions) {
  AttachBrowserWindowToTab();
  FakeGlicSidePanelCoordinator coordinator(tab().get());

  auto side_panel_ui = std::make_unique<GlicSidePanelUi>(
      profile(), tab(), delegate(), instance_metrics());
  GlicCoBrowseViews* initial_views = coordinator.GetCoBrowseViews();
  ASSERT_TRUE(initial_views);

  // Transition to inactive embedder:
  auto inactive_ui = side_panel_ui->CreateInactiveEmbedder();
  EXPECT_EQ(coordinator.GetCoBrowseViews(), initial_views);

  // Destroy active embedder; the coordinator keeps the views alive and the
  // inactive embedder's close callback remains bound.
  side_panel_ui.reset();
  EXPECT_EQ(coordinator.GetCoBrowseViews(), initial_views);

  // Requesting close from the view routes to inactive embedder's Close().
  initial_views->RequestCloseForTesting();
  EXPECT_EQ(coordinator.state(), GlicSidePanelCoordinator::State::kClosed);
}

TEST_F(GlicSidePanelUiAndroidTest, NativeFreLatchSurvivesActiveInactiveReshow) {
  AttachBrowserWindowToTab();
  FakeGlicSidePanelCoordinator coordinator(tab().get());

  SetFreCompleted(false);

  auto side_panel_ui = std::make_unique<GlicSidePanelUi>(
      profile(), tab(), delegate(), instance_metrics());
  ASSERT_TRUE(coordinator.GetCoBrowseViews());

  auto show_options = ShowOptions::ForSidePanel(*tab());
  show_options.fre_override = mojom::FreOverride::kNativeFre;
  side_panel_ui->Show(show_options);

  EXPECT_TRUE(coordinator.GetCoBrowseViews()->IsShowingNativeFreForTesting());

  // Transition via CreateInactiveEmbedder(), destroy the first
  // GlicSidePanelUi, construct a new GlicSidePanelUi, and call Show with
  // kUnspecified fre_override.
  auto inactive_ui = side_panel_ui->CreateInactiveEmbedder();
  side_panel_ui.reset();

  auto new_side_panel_ui = std::make_unique<GlicSidePanelUi>(
      profile(), tab(), delegate(), instance_metrics());
  inactive_ui.reset();

  new_side_panel_ui->Show(ShowOptions::ForSidePanel(*tab()));
  EXPECT_TRUE(coordinator.GetCoBrowseViews()->IsShowingNativeFreForTesting());

  // Completing FRE and showing again clears the native FRE latch.
  SetFreCompleted(true);
  new_side_panel_ui->Show(ShowOptions::ForSidePanel(*tab()));
  EXPECT_FALSE(coordinator.GetCoBrowseViews()->IsShowingNativeFreForTesting());

  // Consenting cleared the latch rather than only masking it.
  SetFreCompleted(false);
  EXPECT_FALSE(coordinator.GetCoBrowseViews()->IsShowingNativeFreForTesting());
}

class GlicSidePanelUiAndroidConsentTest : public GlicSidePanelUiAndroidTest {
 public:
  void SetUp() override {
    GlicSidePanelUiAndroidTest::SetUp();
    ASSERT_TRUE(profile_manager_.SetUp());

    TestingProfile::TestingFactories factories =
        IdentityTestEnvironmentProfileAdaptor::
            GetIdentityTestEnvironmentFactories();
    factories.push_back(
        {GlicKeyedServiceFactory::GetInstance(),
         base::BindRepeating(
             [](ProfileManager* pm, GlicProfileManager* gpm,
                content::BrowserContext* context)
                 -> std::unique_ptr<KeyedService> {
               Profile* p = Profile::FromBrowserContext(context);
               return std::make_unique<GlicKeyedService>(
                   p, IdentityManagerFactory::GetForProfile(p), pm, gpm,
                   /*contextual_cueing_service=*/nullptr,
                   /*actor_keyed_service=*/nullptr);
             },
             profile_manager_.profile_manager(), &glic_profile_manager_)});

    keyed_profile_ =
        profile_manager_.CreateTestingProfile("user", std::move(factories));
    identity_adaptor_ =
        std::make_unique<IdentityTestEnvironmentProfileAdaptor>(keyed_profile_);
    ASSERT_TRUE(GlicKeyedServiceFactory::GetGlicKeyedService(keyed_profile_,
                                                             /*create=*/true));

    keyed_host_ = std::make_unique<FakeHost>(keyed_profile_);
    keyed_delegate_ =
        std::make_unique<FakeGlicUiEmbedderDelegate>(*keyed_host_);
    keyed_metrics_ = std::make_unique<GlicInstanceMetrics>(
        &keyed_profile_metrics_service_, keyed_profile_);

    ON_CALL(keyed_tab_, GetProfile())
        .WillByDefault(testing::Return(keyed_profile_));
    ON_CALL(keyed_tab_, GetBrowserWindowInterface())
        .WillByDefault(testing::Return(&browser_window()));
  }

  void TearDown() override {
    keyed_metrics_.reset();
    keyed_delegate_.reset();
    keyed_host_.reset();
    identity_adaptor_.reset();
    keyed_profile_ = nullptr;
    GlicSidePanelUiAndroidTest::TearDown();
  }

  TestingProfile* keyed_profile() { return keyed_profile_; }
  GlicEnabling& enabling() {
    return GlicKeyedService::Get(keyed_profile_)->enabling();
  }
  FakeHost& keyed_host() { return *keyed_host_; }
  FakeGlicUiEmbedderDelegate& keyed_delegate() { return *keyed_delegate_; }
  GlicInstanceMetrics& keyed_metrics() { return *keyed_metrics_; }
  tabs::MockTabInterface& keyed_tab() { return keyed_tab_; }
  base::WeakPtr<tabs::TabInterface> keyed_tab_weak_ptr() {
    return keyed_tab_weak_factory_.GetWeakPtr();
  }

 private:
  GlicEnabling::ScopedBypassEnablementChecksForTesting bypass_enablement_;
  GlicProfileManager glic_profile_manager_;
  TestingProfileManager profile_manager_{TestingBrowserProcess::GetGlobal()};
  raw_ptr<TestingProfile> keyed_profile_ = nullptr;
  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor> identity_adaptor_;
  metrics::ProfileMetricsService keyed_profile_metrics_service_;
  std::unique_ptr<FakeHost> keyed_host_;
  std::unique_ptr<FakeGlicUiEmbedderDelegate> keyed_delegate_;
  std::unique_ptr<GlicInstanceMetrics> keyed_metrics_;
  tabs::MockTabInterface keyed_tab_;
  base::WeakPtrFactory<tabs::TabInterface> keyed_tab_weak_factory_{&keyed_tab_};
};

TEST_F(GlicSidePanelUiAndroidConsentTest,
       ConsentInAnotherWindowClearsNativeFreAndRevealsLoadingPlaceholder) {
  keyed_host().set_showing_native_loading(true);
  FakeGlicSidePanelCoordinator coordinator(&keyed_tab());
  enabling().SetCompletedFre(prefs::FreStatus::kNotStarted);

  auto side_panel_ui = std::make_unique<GlicSidePanelUi>(
      keyed_profile(), keyed_tab_weak_ptr(), keyed_delegate(), keyed_metrics());
  GlicCoBrowseViews* views = coordinator.GetCoBrowseViews();
  ASSERT_TRUE(views);

  auto show_options = ShowOptions::ForSidePanel(keyed_tab());
  show_options.fre_override = mojom::FreOverride::kNativeFre;
  side_panel_ui->Show(show_options);

  EXPECT_TRUE(views->IsShowingNativeFreForTesting());
  EXPECT_FALSE(views->ShouldShowLoadingPlaceholderForTesting());

  // Completing FRE on the profile (e.g. from another window) immediately clears
  // the native FRE latch and reveals the loading placeholder without a re-show.
  enabling().SetCompletedFre(prefs::FreStatus::kCompleted);

  EXPECT_FALSE(views->IsShowingNativeFreForTesting());
  EXPECT_TRUE(views->ShouldShowLoadingPlaceholderForTesting());

  // Reverting the pref confirms the latch was cleared rather than masked.
  enabling().SetCompletedFre(prefs::FreStatus::kNotStarted);
  EXPECT_FALSE(views->IsShowingNativeFreForTesting());
}
}  // namespace glic
