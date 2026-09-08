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
#include "content/public/browser/media_stream_request.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/common/drop_data.h"
#include "content/public/test/test_renderer_host.h"
#include "third_party/blink/public/common/page/drag_operation.h"
#include "third_party/blink/public/mojom/mediastream/media_stream.mojom.h"
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

TEST_F(GlicViewTest, SetWebContents_NoWebview_ClearsOldDelegate) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      {features::kGlicNoWebview, pwc::mojom::features::kPrivilegedWebContents},
      {});

  content::RenderViewHostTestEnabler rvh_test_enabler;

  auto glic_view =
      std::make_unique<GlicView>(profile(), gfx::Size(800, 600), nullptr);

  auto policy_delegate = std::make_unique<pwc::FixedPwcPolicyDelegate>(
      std::vector<url::Origin>{
          url::Origin::Create(GURL("https://pwc-test.example.com"))},
      std::vector<url::Origin>{
          url::Origin::Create(GURL("https://pwc-test.example.com"))});
  std::unique_ptr<pwc::PrivilegedWebContents> pwc =
      pwc::PrivilegedWebContents::Create(pwc::PrivilegedComponent::kGlic,
                                         profile(), std::move(policy_delegate));
  ASSERT_TRUE(pwc);

  EXPECT_EQ(pwc->embedder_delegate(), nullptr);

  glic_view->SetWebContents(pwc->web_contents());
  EXPECT_EQ(pwc->embedder_delegate(), glic_view.get());

  glic_view->SetWebContents(nullptr);
  EXPECT_EQ(pwc->embedder_delegate(), nullptr);
}

TEST_F(GlicViewTest, SetWebContents_NoWebview_DoesNotClearIfOverwritten) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      {features::kGlicNoWebview, pwc::mojom::features::kPrivilegedWebContents},
      {});

  content::RenderViewHostTestEnabler rvh_test_enabler;

  TestEmbedderDelegate other_delegate;
  auto glic_view =
      std::make_unique<GlicView>(profile(), gfx::Size(800, 600), nullptr);

  auto policy_delegate = std::make_unique<pwc::FixedPwcPolicyDelegate>(
      std::vector<url::Origin>{
          url::Origin::Create(GURL("https://pwc-test.example.com"))},
      std::vector<url::Origin>{
          url::Origin::Create(GURL("https://pwc-test.example.com"))});
  std::unique_ptr<pwc::PrivilegedWebContents> pwc =
      pwc::PrivilegedWebContents::Create(pwc::PrivilegedComponent::kGlic,
                                         profile(), std::move(policy_delegate));
  ASSERT_TRUE(pwc);

  glic_view->SetWebContents(pwc->web_contents());
  EXPECT_EQ(pwc->embedder_delegate(), glic_view.get());

  pwc->SetEmbedderDelegate(&other_delegate);
  EXPECT_EQ(pwc->embedder_delegate(), &other_delegate);

  glic_view->SetWebContents(nullptr);
  EXPECT_EQ(pwc->embedder_delegate(), &other_delegate);

  pwc->SetEmbedderDelegate(nullptr);
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

TEST_F(GlicViewTest, CheckMediaAccessPermission_NoWebview_ForwardsFromPwc) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      {features::kGlicNoWebview, pwc::mojom::features::kPrivilegedWebContents},
      {});

  content::RenderViewHostTestEnabler rvh_test_enabler;
  auto glic_view =
      std::make_unique<GlicView>(profile(), gfx::Size(800, 600), nullptr);

  auto policy_delegate = std::make_unique<pwc::FixedPwcPolicyDelegate>(
      std::vector<url::Origin>{
          url::Origin::Create(GURL("https://pwc-test.example.com"))},
      std::vector<url::Origin>{
          url::Origin::Create(GURL("https://pwc-test.example.com"))});
  std::unique_ptr<pwc::PrivilegedWebContents> pwc =
      pwc::PrivilegedWebContents::Create(pwc::PrivilegedComponent::kGlic,
                                         profile(), std::move(policy_delegate));
  ASSERT_TRUE(pwc);

  glic_view->SetWebContents(pwc->web_contents());
  ASSERT_EQ(pwc->embedder_delegate(), glic_view.get());

  EXPECT_FALSE(pwc->web_contents()->GetDelegate()->CheckMediaAccessPermission(
      pwc->web_contents()->GetPrimaryMainFrame(),
      url::Origin::Create(GURL("https://pwc-test.example.com")),
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

TEST_F(GlicViewTest, RequestMediaAccessPermission_NoWebview_ForwardsFromPwc) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      {features::kGlicNoWebview, pwc::mojom::features::kPrivilegedWebContents},
      {});

  content::RenderViewHostTestEnabler rvh_test_enabler;
  auto glic_view =
      std::make_unique<GlicView>(profile(), gfx::Size(800, 600), nullptr);

  auto policy_delegate = std::make_unique<pwc::FixedPwcPolicyDelegate>(
      std::vector<url::Origin>{
          url::Origin::Create(GURL("https://pwc-test.example.com"))},
      std::vector<url::Origin>{
          url::Origin::Create(GURL("https://pwc-test.example.com"))});
  std::unique_ptr<pwc::PrivilegedWebContents> pwc =
      pwc::PrivilegedWebContents::Create(pwc::PrivilegedComponent::kGlic,
                                         profile(), std::move(policy_delegate));
  ASSERT_TRUE(pwc);

  glic_view->SetWebContents(pwc->web_contents());
  ASSERT_EQ(pwc->embedder_delegate(), glic_view.get());

  content::MediaStreamRequest request(
      /*render_process_id=*/pwc->web_contents()
          ->GetPrimaryMainFrame()
          ->GetProcess()
          ->GetDeprecatedID(),
      /*render_frame_id=*/
      pwc->web_contents()->GetPrimaryMainFrame()->GetRoutingID(),
      /*page_request_id=*/0,
      /*security_origin=*/
      url::Origin::Create(GURL("https://pwc-test.example.com")),
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
  pwc->web_contents()->GetDelegate()->RequestMediaAccessPermission(
      pwc->web_contents(), request, BindResultToFuture(future));
  EXPECT_EQ(future.Get(),
            blink::mojom::MediaStreamRequestResult::INVALID_SECURITY_ORIGIN);
}

}  // namespace glic
