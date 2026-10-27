// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/widget/glic_view.h"

#include <memory>

#include "base/i18n/rtl.h"
#include "base/numerics/safe_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/icu_test_util.h"
#include "base/test/scoped_command_line.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/browser/glic/widget/glic_loading_placeholder_view.h"
#include "chrome/browser/glic/widget/glic_widget.h"
#include "chrome/browser/pwc/privileged_web_contents.h"
#include "chrome/browser/pwc/pwc_component_policy.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "components/viz/common/frame_sinks/copy_output_request.h"
#include "components/viz/common/frame_sinks/copy_output_result.h"
#include "content/public/browser/file_select_listener.h"
#include "content/public/browser/media_stream_request.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/common/drop_data.h"
#include "content/public/test/test_renderer_host.h"
#include "third_party/blink/public/common/page/drag_operation.h"
#include "third_party/blink/public/mojom/choosers/file_chooser.mojom.h"
#include "third_party/blink/public/mojom/mediastream/media_stream.mojom.h"
#include "third_party/blink/public/mojom/page/draggable_region.mojom.h"
#include "third_party/blink/public/mojom/window_features/window_features.mojom.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/window_open_disposition.h"
#include "ui/color/color_provider.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/compositor_switches.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/test/draw_waiter_for_test.h"
#include "ui/gfx/animation/animation_test_api.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/controls/native/native_view_host.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/test/button_test_api.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace glic {

class GlicViewTest : public ChromeViewsTestBase {
 public:
  GlicViewTest() = default;
  ~GlicViewTest() override = default;

  TestingProfile* profile() { return &profile_; }

 private:
  TestingProfile profile_;
};

class GlicViewPixelTest : public GlicViewTest {
 public:
  void SetUp() override {
    scoped_command_line_.GetProcessCommandLine()->AppendSwitch(
        switches::kEnablePixelOutputInTests);
    GlicViewTest::SetUp();
  }

 private:
  base::test::ScopedCommandLine scoped_command_line_;
};

TEST_F(GlicViewTest, CanDragEnter) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures({features::kGlicDragAndDropFileUpload,
                                 features::kGlicWebDragAndDropFileUpload},
                                {});

  auto glic_view =
      std::make_unique<GlicView>(profile(), gfx::Size(800, 600), nullptr);

  content::DropData drop_data;
  blink::DragOperationsMask ops = blink::kDragOperationCopy;

  // Empty DropData should be rejected.
  EXPECT_FALSE(glic_view->CanDragEnter(nullptr, drop_data, ops));

  // DropData with files should be accepted.
  drop_data.filenames.emplace_back(
      base::FilePath(FILE_PATH_LITERAL("test.txt")),
      base::FilePath(FILE_PATH_LITERAL("test.txt")));
  EXPECT_TRUE(glic_view->CanDragEnter(nullptr, drop_data, ops));

  // DropData with file_contents (e.g., web image drag) should be accepted.
  drop_data.filenames.clear();
  drop_data.file_contents = {1, 2, 3, 4};
  EXPECT_TRUE(glic_view->CanDragEnter(nullptr, drop_data, ops));

  // DropData with URL alone (e.g., hyperlink without image bytes) should be
  // rejected.
  drop_data.file_contents.clear();
  drop_data.url_infos.emplace_back(GURL("https://example.com"),
                                   std::u16string());
  EXPECT_FALSE(glic_view->CanDragEnter(nullptr, drop_data, ops));

  // DropData with text should be rejected.
  drop_data.url_infos.clear();
  drop_data.text = u"test text";
  EXPECT_FALSE(glic_view->CanDragEnter(nullptr, drop_data, ops));

  // DropData with html should be rejected.
  drop_data.text.reset();
  drop_data.html = u"<b>test html</b>";
  EXPECT_FALSE(glic_view->CanDragEnter(nullptr, drop_data, ops));
}

TEST_F(GlicViewTest, CanDragEnter_WebDragDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kGlicDragAndDropFileUpload},
      /*disabled_features=*/{features::kGlicWebDragAndDropFileUpload});

  auto glic_view =
      std::make_unique<GlicView>(profile(), gfx::Size(800, 600), nullptr);

  content::DropData drop_data;
  drop_data.file_contents = {1, 2, 3, 4};
  blink::DragOperationsMask ops = blink::kDragOperationCopy;

  // Should be rejected because kGlicWebDragAndDropFileUpload is disabled.
  EXPECT_FALSE(glic_view->CanDragEnter(nullptr, drop_data, ops));
}

TEST_F(GlicViewTest, CanDragEnter_Disabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(features::kGlicDragAndDropFileUpload);

  auto glic_view =
      std::make_unique<GlicView>(profile(), gfx::Size(800, 600), nullptr);

  content::DropData drop_data;
  drop_data.filenames.emplace_back(
      base::FilePath(FILE_PATH_LITERAL("test.txt")),
      base::FilePath(FILE_PATH_LITERAL("test.txt")));
  blink::DragOperationsMask ops = blink::kDragOperationCopy;

  // Should be rejected because the feature is disabled.
  EXPECT_FALSE(glic_view->CanDragEnter(nullptr, drop_data, ops));
}

TEST_F(GlicViewTest, UpdatesBackgroundColorOnThemeChange) {
  auto widget = std::make_unique<views::Widget>();
  views::Widget::InitParams params =
      CreateParams(views::Widget::InitParams::TYPE_POPUP);
  params.ownership = views::Widget::InitParams::CLIENT_OWNS_WIDGET;
  widget->Init(std::move(params));

  auto* glic_view = widget->SetContentsView(
      std::make_unique<GlicView>(profile(), gfx::Size(800, 600), nullptr));

  // Trigger OnThemeChanged manually to simulate the view being notified.
  glic_view->OnThemeChanged();

  // Verify that a background has been set on the view.
  EXPECT_NE(glic_view->GetBackground(), nullptr);
}

class TestWebContentsDelegate : public content::WebContentsDelegate {
 public:
  TestWebContentsDelegate() = default;
  ~TestWebContentsDelegate() override = default;
};

TEST_F(GlicViewTest, SetWebContents_ClearsOldDelegate) {
  content::RenderViewHostTestEnabler rvh_test_enabler;

  auto glic_view =
      std::make_unique<GlicView>(profile(), gfx::Size(800, 600), nullptr);

  auto web_contents = content::WebContents::Create(
      content::WebContents::CreateParams(profile()));
  auto* wc_ptr = web_contents.get();

  EXPECT_EQ(wc_ptr->GetDelegate(), nullptr);

  glic_view->SetWebContents(wc_ptr);
  EXPECT_EQ(wc_ptr->GetDelegate(), glic_view.get());

  glic_view->SetWebContents(nullptr);
  EXPECT_EQ(wc_ptr->GetDelegate(), nullptr);
}

