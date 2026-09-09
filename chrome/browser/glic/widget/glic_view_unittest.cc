// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/widget/glic_view.h"

#include <memory>

#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/browser/pwc/privileged_web_contents.h"
#include "chrome/browser/pwc/pwc_component_policy.h"
#include "chrome/browser/pwc/pwc_features.mojom-features.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "content/public/browser/file_select_listener.h"
#include "content/public/browser/media_stream_request.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/common/drop_data.h"
#include "content/public/test/test_renderer_host.h"
#include "third_party/blink/public/common/page/drag_operation.h"
#include "third_party/blink/public/mojom/choosers/file_chooser.mojom.h"
#include "third_party/blink/public/mojom/mediastream/media_stream.mojom.h"
#include "third_party/blink/public/mojom/page/draggable_region.mojom.h"
#include "ui/views/widget/widget.h"
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

  // DropData with URL should be accepted.
  drop_data.filenames.clear();
  drop_data.url_infos.emplace_back(GURL("https://example.com"),
                                   std::u16string());
  EXPECT_TRUE(glic_view->CanDragEnter(nullptr, drop_data, ops));

  // DropData with text should be rejected.
  drop_data.url_infos.clear();
  drop_data.text = u"test text";
  EXPECT_FALSE(glic_view->CanDragEnter(nullptr, drop_data, ops));

  // DropData with html should be rejected.
  drop_data.text.reset();
  drop_data.html = u"<b>test html</b>";
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
        {features::kGlicNoWebview, pwc::mojom::features::kPrivilegedWebContents,
         features::kGlicDragAndDropFileUpload},
        {});
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

}  // namespace glic