TEST_F(GlicViewTest, SetWebContents_DoesNotClearIfOverwritten) {
  content::RenderViewHostTestEnabler rvh_test_enabler;

  auto glic_view =
      std::make_unique<GlicView>(profile(), gfx::Size(800, 600), nullptr);

  auto web_contents = content::WebContents::Create(
      content::WebContents::CreateParams(profile()));
  auto* wc_ptr = web_contents.get();

  glic_view->SetWebContents(wc_ptr);
  EXPECT_EQ(wc_ptr->GetDelegate(), glic_view.get());

  TestWebContentsDelegate other_delegate;
  wc_ptr->SetDelegate(&other_delegate);
  EXPECT_EQ(wc_ptr->GetDelegate(), &other_delegate);

  glic_view->SetWebContents(nullptr);
  EXPECT_EQ(wc_ptr->GetDelegate(), &other_delegate);
}

class TestEmbedderDelegate
    : public pwc::PrivilegedWebContents::EmbedderDelegate {
 public:
  TestEmbedderDelegate() = default;
  ~TestEmbedderDelegate() override = default;
};

class GlicViewNoWebviewTest : public ChromeViewsTestBase {
 public:
  GlicViewNoWebviewTest() {
    feature_list_.InitWithFeatures(
        {features::kGlicNoWebview, features::kGlicDragAndDropFileUpload}, {});
  }
  ~GlicViewNoWebviewTest() override = default;

  void SetUp() override {
    ChromeViewsTestBase::SetUp();
    glic_view_ =
        std::make_unique<GlicView>(profile(), gfx::Size(800, 600), nullptr);
    pwc_ = CreatePwc();
    glic_view_->SetWebContents(pwc_->web_contents());
  }

  void TearDown() override {
    glic_view_.reset();
    pwc_.reset();
    ChromeViewsTestBase::TearDown();
  }

  std::unique_ptr<pwc::PrivilegedWebContents> CreatePwc() {
    auto policy_delegate = std::make_unique<pwc::FixedPwcPolicyDelegate>(
        std::vector<url::Origin>{test_origin()},
        std::vector<url::Origin>{test_origin()});
    return pwc::PrivilegedWebContents::Create(
        pwc::PrivilegedComponent::kGlic, profile(), std::move(policy_delegate));
  }

  TestingProfile* profile() { return &profile_; }
  url::Origin test_origin() const {
    return url::Origin::Create(GURL("https://pwc-test.example.com"));
  }
  GlicView* glic_view() { return glic_view_.get(); }
  pwc::PrivilegedWebContents* pwc() { return pwc_.get(); }

 protected:
  base::test::ScopedFeatureList feature_list_;
  content::RenderViewHostTestEnabler rvh_test_enabler_;
  TestingProfile profile_;
  std::unique_ptr<pwc::PrivilegedWebContents> pwc_;
  std::unique_ptr<GlicView> glic_view_;
};

TEST_F(GlicViewNoWebviewTest, SetWebContents_ClearsOldDelegate) {
  auto fresh_pwc = CreatePwc();
  EXPECT_EQ(fresh_pwc->embedder_delegate(), nullptr);

  auto view =
      std::make_unique<GlicView>(profile(), gfx::Size(800, 600), nullptr);
  view->SetWebContents(fresh_pwc->web_contents());
  EXPECT_EQ(fresh_pwc->embedder_delegate(), view.get());

  view->SetWebContents(nullptr);
  EXPECT_EQ(fresh_pwc->embedder_delegate(), nullptr);
}

TEST_F(GlicViewNoWebviewTest, SetWebContents_DoesNotClearIfOverwritten) {
  TestEmbedderDelegate other_delegate;
  EXPECT_EQ(pwc()->embedder_delegate(), glic_view());

  pwc()->SetEmbedderDelegate(&other_delegate);
  EXPECT_EQ(pwc()->embedder_delegate(), &other_delegate);

  glic_view()->SetWebContents(nullptr);
  EXPECT_EQ(pwc()->embedder_delegate(), &other_delegate);

  pwc()->SetEmbedderDelegate(nullptr);
}

TEST_F(GlicViewTest, CheckMediaAccessPermission_RoutesToDispatcher) {
  content::RenderViewHostTestEnabler rvh_test_enabler;
  auto glic_view =
      std::make_unique<GlicView>(profile(), gfx::Size(800, 600), nullptr);

  auto web_contents = content::WebContents::Create(
      content::WebContents::CreateParams(profile()));
  glic_view->SetWebContents(web_contents.get());

  EXPECT_FALSE(web_contents->GetDelegate()->CheckMediaAccessPermission(
      web_contents->GetPrimaryMainFrame(),
      url::Origin::Create(GURL("https://example.com")),
      blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE));
}

TEST_F(GlicViewNoWebviewTest, CheckMediaAccessPermission_ForwardsFromPwc) {
  EXPECT_FALSE(pwc()->web_contents()->GetDelegate()->CheckMediaAccessPermission(
      pwc()->web_contents()->GetPrimaryMainFrame(), test_origin(),
      blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE));
}

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

TEST_F(GlicViewTest, RequestMediaAccessPermission_RoutesToDispatcher) {
  content::RenderViewHostTestEnabler rvh_test_enabler;
  auto glic_view =
      std::make_unique<GlicView>(profile(), gfx::Size(800, 600), nullptr);

  auto web_contents = content::WebContents::Create(
      content::WebContents::CreateParams(profile()));
  glic_view->SetWebContents(web_contents.get());

  content::MediaStreamRequest request(
      /*render_process_id=*/web_contents->GetPrimaryMainFrame()
          ->GetProcess()
          ->GetDeprecatedID(),
      /*render_frame_id=*/web_contents->GetPrimaryMainFrame()->GetRoutingID(),
      /*page_request_id=*/0,
      /*security_origin=*/url::Origin::Create(GURL("https://example.com")),
      /*user_gesture=*/false,
      /*request_type=*/blink::MEDIA_DEVICE_ACCESS,
      /*requested_audio_device_ids=*/{},
      /*requested_video_device_ids=*/{},
      /*audio_type=*/blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE,
      /*video_type=*/blink::mojom::MediaStreamType::NO_SERVICE,
      /*disable_local_echo=*/false,
      /*request_pan_tilt_zoom_permission=*/false,
      /*captured_surface_control_active=*/false);

  base::test::TestFuture<blink::mojom::MediaStreamRequestResult> future;
  web_contents->GetDelegate()->RequestMediaAccessPermission(
      web_contents.get(), request, BindResultToFuture(future));
  EXPECT_EQ(future.Get(),
            blink::mojom::MediaStreamRequestResult::INVALID_SECURITY_ORIGIN);
}

TEST_F(GlicViewNoWebviewTest, RequestMediaAccessPermission_ForwardsFromPwc) {
  content::MediaStreamRequest request(
      /*render_process_id=*/pwc()
          ->web_contents()
          ->GetPrimaryMainFrame()
          ->GetProcess()
          ->GetDeprecatedID(),
      /*render_frame_id=*/
      pwc()->web_contents()->GetPrimaryMainFrame()->GetRoutingID(),
      /*page_request_id=*/0,
      /*security_origin=*/test_origin(),
      /*user_gesture=*/false,
      /*request_type=*/blink::MEDIA_DEVICE_ACCESS,
      /*requested_audio_device_ids=*/{},
      /*requested_video_device_ids=*/{},
      /*audio_type=*/blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE,
      /*video_type=*/blink::mojom::MediaStreamType::NO_SERVICE,
      /*disable_local_echo=*/false,
      /*request_pan_tilt_zoom_permission=*/false,
      /*captured_surface_control_active=*/false);

  base::test::TestFuture<blink::mojom::MediaStreamRequestResult> future;
  pwc()->web_contents()->GetDelegate()->RequestMediaAccessPermission(
      pwc()->web_contents(), request, BindResultToFuture(future));
  EXPECT_EQ(future.Get(),
            blink::mojom::MediaStreamRequestResult::INVALID_SECURITY_ORIGIN);
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

class TestGlicViewForFileChooser : public GlicView {
 public:
  using GlicView::GlicView;

  void RunFileChooser(content::RenderFrameHost* render_frame_host,
                      scoped_refptr<content::FileSelectListener> listener,
                      const blink::mojom::FileChooserParams& params) override {
    last_rfh_ = render_frame_host;
    file_chooser_called_ = true;
    if (listener) {
      listener->FileSelectionCanceled();
    }
  }

  raw_ptr<content::RenderFrameHost> last_rfh_ = nullptr;
  bool file_chooser_called_ = false;
};

TEST_F(GlicViewNoWebviewTest, RunFileChooser_ForwardsFromPwc) {
  auto test_view = std::make_unique<TestGlicViewForFileChooser>(
      profile(), gfx::Size(800, 600), nullptr);

  test_view->SetWebContents(pwc()->web_contents());
  ASSERT_EQ(pwc()->embedder_delegate(), test_view.get());

  content::RenderFrameHost* rfh = pwc()->web_contents()->GetPrimaryMainFrame();
  auto listener = base::MakeRefCounted<TestFileSelectListener>();
  blink::mojom::FileChooserParams params;
  params.mode = blink::mojom::FileChooserParams::Mode::kOpen;

  pwc()->web_contents()->GetDelegate()->RunFileChooser(rfh, listener, params);
  EXPECT_TRUE(test_view->file_chooser_called_);
  EXPECT_EQ(test_view->last_rfh_, rfh);
  EXPECT_TRUE(listener->canceled());
}

TEST_F(GlicViewNoWebviewTest, CanDragEnter_ForwardsFromPwc) {
  content::DropData drop_data;
  drop_data.filenames.emplace_back(
      base::FilePath(FILE_PATH_LITERAL("test.txt")),
      base::FilePath(FILE_PATH_LITERAL("test.txt")));
  blink::DragOperationsMask ops = blink::kDragOperationCopy;

  // Forwards through PWC delegate to GlicView::CanDragEnter.
  EXPECT_TRUE(pwc()->web_contents()->GetDelegate()->CanDragEnter(
      pwc()->web_contents(), drop_data, ops));

  // Empty drop data returns false from GlicView::CanDragEnter.
  content::DropData empty_drop_data;
  EXPECT_FALSE(pwc()->web_contents()->GetDelegate()->CanDragEnter(
      pwc()->web_contents(), empty_drop_data, ops));
}

TEST_F(GlicViewNoWebviewTest, DraggableRegionsChanged_ForwardsFromPwc) {
  std::vector<blink::mojom::DraggableRegionPtr> regions;
  auto region = blink::mojom::DraggableRegion::New();
  region->bounds = gfx::Rect(0, 0, 800, 50);
  region->draggable = true;
  regions.push_back(std::move(region));

  EXPECT_FALSE(glic_view()->IsPointWithinDraggableRegion(gfx::Point(10, 10)));

  // Forwards through PWC delegate to GlicView::DraggableRegionsChanged.
  pwc()->web_contents()->GetDelegate()->DraggableRegionsChanged(
      regions, pwc()->web_contents());

  EXPECT_TRUE(glic_view()->IsPointWithinDraggableRegion(gfx::Point(10, 10)));
  EXPECT_FALSE(glic_view()->IsPointWithinDraggableRegion(gfx::Point(10, 100)));
}

class TestGlicViewForWindowCreation : public GlicView {
 public:
  using GlicView::GlicView;

  bool HandleWindowCreation(
      content::RenderFrameHost* opener,
      const GURL& target_url,
      WindowOpenDisposition disposition,
      const blink::mojom::WindowFeatures& features) override {
    last_opener_ = opener;
    last_url_ = target_url;
    last_disposition_ = disposition;
    window_creation_called_ = true;
    return true;
  }

  raw_ptr<content::RenderFrameHost> last_opener_ = nullptr;
  GURL last_url_;
  WindowOpenDisposition last_disposition_ = WindowOpenDisposition::kUnknown;
  bool window_creation_called_ = false;
};

TEST_F(GlicViewNoWebviewTest, HandleWindowCreation_ForwardsFromPwc) {
  auto test_view = std::make_unique<TestGlicViewForWindowCreation>(
      profile(), gfx::Size(800, 600), nullptr);

  test_view->SetWebContents(pwc()->web_contents());
  ASSERT_EQ(pwc()->embedder_delegate(), test_view.get());

  content::RenderFrameHost* rfh = pwc()->web_contents()->GetPrimaryMainFrame();
  GURL target_url("https://example.com");
  blink::mojom::WindowFeatures features;

  EXPECT_TRUE(pwc()->HandleWindowCreation(
      rfh, target_url, WindowOpenDisposition::kNewForegroundTab, features));
  EXPECT_TRUE(test_view->window_creation_called_);
  EXPECT_EQ(test_view->last_opener_, rfh);
  EXPECT_EQ(test_view->last_url_, target_url);
  EXPECT_EQ(test_view->last_disposition_,
            WindowOpenDisposition::kNewForegroundTab);
}

TEST_F(GlicViewNoWebviewTest, SetWebContents_InformsSize) {
  auto widget = CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  widget->SetBounds(gfx::Rect(0, 0, 500, 700));
  auto* view = widget->SetContentsView(
      std::make_unique<GlicView>(profile(), gfx::Size(500, 700), nullptr));
  widget->LayoutRootViewIfNecessary();

  auto fresh_pwc = CreatePwc();
  content::WebContents* wc = fresh_pwc->web_contents();
  ASSERT_TRUE(wc);
  ASSERT_TRUE(wc->GetRenderWidgetHostView());

  ASSERT_FALSE(view->GetContentsBounds().size().IsEmpty());
  view->SetWebContents(wc);
  EXPECT_EQ(wc->GetRenderWidgetHostView()->GetVisibleViewportSize(),
            view->GetContentsBounds().size());
}

TEST_F(GlicViewNoWebviewTest, RenderFrameCreated_InformsSize) {
  auto widget = CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  widget->SetBounds(gfx::Rect(0, 0, 500, 700));
  auto* view = widget->SetContentsView(
      std::make_unique<GlicView>(profile(), gfx::Size(500, 700), nullptr));
  widget->LayoutRootViewIfNecessary();

  auto fresh_pwc = CreatePwc();
  content::WebContents* wc = fresh_pwc->web_contents();
  ASSERT_TRUE(wc);
  ASSERT_TRUE(wc->GetRenderWidgetHostView());

  content::RenderFrameHostTester::For(wc->GetPrimaryMainFrame())
      ->InitializeRenderFrameIfNeeded();

  ASSERT_FALSE(view->GetContentsBounds().size().IsEmpty());
  view->SetWebContents(wc);
  // Reset the size to simulate a newly created frame before sizing.
  wc->GetRenderWidgetHostView()->SetSize(gfx::Size(100, 100));
  ASSERT_EQ(wc->GetRenderWidgetHostView()->GetVisibleViewportSize(),
            gfx::Size(100, 100));

  view->RenderFrameCreated(wc->GetPrimaryMainFrame());
  EXPECT_EQ(wc->GetRenderWidgetHostView()->GetVisibleViewportSize(),
            view->GetContentsBounds().size());
}

TEST_F(GlicViewNoWebviewTest, RenderFrameHostChanged_InformsSize) {
  auto widget = CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  widget->SetBounds(gfx::Rect(0, 0, 500, 700));
  auto* view = widget->SetContentsView(
      std::make_unique<GlicView>(profile(), gfx::Size(500, 700), nullptr));
  widget->LayoutRootViewIfNecessary();

  auto fresh_pwc = CreatePwc();
  content::WebContents* wc = fresh_pwc->web_contents();
  ASSERT_TRUE(wc);
  ASSERT_TRUE(wc->GetRenderWidgetHostView());

  ASSERT_FALSE(view->GetContentsBounds().size().IsEmpty());
  view->SetWebContents(wc);
  // Reset the size to simulate a newly created frame before sizing.
  wc->GetRenderWidgetHostView()->SetSize(gfx::Size(100, 100));
  ASSERT_EQ(wc->GetRenderWidgetHostView()->GetVisibleViewportSize(),
            gfx::Size(100, 100));

  view->RenderFrameHostChanged(nullptr, wc->GetPrimaryMainFrame());
  EXPECT_EQ(wc->GetRenderWidgetHostView()->GetVisibleViewportSize(),
            view->GetContentsBounds().size());
}

TEST_F(GlicViewNoWebviewTest, OnBoundsChanged_InformsSize) {
  auto widget = CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  widget->SetBounds(gfx::Rect(0, 0, 200, 300));
  auto* view = widget->SetContentsView(
      std::make_unique<GlicView>(profile(), gfx::Size(200, 300), nullptr));
  widget->LayoutRootViewIfNecessary();

  auto fresh_pwc = CreatePwc();
  content::WebContents* wc = fresh_pwc->web_contents();
  ASSERT_TRUE(wc);
  ASSERT_TRUE(wc->GetRenderWidgetHostView());

  view->SetWebContents(wc);
  EXPECT_EQ(wc->GetRenderWidgetHostView()->GetVisibleViewportSize(),
            view->GetContentsBounds().size());
  EXPECT_EQ(view->GetContentsBounds().size(), gfx::Size(200, 300));

  widget->SetBounds(gfx::Rect(0, 0, 300, 400));
  widget->LayoutRootViewIfNecessary();
  EXPECT_EQ(wc->GetRenderWidgetHostView()->GetVisibleViewportSize(),
            view->GetContentsBounds().size());
  EXPECT_EQ(view->GetContentsBounds().size(), gfx::Size(300, 400));
}

TEST_F(GlicViewNoWebviewTest, EmptyInitialSize_InformsSizeAfterLayout) {
  auto widget = CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  widget->SetBounds(gfx::Rect(0, 0, 200, 300));

  auto view = std::make_unique<GlicView>(profile(), gfx::Size(), nullptr);

  auto fresh_pwc = CreatePwc();
  content::WebContents* wc = fresh_pwc->web_contents();
  ASSERT_TRUE(wc);
  ASSERT_TRUE(wc->GetRenderWidgetHostView());

  view->SetWebContents(wc);

  auto* view_ptr = widget->SetContentsView(std::move(view));
  widget->LayoutRootViewIfNecessary();
  EXPECT_EQ(wc->GetRenderWidgetHostView()->GetVisibleViewportSize(),
            view_ptr->GetContentsBounds().size());
  EXPECT_EQ(view_ptr->GetContentsBounds().size(), gfx::Size(200, 300));
}

TEST_F(GlicViewTest, NativeLoadingPlaceholderFeaturePrecedence) {
  EXPECT_FALSE(features::IsGlicNativeLoadingPlaceholderEnabled());

  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeature(features::kGlicNativeLoadingPlaceholder);
    EXPECT_TRUE(features::IsGlicNativeLoadingPlaceholderEnabled());
  }

  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitWithFeatures(
        {features::kGlicNativeLoadingPlaceholder, features::kGlicNoWebUiLoader},
        {});
    EXPECT_FALSE(features::IsGlicNativeLoadingPlaceholderEnabled());
  }
}

TEST_F(GlicViewTest, LoadingPlaceholder_SidePanelLayoutAndCloseCallback) {
  auto widget = CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  widget->SetBounds(gfx::Rect(0, 0, 400, 700));

  int close_count = 0;
  auto* placeholder =
      widget->SetContentsView(std::make_unique<GlicLoadingPlaceholderView>(
          GlicLoadingPlaceholderView::Mode::kSidePanel,
          base::BindRepeating([](int* count) { ++(*count); }, &close_count)));
  widget->LayoutRootViewIfNecessary();

  EXPECT_EQ(placeholder->mode_for_testing(),
            GlicLoadingPlaceholderView::Mode::kSidePanel);
  ASSERT_EQ(placeholder->block_count_for_testing(), 4u);
  for (size_t i = 0; i < placeholder->block_count_for_testing(); ++i) {
    views::View* block = placeholder->block_for_testing(i);
    ASSERT_NE(block, nullptr);
    EXPECT_FALSE(block->bounds().IsEmpty());
  }
  EXPECT_EQ(placeholder->block_for_testing(0)->bounds().size(),
            gfx::Size(78, 17));
  EXPECT_EQ(placeholder->block_for_testing(1)->bounds().size(),
            gfx::Size(78, 17));
  EXPECT_EQ(placeholder->block_for_testing(2)->bounds().size(),
            gfx::Size(380, 244));
  EXPECT_EQ(placeholder->block_for_testing(3)->bounds().size(),
            gfx::Size(380, 112));

  // Side panel placeholder is never draggable.
  EXPECT_FALSE(placeholder->IsPointWithinDraggableHeader(gfx::Point(20, 20)));

  // On a short side panel, p3 shrinks gracefully so p4 stays fully visible.
  widget->SetBounds(gfx::Rect(0, 0, 400, 260));
  widget->LayoutRootViewIfNecessary();
  EXPECT_LT(placeholder->block_for_testing(2)->bounds().height(), 244);
  EXPECT_EQ(placeholder->block_for_testing(3)->bounds().height(), 112);

  // Clicking the close button invokes the close callback.
  ASSERT_NE(placeholder->close_button_for_testing(), nullptr);
  views::test::ButtonTestApi(placeholder->close_button_for_testing())
      .NotifyDefaultMouseClick();
  EXPECT_EQ(close_count, 1);
}

TEST_F(GlicViewTest, LoadingPlaceholder_FloatingLayoutAndDraggableRegion) {
  auto widget = CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  widget->SetBounds(gfx::Rect(0, 0, 400, 300));

  int close_count = 0;
  auto* placeholder =
      widget->SetContentsView(std::make_unique<GlicLoadingPlaceholderView>(
          GlicLoadingPlaceholderView::Mode::kFloating,
          base::BindRepeating([](int* count) { ++(*count); }, &close_count)));
  widget->LayoutRootViewIfNecessary();

  EXPECT_EQ(placeholder->mode_for_testing(),
            GlicLoadingPlaceholderView::Mode::kFloating);
  ASSERT_EQ(placeholder->block_count_for_testing(), 3u);
  EXPECT_EQ(placeholder->block_for_testing(1)->bounds().size(),
            gfx::Size(80, 32));
  EXPECT_EQ(placeholder->block_for_testing(2)->bounds().size(),
            gfx::Size(32, 32));

  // Header left area is draggable; close button and body row are not.
  EXPECT_TRUE(placeholder->IsPointWithinDraggableHeader(gfx::Point(25, 20)));
  views::ImageButton* close_button = placeholder->close_button_for_testing();
  ASSERT_NE(close_button, nullptr);
  gfx::Point close_center = close_button->bounds().CenterPoint();
  views::View::ConvertPointToTarget(close_button->parent(), placeholder,
                                    &close_center);
  EXPECT_FALSE(placeholder->IsPointWithinDraggableHeader(close_center));

  gfx::Point second_row_point =
      placeholder->block_for_testing(1)->bounds().CenterPoint();
  views::View::ConvertPointToTarget(placeholder->block_for_testing(1)->parent(),
                                    placeholder, &second_row_point);
  EXPECT_FALSE(placeholder->IsPointWithinDraggableHeader(second_row_point));

  // Hiding disables header dragging.
  placeholder->SetVisible(false);
  EXPECT_FALSE(placeholder->IsPointWithinDraggableHeader(gfx::Point(25, 20)));

  views::test::ButtonTestApi(close_button).NotifyDefaultMouseClick();
  EXPECT_EQ(close_count, 1);
}

TEST_F(GlicViewTest, LoadingPlaceholder_PulseAnimationStaggerAndReset) {
  auto render_mode = gfx::AnimationTestApi::SetRichAnimationRenderMode(
      gfx::Animation::RichAnimationRenderMode::FORCE_ENABLED);

  auto widget = CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  widget->SetBounds(gfx::Rect(0, 0, 400, 700));
  auto* placeholder =
      widget->SetContentsView(std::make_unique<GlicLoadingPlaceholderView>(
          GlicLoadingPlaceholderView::Mode::kSidePanel, base::DoNothing()));
  widget->Show();
  widget->LayoutRootViewIfNecessary();

  EXPECT_TRUE(placeholder->is_animating_for_testing());

  // At 200ms: p1 (0ms delay) and p2 (100ms delay) have started dimming, while
  // p3 (300ms delay) and p4 (500ms delay) are still at full 1.0 opacity.
  placeholder->ApplyAnimationElapsedForTesting(base::Milliseconds(200));
  EXPECT_LT(placeholder->block_for_testing(0)->layer()->opacity(), 1.0f);
  EXPECT_LT(placeholder->block_for_testing(1)->layer()->opacity(), 1.0f);
  EXPECT_GT(placeholder->block_for_testing(1)->layer()->opacity(),
            placeholder->block_for_testing(0)->layer()->opacity());
  EXPECT_FLOAT_EQ(placeholder->block_for_testing(2)->layer()->opacity(), 1.0f);
  EXPECT_FLOAT_EQ(placeholder->block_for_testing(3)->layer()->opacity(), 1.0f);

  // At 990ms (66% of 1500ms), p1 reaches the 0.6 trough while p4 has started.
  placeholder->ApplyAnimationElapsedForTesting(base::Milliseconds(990));
  EXPECT_NEAR(placeholder->block_for_testing(0)->layer()->opacity(), 0.6f,
              0.01f);
  EXPECT_LT(placeholder->block_for_testing(3)->layer()->opacity(), 1.0f);
  EXPECT_GE(placeholder->block_for_testing(3)->layer()->opacity(), 0.6f);

  // Hiding stops the animation and resets all block opacities to 1.0.
  placeholder->SetVisible(false);
  EXPECT_FALSE(placeholder->is_animating_for_testing());
  for (size_t i = 0; i < placeholder->block_count_for_testing(); ++i) {
    EXPECT_FLOAT_EQ(placeholder->block_for_testing(i)->layer()->opacity(),
                    1.0f);
  }
}

TEST_F(GlicViewTest, SetShowingLoadingPlaceholder_TogglesHolderAndForwards) {
  content::RenderViewHostTestEnabler rvh_test_enabler;

  auto widget = CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  widget->SetBounds(gfx::Rect(0, 0, 400, 500));
  auto* view = widget->SetContentsView(
      std::make_unique<GlicView>(profile(), gfx::Size(400, 500), nullptr));
  widget->LayoutRootViewIfNecessary();

  int close_count = 0;
  view->ConfigureLoadingPlaceholder(
      GlicLoadingPlaceholderView::Mode::kFloating,
      base::BindRepeating([](int* count) { ++(*count); }, &close_count));
  view->SetBackgroundRoundedCorners(gfx::RoundedCornersF(16.0f));

  EXPECT_FALSE(view->IsShowingLoadingPlaceholder());
  EXPECT_EQ(view->loading_placeholder_view_for_testing(), nullptr);
  EXPECT_TRUE(view->holder()->GetVisible());

  view->SetShowingLoadingPlaceholder(true);
  widget->LayoutRootViewIfNecessary();
  EXPECT_TRUE(view->IsShowingLoadingPlaceholder());
  EXPECT_FALSE(view->holder()->GetVisible());
  auto* placeholder = view->loading_placeholder_view_for_testing();
  ASSERT_NE(placeholder, nullptr);
  EXPECT_TRUE(placeholder->GetVisible());
  EXPECT_EQ(placeholder->bounds(), view->GetContentsBounds());
  ASSERT_NE(placeholder->GetBackground(), nullptr);
  EXPECT_EQ(placeholder->GetBackground()->GetRoundedCornerRadii(),
            gfx::RoundedCornersF(16.0f));

  // Attaching WebContents while loading placeholder is active keeps holder()
  // hidden so the native WebContents view cannot occlude the placeholder.
  auto web_contents = content::WebContents::Create(
      content::WebContents::CreateParams(profile()));
  view->SetWebContents(web_contents.get());
  EXPECT_FALSE(view->holder()->GetVisible());

  // Floating draggable header forwards through
  // GlicView::IsPointWithinDraggableRegion.
  EXPECT_TRUE(view->IsPointWithinDraggableRegion(gfx::Point(25, 20)));
  EXPECT_FALSE(view->IsPointWithinDraggableRegion(gfx::Point(25, 100)));

  // Configuring borders/insets on `GlicView` sizes the placeholder to
  // `GetContentsBounds()` and translates draggable header coordinates. Keep
  // widget height <= 552dip so ChromeOS Ash's 800x600 display work area (minus
  // the 48dip shelf) does not clamp the widget height.
  view->SetBorder(views::CreateEmptyBorder(gfx::Insets::TLBR(4, 6, 8, 10)));
  widget->SetBounds(gfx::Rect(0, 0, 420, 500));
  widget->LayoutRootViewIfNecessary();
  EXPECT_EQ(placeholder->bounds(), view->GetContentsBounds());
  EXPECT_EQ(placeholder->bounds(), gfx::Rect(6, 4, 404, 488));
  EXPECT_FALSE(view->IsPointWithinDraggableRegion(gfx::Point(25, 2)));
  EXPECT_TRUE(view->IsPointWithinDraggableRegion(gfx::Point(31, 24)));
  view->SetBorder(nullptr);
  widget->SetBounds(gfx::Rect(0, 0, 400, 500));
  widget->LayoutRootViewIfNecessary();

  // Theme changes resolve kColorGlicBackground and kColorGlicPlaceholderElem
  // cleanly in both light and dark modes.
  widget->SetColorModeOverride(ui::ColorProviderKey::ColorMode::kLight);
  view->OnThemeChanged();
  const SkColor light_bg =
      placeholder->GetColorProvider()->GetColor(kColorGlicBackground);
  const SkColor light_elem =
      placeholder->GetColorProvider()->GetColor(kColorGlicPlaceholderElem);
  EXPECT_EQ(placeholder->GetBackground()->color().ResolveToSkColor(
                placeholder->GetColorProvider()),
            light_bg);
  EXPECT_EQ(placeholder->block_for_testing(0)
                ->GetBackground()
                ->color()
                .ResolveToSkColor(placeholder->GetColorProvider()),
            light_elem);

  widget->SetColorModeOverride(ui::ColorProviderKey::ColorMode::kDark);
  view->OnThemeChanged();
  const SkColor dark_bg =
      placeholder->GetColorProvider()->GetColor(kColorGlicBackground);
  const SkColor dark_elem =
      placeholder->GetColorProvider()->GetColor(kColorGlicPlaceholderElem);
  EXPECT_NE(light_bg, dark_bg);
  EXPECT_NE(light_elem, dark_elem);
  EXPECT_EQ(placeholder->GetBackground()->color().ResolveToSkColor(
                placeholder->GetColorProvider()),
            dark_bg);
  EXPECT_EQ(placeholder->block_for_testing(0)
                ->GetBackground()
                ->color()
                .ResolveToSkColor(placeholder->GetColorProvider()),
            dark_elem);

  views::test::ButtonTestApi(placeholder->close_button_for_testing())
      .NotifyDefaultMouseClick();
  EXPECT_EQ(close_count, 1);

  // Dismissing the loading placeholder restores holder() visibility.
  view->SetShowingLoadingPlaceholder(false);
  EXPECT_FALSE(view->IsShowingLoadingPlaceholder());
  EXPECT_FALSE(placeholder->GetVisible());
  EXPECT_TRUE(view->holder()->GetVisible());

  view->SetWebContents(nullptr);
}

TEST_F(GlicViewPixelTest,
       SetShowingLoadingPlaceholder_ConfiguredHiddenBeforeWidgetAttach) {
  content::RenderViewHostTestEnabler rvh_test_enabler;

  // Mirrors GlicFloatingUi::CreateAndSetupWidget when detaching an already-
  // loaded panel: GlicView is constructed with non-empty initial_size,
  // ConfigureLoadingPlaceholder + SetShowingLoadingPlaceholder(false) run
  // before GlicView is attached to GlicWidget, and SetShowingLoadingPlaceholder
  // is toggled to true later on reload without a widget resize.
  auto detached_view =
      std::make_unique<GlicView>(profile(), gfx::Size(400, 500), nullptr);
  GlicView* view = detached_view.get();
  view->ConfigureLoadingPlaceholder(GlicLoadingPlaceholderView::Mode::kFloating,
                                    base::DoNothing());
  view->SetShowingLoadingPlaceholder(false);
  EXPECT_EQ(view->loading_placeholder_view_for_testing(), nullptr);

  auto web_contents = content::WebContents::Create(
      content::WebContents::CreateParams(profile()));
  view->SetWebContents(web_contents.get());

  auto delegate =
      GlicWidget::CreateWidgetDelegate(std::move(detached_view), false);
  auto widget =
      GlicWidget::Create(delegate.get(), profile(), gfx::Rect(10, 10, 400, 500),
                         false, GetContext());
  widget->Show();
  view->UpdateBackgroundColor();
  widget->SetColorModeOverride(ui::ColorProviderKey::ColorMode::kLight);
  // Also simulate SetDragResizeEnabled(true) called by PanelWillOpenComplete.
  delegate->SetCanResize(true);
  view->UpdateBackgroundColor();

  // Simulate guest reload transitioning into native loading without resizing.
  view->SetShowingLoadingPlaceholder(true);
  view->SetWebContents(nullptr);

  auto* placeholder = view->loading_placeholder_view_for_testing();
  ASSERT_NE(placeholder, nullptr);
  ASSERT_NE(view->layer(), nullptr);
  ASSERT_NE(placeholder->layer(), nullptr);
  EXPECT_TRUE(placeholder->GetVisible());
  EXPECT_TRUE(placeholder->IsDrawn());
  EXPECT_TRUE(placeholder->layer()->visible());
  EXPECT_TRUE(placeholder->layer()->IsVisible());
  EXPECT_TRUE(placeholder->layer()->clip_rect().IsEmpty());
  EXPECT_EQ(placeholder->layer()->parent(), view->layer());
  EXPECT_EQ(placeholder->bounds(), view->GetContentsBounds());
  EXPECT_EQ(placeholder->layer()->bounds(), view->GetContentsBounds());

  ASSERT_EQ(placeholder->block_count_for_testing(), 3u);
  for (size_t i = 0; i < placeholder->block_count_for_testing(); ++i) {
    views::View* block = placeholder->block_for_testing(i);
    ASSERT_NE(block, nullptr) << "block " << i;
    ASSERT_NE(block->layer(), nullptr) << "block " << i;
    EXPECT_FALSE(block->bounds().IsEmpty()) << "block " << i;
    EXPECT_FALSE(block->layer()->bounds().IsEmpty()) << "block " << i;
    EXPECT_EQ(block->layer()->parent(), placeholder->layer()) << "block " << i;
    EXPECT_TRUE(block->layer()->visible()) << "block " << i;
    EXPECT_TRUE(block->layer()->IsVisible()) << "block " << i;
  }

  views::ImageButton* close_button = placeholder->close_button_for_testing();
  ASSERT_NE(close_button, nullptr);
  ASSERT_NE(close_button->layer(), nullptr);
  EXPECT_FALSE(close_button->bounds().IsEmpty());
  EXPECT_FALSE(close_button->layer()->bounds().IsEmpty());
  EXPECT_EQ(close_button->layer()->parent(), placeholder->layer());
  EXPECT_TRUE(close_button->layer()->visible());
  EXPECT_TRUE(close_button->layer()->IsVisible());

  ui::Compositor* compositor = widget->GetCompositor();
  ASSERT_NE(compositor, nullptr);
  base::test::TestFuture<std::unique_ptr<viz::CopyOutputResult>> copy_future;
  auto copy_request = std::make_unique<viz::CopyOutputRequest>(
      viz::CopyOutputRequest::ResultFormat::RGBA,
      viz::CopyOutputRequest::ResultDestination::kSystemMemory,
      copy_future.GetCallback());
  copy_request->set_result_task_runner(
      base::SequencedTaskRunner::GetCurrentDefault());
  view->layer()->RequestCopyOfOutput(std::move(copy_request));
  for (int i = 0; i < 2; ++i) {
    compositor->ScheduleFullRedraw();
    ui::DrawWaiterForTest::WaitForCompositingStarted(compositor);
  }

  std::unique_ptr<viz::CopyOutputResult> copy_result = copy_future.Take();
  ASSERT_NE(copy_result, nullptr);
  ASSERT_FALSE(copy_result->IsEmpty());
  auto scoped_bitmap = copy_result->ScopedAccessSkBitmap();
  SkBitmap bmp = scoped_bitmap.GetOutScopedBitmap();
  ASSERT_FALSE(bmp.drawsNothing());

  const SkColor bg_color =
      placeholder->GetColorProvider()->GetColor(kColorGlicBackground);
  const SkColor elem_color =
      placeholder->GetColorProvider()->GetColor(kColorGlicPlaceholderElem);
  gfx::Point p1_pt = placeholder->block_for_testing(0)->bounds().CenterPoint();
  views::View::ConvertPointToTarget(placeholder->block_for_testing(0)->parent(),
                                    view, &p1_pt);
  gfx::Point p2_pt = placeholder->block_for_testing(1)->bounds().CenterPoint();
  views::View::ConvertPointToTarget(placeholder->block_for_testing(1)->parent(),
                                    view, &p2_pt);
  const float scale_x = static_cast<float>(bmp.width()) / view->width();
  const float scale_y = static_cast<float>(bmp.height()) / view->height();
  auto sample_view_dip = [&](gfx::Point pt_in_view) {
    return bmp.getColor(base::ClampRound(pt_in_view.x() * scale_x),
                        base::ClampRound(pt_in_view.y() * scale_y));
  };
  EXPECT_EQ(sample_view_dip(gfx::Point(200, 300)), bg_color);
  EXPECT_EQ(sample_view_dip(p1_pt), elem_color);
  EXPECT_EQ(sample_view_dip(p2_pt), elem_color);
}

TEST_F(
    GlicViewTest,
    FloatingLoadingPlaceholderInitialFocusAvoidsCloseButtonFocusRingAndRestoresFocus) {
  content::RenderViewHostTestEnabler rvh_test_enabler;

  for (const bool force_accessible_only_close_button : {false, true}) {
    for (const bool keyboard_accessible : {false, true}) {
      SCOPED_TRACE(testing::Message()
                   << "force_accessible_only_close_button="
                   << force_accessible_only_close_button
                   << " keyboard_accessible=" << keyboard_accessible);
      auto detached_view =
          std::make_unique<GlicView>(profile(), gfx::Size(400, 500), nullptr);
      GlicView* view = detached_view.get();
      view->ConfigureLoadingPlaceholder(
          GlicLoadingPlaceholderView::Mode::kFloating, base::DoNothing());
      view->SetShowingLoadingPlaceholder(true);

      auto delegate =
          GlicWidget::CreateWidgetDelegate(std::move(detached_view), false);
      auto widget =
          GlicWidget::Create(delegate.get(), profile(),
                             gfx::Rect(10, 10, 400, 500), false, GetContext());
      widget->Show();
      widget->Activate();

      views::FocusManager* focus_manager = widget->GetFocusManager();
      ASSERT_NE(focus_manager, nullptr);
      focus_manager->SetKeyboardAccessible(keyboard_accessible);

      auto* placeholder = view->loading_placeholder_view_for_testing();
      ASSERT_NE(placeholder, nullptr);
      views::ImageButton* close_button =
          placeholder->close_button_for_testing();
      ASSERT_NE(close_button, nullptr);
      if (force_accessible_only_close_button) {
        close_button->SetFocusBehavior(
            views::View::FocusBehavior::ACCESSIBLE_ONLY);
      }
      auto* focus_ring = views::FocusRing::Get(close_button);
      ASSERT_NE(focus_ring, nullptr);

      // Initial activation (Widget::SetInitialFocus -> GlicView::RequestFocus)
      // focuses the placeholder surface rather than auto-focusing the Close
      // button, avoiding a spurious circular focus ring on open.
      view->RequestFocus();
      EXPECT_EQ(focus_manager->GetFocusedView(), placeholder);
      EXPECT_FALSE(focus_ring->ShouldPaintForTesting());

      // Keyboard Tab traversal reaches the Close button and paints its focus
      // ring whenever the button participates in Tab focus (always on
      // non-macOS, and on macOS ACCESSIBLE_ONLY when keyboard_accessible is
      // enabled).
      focus_manager->AdvanceFocus(/*reverse=*/false);
      const bool close_button_is_tab_focusable =
          close_button->IsFocusable() ||
          (keyboard_accessible &&
           close_button->GetViewAccessibility().IsAccessibilityFocusable());
      if (close_button_is_tab_focusable) {
        EXPECT_EQ(focus_manager->GetFocusedView(), close_button);
        EXPECT_TRUE(focus_ring->ShouldPaintForTesting());
      } else {
        EXPECT_EQ(focus_manager->GetFocusedView(), placeholder);
        EXPECT_FALSE(focus_ring->ShouldPaintForTesting());
      }

      // Swapping from native loading to a ready WebContents hands focus off to
      // GlicView cleanly instead of dropping focus when the placeholder hides.
      auto web_contents = content::WebContents::Create(
          content::WebContents::CreateParams(profile()));
      view->SetWebContents(web_contents.get());
      view->SetShowingLoadingPlaceholder(false);
      EXPECT_TRUE(view->GetViewAccessibility().IsAccessibilityFocusable());
      EXPECT_EQ(focus_manager->GetFocusedView(), view);

      // Mid-session reload transitions back into native loading and detaches
      // the active WebContents, handing focus back to the placeholder surface
      // without painting a spurious focus ring on the Close button.
      view->SetShowingLoadingPlaceholder(true);
      view->SetWebContents(nullptr);
      EXPECT_FALSE(view->GetViewAccessibility().IsAccessibilityFocusable());
      EXPECT_EQ(focus_manager->GetFocusedView(), placeholder);
      EXPECT_FALSE(focus_ring->ShouldPaintForTesting());
    }
  }
}

TEST_F(GlicViewTest,
       LoadingPlaceholder_RtlMirroringAndDraggableHeaderExcludesCloseButton) {
  base::test::ScopedRestoreICUDefaultLocale scoped_rtl("he");
  ASSERT_TRUE(base::i18n::IsRTL());

  // 1. Side Panel RTL layout mirrors p1 to the physical right and Close button
  // to the physical left.
  {
    auto widget =
        CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
    widget->SetBounds(gfx::Rect(0, 0, 400, 700));
    auto* placeholder =
        widget->SetContentsView(std::make_unique<GlicLoadingPlaceholderView>(
            GlicLoadingPlaceholderView::Mode::kSidePanel, base::DoNothing()));
    widget->LayoutRootViewIfNecessary();

    views::ImageButton* close_button = placeholder->close_button_for_testing();
    ASSERT_NE(close_button, nullptr);
    EXPECT_EQ(close_button->GetMirroredBounds(), gfx::Rect(20, 12, 24, 24));
    EXPECT_EQ(placeholder->block_for_testing(0)->GetMirroredBounds().right(),
              400 - 12);
    EXPECT_LT(close_button->GetBoundsInScreen().right(),
              placeholder->block_for_testing(1)->GetBoundsInScreen().x());
  }

  // 2. Floating RTL layout mirrors Close button to physical (20, 12, 24x24)
  // and excludes that physical region from HTCAPTION dragging while allowing
  // dragging on the physical right side of the header.
  {
    auto widget =
        CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
    widget->SetBounds(gfx::Rect(0, 0, 400, 300));
    int close_count = 0;
    auto* placeholder =
        widget->SetContentsView(std::make_unique<GlicLoadingPlaceholderView>(
            GlicLoadingPlaceholderView::Mode::kFloating,
            base::BindRepeating([](int* count) { ++(*count); }, &close_count)));
    widget->LayoutRootViewIfNecessary();

    views::ImageButton* close_button = placeholder->close_button_for_testing();
    ASSERT_NE(close_button, nullptr);
    EXPECT_EQ(close_button->GetMirroredBounds(), gfx::Rect(20, 12, 24, 24));

    const gfx::Point physical_close_center = views::View::ConvertPointToTarget(
        close_button, placeholder,
        close_button->GetLocalBounds().CenterPoint());
    EXPECT_EQ(physical_close_center, gfx::Point(32, 24));
    EXPECT_FALSE(
        placeholder->IsPointWithinDraggableHeader(physical_close_center));
    EXPECT_TRUE(placeholder->IsPointWithinDraggableHeader(gfx::Point(368, 24)));
  }
}

TEST_F(GlicViewTest,
       LoadingPlaceholder_FocusBehaviorHierarchyAndStoredFocusHandoff) {
  content::RenderViewHostTestEnabler rvh_test_enabler;

  auto detached_view =
      std::make_unique<GlicView>(profile(), gfx::Size(400, 500), nullptr);
  GlicView* view = detached_view.get();
  view->ConfigureLoadingPlaceholder(
      GlicLoadingPlaceholderView::Mode::kSidePanel, base::DoNothing());
  view->SetShowingLoadingPlaceholder(true);

  auto web_contents = content::WebContents::Create(
      content::WebContents::CreateParams(profile()));
  view->SetWebContents(web_contents.get());

  auto delegate =
      GlicWidget::CreateWidgetDelegate(std::move(detached_view), false);
  auto widget =
      GlicWidget::Create(delegate.get(), profile(), gfx::Rect(10, 10, 400, 500),
                         false, GetContext());
  widget->Show();
  widget->Activate();

  // Even when a WebContents is attached (<webview> mode), GlicView itself is
  // unfocusable while covered by the native loading placeholder so keyboard Tab
  // traversal does not visit the hidden WebContents.
  EXPECT_EQ(view->GetFocusBehavior(), views::View::FocusBehavior::NEVER);
  EXPECT_FALSE(view->GetViewAccessibility().IsAccessibilityFocusable());

  view->RequestFocus();
  views::FocusManager* focus_manager = widget->GetFocusManager();
  ASSERT_NE(focus_manager, nullptr);
  EXPECT_FALSE(view->HasFocus());
  EXPECT_TRUE(view->HasFocusInHierarchy());

  // Reconfiguring the placeholder mode while focused preserves focus inside the
  // replacement placeholder without painting a focus ring on the Close button.
  view->ConfigureLoadingPlaceholder(GlicLoadingPlaceholderView::Mode::kFloating,
                                    base::DoNothing());
  auto* placeholder = view->loading_placeholder_view_for_testing();
  ASSERT_NE(placeholder, nullptr);
  EXPECT_EQ(focus_manager->GetFocusedView(), placeholder);
  EXPECT_TRUE(view->HasFocusInHierarchy());
  EXPECT_FALSE(views::FocusRing::Get(placeholder->close_button_for_testing())
                   ->ShouldPaintForTesting());

  // Simulate window deactivation storing focus on the placeholder; hiding the
  // placeholder transfers stored focus to GlicView and restores FocusBehavior.
  focus_manager->StoreFocusedView(/*clear_native_focus=*/true);
  ASSERT_EQ(focus_manager->GetFocusedView(), nullptr);
  ASSERT_EQ(focus_manager->GetStoredFocusView(), placeholder);

  view->SetShowingLoadingPlaceholder(false);
  EXPECT_NE(view->GetFocusBehavior(), views::View::FocusBehavior::NEVER);
  EXPECT_TRUE(view->GetViewAccessibility().IsAccessibilityFocusable());
  EXPECT_EQ(focus_manager->GetStoredFocusView(), view);

  view->SetWebContents(nullptr);
}

}  // namespace glic
