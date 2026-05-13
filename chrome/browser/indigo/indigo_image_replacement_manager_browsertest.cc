// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/indigo/indigo_image_replacement_manager.h"

#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/indigo/fake_api.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "mojo/public/cpp/base/big_buffer.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/tokens/tokens.h"

namespace indigo {

namespace {

// 1x1 red pixel in image/webp.
const std::vector<uint8_t> kImageBytes = {
    0x52, 0x49, 0x46, 0x46, 0x3c, 0x00, 0x00, 0x00, 0x57, 0x45, 0x42, 0x50,
    0x56, 0x50, 0x38, 0x20, 0x30, 0x00, 0x00, 0x00, 0xd0, 0x01, 0x00, 0x9d,
    0x01, 0x2a, 0x01, 0x00, 0x01, 0x00, 0x02, 0x00, 0x34, 0x25, 0xa0, 0x02,
    0x74, 0xba, 0x01, 0xf8, 0x00, 0x03, 0xb0, 0x00, 0xfe, 0xf0, 0xc4, 0x0b,
    0xff, 0x20, 0xb9, 0x61, 0x75, 0xc8, 0xd7, 0xff, 0x20, 0x3f, 0xe4, 0x07,
    0xfc, 0x80, 0xff, 0xf8, 0xf2, 0x00, 0x00, 0x00};

GURL GetComponentExtensionUrl() {
  return extensions::Extension::GetResourceURL(
      extensions::Extension::GetBaseURLFromExtensionId(
          extension_misc::kIndigoExtensionId),
      "index.html");
}

std::string WaitUntilReplacementImageSrcIsSet(content::RenderFrameHost* rfh) {
  return content::EvalJs(rfh, R"js(
    (async () => {
      const app = document.body.querySelector('indigo-image-replacement-app');
      if (!app) return 'no app';
      const img = app.$.image;
      if (img.src.startsWith('data:')) {
        return img.src;
      }
      return new Promise(resolve => {
        const observer = new MutationObserver(() => {
          if (img.src.startsWith('data:')) {
            observer.disconnect();
            resolve(img.src);
          }
        });
        observer.observe(img, { attributes: true, attributeFilter: ['src'] });
      });
    })();
  )js")
      .ExtractString();
}

class MockImageReplacement : public blink::mojom::ImageReplacement {
 public:
  explicit MockImageReplacement(content::WebContents* web_contents,
                                size_t frame_index = 0)
      : web_contents_(web_contents), frame_index_(frame_index) {}

  void StartReplacement(mojo::PendingRemote<blink::mojom::ImageReplacementHost>
                            host_remote) override {
    host_remote_.Bind(std::move(host_remote));
    host_remote_.set_disconnect_handler(disconnect_future_.GetCallback());

    // Create a subframe in the main frame.
    EXPECT_TRUE(
        content::ExecJs(web_contents_,
                        "const iframe = document.createElement('iframe');"
                        "document.body.appendChild(iframe);"));

    // Find the subframe RFH.
    content::RenderFrameHost* raw_subframe = content::ChildFrameAt(
        web_contents_->GetPrimaryMainFrame(), frame_index_);
    ASSERT_TRUE(raw_subframe);
    frame_tree_node_id_ = raw_subframe->GetFrameTreeNodeId();

    blink::mojom::ImageDataPtr image_data = blink::mojom::ImageData::New();
    image_data->webp_bytes = mojo_base::BigBuffer(kImageBytes);
    host_remote_->ReplacementFrameAttached(
        raw_subframe->GetFrameToken(),
        gfx::QuadF(gfx::RectF(0.f, 0.f, 100.f, 100.f)), std::move(image_data));

    start_replacement_future_.SetValue();
  }

  void RenderReplacement() override {
    content::RenderFrameHost* rfh =
        web_contents_->UnsafeFindFrameByFrameTreeNodeId(frame_tree_node_id_);
    ASSERT_TRUE(rfh);

    // Ensure that the subframe has finished loading the component extension
    // before calling RenderReplacement().
    GURL component_extension_url = GetComponentExtensionUrl();
    EXPECT_EQ(rfh->GetLastCommittedURL(), component_extension_url);
    EXPECT_EQ("complete", content::EvalJs(rfh, "document.readyState"));

    render_replacement_future_.SetValue();
  }

  void WaitForStartReplacement() {
    EXPECT_TRUE(start_replacement_future_.Wait());
  }

  void WaitForRenderReplacement() {
    EXPECT_TRUE(render_replacement_future_.Wait());
  }

  void WaitForDisconnect() { EXPECT_TRUE(disconnect_future_.Wait()); }

  void Disconnect() { host_remote_.reset(); }

  void ExpectStartReplacementToNotBeCalled() {
    EXPECT_FALSE(start_replacement_future_.IsReady());
  }

 private:
  raw_ptr<content::WebContents> web_contents_;
  const size_t frame_index_;
  mojo::Remote<blink::mojom::ImageReplacementHost> host_remote_;
  base::test::TestFuture<void> start_replacement_future_;
  base::test::TestFuture<void> render_replacement_future_;
  base::test::TestFuture<void> disconnect_future_;
  content::FrameTreeNodeId frame_tree_node_id_;
};

}  // namespace

class IndigoImageReplacementManagerBrowserTest : public InProcessBrowserTest {
 protected:
  void SetUp() override {
    ASSERT_TRUE(fake_api_.InitializeAndListen());

    feature_list_.InitAndEnableFeatureWithParameters(
        features::kIndigo, {{features::kIndigoGenerateUrl.name,
                             fake_api_.GetGenerateUrl().spec()}});

    InProcessBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    ASSERT_TRUE(embedded_test_server()->Start());
    identity_test_env_adaptor_ =
        std::make_unique<IdentityTestEnvironmentProfileAdaptor>(
            browser()->profile());
    identity_test_env_adaptor_->identity_test_env()
        ->SetAutomaticIssueOfAccessTokens(true);
    identity_test_env_adaptor_->identity_test_env()
        ->MakePrimaryAccountAvailable("user@gmail.com",
                                      signin::ConsentLevel::kSignin);
    fake_api_.StartAcceptingConnections(5);
  }

  void SetUpBrowserContextKeyedServices(
      content::BrowserContext* context) override {
    InProcessBrowserTest::SetUpBrowserContextKeyedServices(context);
    IdentityTestEnvironmentProfileAdaptor::
        SetIdentityTestEnvironmentFactoriesOnBrowserContext(context);
  }

  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor>
      identity_test_env_adaptor_;
  FakeApi fake_api_;
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(IndigoImageReplacementManagerBrowserTest,
                       RegistersAndNavigatesToComponentExtension) {
  GURL test_url = embedded_test_server()->GetURL("/empty.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), test_url));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::RenderFrameHostWrapper main_rfh(web_contents->GetPrimaryMainFrame());

  IndigoImageReplacementManager* manager =
      IndigoImageReplacementManager::GetOrCreateForPage(main_rfh->GetPage());
  ASSERT_TRUE(manager);

  MockImageReplacement mock_replacement(web_contents);
  mojo::Receiver<blink::mojom::ImageReplacement> receiver(&mock_replacement);

  manager->RegisterImageReplacement(receiver.BindNewPipeAndPassRemote(),
                                    /*is_primary=*/true);
  mock_replacement.WaitForStartReplacement();

  GURL component_extension_url = GetComponentExtensionUrl();
  // Setup observer for the subframe navigation to the Indigo Component
  // Extension URL.
  content::TestNavigationObserver navigation_observer(component_extension_url);
  navigation_observer.WatchExistingWebContents();
  navigation_observer.Wait();

  // Find the subframe and verify its URL.
  content::RenderFrameHostWrapper subframe(
      content::ChildFrameAt(main_rfh.get(), 0));
  ASSERT_TRUE(subframe.get());
  EXPECT_EQ(subframe->GetLastCommittedURL(), component_extension_url);
  EXPECT_FALSE(subframe->IsErrorDocument());
  EXPECT_EQ("Indigo", content::EvalJs(subframe.get(), "document.title"));
  EXPECT_TRUE(content::EvalJs(subframe.get(), R"js(
    (() => {
      const shadowRoot = document.body.querySelector(
        'indigo-image-replacement-app').shadowRoot;
      return !!(shadowRoot && shadowRoot.children.length);
    })();
  )js")
                  .ExtractBool());

  mock_replacement.WaitForRenderReplacement();
}

IN_PROC_BROWSER_TEST_F(IndigoImageReplacementManagerBrowserTest,
                       SendsGenerateRequest) {
  GURL test_url = embedded_test_server()->GetURL("/empty.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), test_url));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::RenderFrameHostWrapper main_rfh(web_contents->GetPrimaryMainFrame());

  IndigoImageReplacementManager* manager =
      IndigoImageReplacementManager::GetOrCreateForPage(main_rfh->GetPage());
  ASSERT_TRUE(manager);

  MockImageReplacement mock_replacement(web_contents);
  mojo::Receiver<blink::mojom::ImageReplacement> receiver(&mock_replacement);

  manager->RegisterImageReplacement(receiver.BindNewPipeAndPassRemote(),
                                    /*is_primary=*/true);
  mock_replacement.WaitForStartReplacement();
  mock_replacement.WaitForRenderReplacement();

  fake_api_.WaitForGenerateRequest();
  EXPECT_TRUE(fake_api_.RequestHasValidProductImage(kImageBytes));
  fake_api_.SendSuccessResponse(GURL(
      "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAAD"
      "UlEQVR42mNk+M9QDwADhgGAWjR9awAAAABJRU5ErkJggg=="));
}

IN_PROC_BROWSER_TEST_F(IndigoImageReplacementManagerBrowserTest,
                       SendsImageBytesToComponentExtension) {
  GURL test_url = embedded_test_server()->GetURL("/empty.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), test_url));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::RenderFrameHostWrapper main_rfh(web_contents->GetPrimaryMainFrame());

  IndigoImageReplacementManager* manager =
      IndigoImageReplacementManager::GetOrCreateForPage(main_rfh->GetPage());
  ASSERT_TRUE(manager);

  MockImageReplacement mock_replacement(web_contents);
  mojo::Receiver<blink::mojom::ImageReplacement> receiver(&mock_replacement);

  manager->RegisterImageReplacement(receiver.BindNewPipeAndPassRemote(),
                                    /*is_primary=*/true);
  mock_replacement.WaitForStartReplacement();

  GURL component_extension_url = GetComponentExtensionUrl();
  content::TestNavigationObserver navigation_observer(component_extension_url);
  navigation_observer.WatchExistingWebContents();
  navigation_observer.Wait();

  content::RenderFrameHostWrapper subframe(
      content::ChildFrameAt(main_rfh.get(), 0));
  mock_replacement.WaitForRenderReplacement();

  auto result = content::EvalJs(subframe.get(), R"js(
    (async () => {
      const app = document.body.querySelector('indigo-image-replacement-app');
      if (!app || !app.$.image.src) return [];
      const res = await fetch(app.$.image.src);
      const blob = await res.blob();
      const arrayBuffer = await blob.arrayBuffer();
      return Array.from(new Uint8Array(arrayBuffer));
    })();
  )js");
  const auto& result_bytes_list = result.ExtractList();
  std::vector<uint8_t> actual_bytes;
  actual_bytes.reserve(result_bytes_list.size());
  for (const auto& value : result_bytes_list) {
    actual_bytes.push_back(static_cast<uint8_t>(value.GetInt()));
  }
  EXPECT_EQ(actual_bytes, kImageBytes);
}

IN_PROC_BROWSER_TEST_F(IndigoImageReplacementManagerBrowserTest,
                       SetsReplacementImageUrlInComponentExtension) {
  GURL test_url = embedded_test_server()->GetURL("/empty.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), test_url));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::RenderFrameHostWrapper main_rfh(web_contents->GetPrimaryMainFrame());

  IndigoImageReplacementManager* manager =
      IndigoImageReplacementManager::GetOrCreateForPage(main_rfh->GetPage());
  ASSERT_TRUE(manager);

  MockImageReplacement mock_replacement(web_contents);
  mojo::Receiver<blink::mojom::ImageReplacement> receiver(&mock_replacement);

  manager->RegisterImageReplacement(receiver.BindNewPipeAndPassRemote(),
                                    /*is_primary=*/true);
  mock_replacement.WaitForStartReplacement();
  mock_replacement.WaitForRenderReplacement();

  fake_api_.WaitForGenerateRequest();
  EXPECT_TRUE(fake_api_.RequestHasValidProductImage(kImageBytes));
  GURL success_url(
      "data:image/"
      "png;base64,iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR42mNk+"
      "M9QDwADhgGAWjR9awAAAABJRU5ErkJggg==");
  fake_api_.SendSuccessResponse(success_url);

  content::RenderFrameHostWrapper subframe(
      content::ChildFrameAt(main_rfh.get(), 0));
  ASSERT_TRUE(subframe.get());

  std::string actual_src = WaitUntilReplacementImageSrcIsSet(subframe.get());

  EXPECT_EQ(actual_src, success_url.spec());
}

IN_PROC_BROWSER_TEST_F(IndigoImageReplacementManagerBrowserTest,
                       HandlesFailureFromGenerateRequest) {
  GURL test_url = embedded_test_server()->GetURL("/empty.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), test_url));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::RenderFrameHostWrapper main_rfh(web_contents->GetPrimaryMainFrame());

  IndigoImageReplacementManager* manager =
      IndigoImageReplacementManager::GetOrCreateForPage(main_rfh->GetPage());
  ASSERT_TRUE(manager);

  MockImageReplacement mock_replacement(web_contents);
  mojo::Receiver<blink::mojom::ImageReplacement> receiver(&mock_replacement);

  manager->RegisterImageReplacement(receiver.BindNewPipeAndPassRemote(),
                                    /*is_primary=*/true);
  mock_replacement.WaitForStartReplacement();
  mock_replacement.WaitForRenderReplacement();

  fake_api_.WaitForGenerateRequest();
  fake_api_.SendErrorResponse();

  mock_replacement.WaitForDisconnect();
}

IN_PROC_BROWSER_TEST_F(IndigoImageReplacementManagerBrowserTest,
                       IgnoresNonPrimaryReplacementBeforePrimaryIsRegistered) {
  GURL test_url = embedded_test_server()->GetURL("/empty.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), test_url));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::RenderFrameHostWrapper main_rfh(web_contents->GetPrimaryMainFrame());

  IndigoImageReplacementManager* manager =
      IndigoImageReplacementManager::GetOrCreateForPage(main_rfh->GetPage());
  ASSERT_TRUE(manager);

  MockImageReplacement mock_replacement(web_contents, 0);
  mojo::Receiver<blink::mojom::ImageReplacement> receiver(&mock_replacement);
  base::test::TestFuture<void> disconnect_future;
  auto pending_remote = receiver.BindNewPipeAndPassRemote();
  receiver.set_disconnect_handler(disconnect_future.GetCallback());

  // A non-primary replacement registered before any primary should be ignored
  // and dropped.
  manager->RegisterImageReplacement(std::move(pending_remote),
                                    /*is_primary=*/false);
  ASSERT_TRUE(disconnect_future.Wait());
  mock_replacement.ExpectStartReplacementToNotBeCalled();
}

IN_PROC_BROWSER_TEST_F(IndigoImageReplacementManagerBrowserTest,
                       ResetsAllReplacementsOnNewPrimaryRegistration) {
  GURL test_url = embedded_test_server()->GetURL("/empty.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), test_url));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::RenderFrameHostWrapper main_rfh(web_contents->GetPrimaryMainFrame());

  IndigoImageReplacementManager* manager =
      IndigoImageReplacementManager::GetOrCreateForPage(main_rfh->GetPage());
  ASSERT_TRUE(manager);

  // Register first primary replacement.
  MockImageReplacement mock_replacement1(web_contents, 0);
  mojo::Receiver<blink::mojom::ImageReplacement> receiver1(&mock_replacement1);
  manager->RegisterImageReplacement(receiver1.BindNewPipeAndPassRemote(),
                                    /*is_primary=*/true);
  mock_replacement1.WaitForStartReplacement();

  // Register second primary replacement - should reset the first one.
  MockImageReplacement mock_replacement2(web_contents, 1);
  mojo::Receiver<blink::mojom::ImageReplacement> receiver2(&mock_replacement2);
  manager->RegisterImageReplacement(receiver2.BindNewPipeAndPassRemote(),
                                    /*is_primary=*/true);

  mock_replacement1.WaitForDisconnect();
  mock_replacement2.WaitForStartReplacement();
}

IN_PROC_BROWSER_TEST_F(IndigoImageReplacementManagerBrowserTest,
                       DisconnectsAllIfPrimaryDisconnectsBeforeGeneratedImage) {
  GURL test_url = embedded_test_server()->GetURL("/empty.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), test_url));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::RenderFrameHostWrapper main_rfh(web_contents->GetPrimaryMainFrame());

  IndigoImageReplacementManager* manager =
      IndigoImageReplacementManager::GetOrCreateForPage(main_rfh->GetPage());
  ASSERT_TRUE(manager);

  // Register primary replacement.
  MockImageReplacement mock_replacement1(web_contents, 0);
  mojo::Receiver<blink::mojom::ImageReplacement> receiver1(&mock_replacement1);
  manager->RegisterImageReplacement(receiver1.BindNewPipeAndPassRemote(),
                                    /*is_primary=*/true);
  mock_replacement1.WaitForStartReplacement();

  // Register non-primary replacement.
  MockImageReplacement mock_replacement2(web_contents, 1);
  mojo::Receiver<blink::mojom::ImageReplacement> receiver2(&mock_replacement2);
  manager->RegisterImageReplacement(receiver2.BindNewPipeAndPassRemote(),
                                    /*is_primary=*/false);
  mock_replacement2.WaitForStartReplacement();

  // Disconnect primary before generated image is ready.
  mock_replacement1.Disconnect();

  // Second non-primary replacement should be disconnected as well.
  mock_replacement2.WaitForDisconnect();
}

IN_PROC_BROWSER_TEST_F(IndigoImageReplacementManagerBrowserTest,
                       SharesGeneratedImageUrlWithNonPrimaryReplacement) {
  GURL test_url = embedded_test_server()->GetURL("/empty.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), test_url));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::RenderFrameHostWrapper main_rfh(web_contents->GetPrimaryMainFrame());

  IndigoImageReplacementManager* manager =
      IndigoImageReplacementManager::GetOrCreateForPage(main_rfh->GetPage());
  ASSERT_TRUE(manager);

  // Register primary replacement.
  MockImageReplacement mock_replacement1(web_contents, 0);
  mojo::Receiver<blink::mojom::ImageReplacement> receiver1(&mock_replacement1);
  manager->RegisterImageReplacement(receiver1.BindNewPipeAndPassRemote(),
                                    /*is_primary=*/true);
  mock_replacement1.WaitForStartReplacement();

  // Register non-primary replacement.
  MockImageReplacement mock_replacement2(web_contents, 1);
  mojo::Receiver<blink::mojom::ImageReplacement> receiver2(&mock_replacement2);
  manager->RegisterImageReplacement(receiver2.BindNewPipeAndPassRemote(),
                                    /*is_primary=*/false);
  mock_replacement2.WaitForStartReplacement();

  mock_replacement1.WaitForRenderReplacement();
  mock_replacement2.WaitForRenderReplacement();

  fake_api_.WaitForGenerateRequest();
  GURL success_url(
      "data:image/"
      "png;base64,iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR42mNk+"
      "M9QDwADhgGAWjR9awAAAABJRU5ErkJggg==");
  fake_api_.SendSuccessResponse(success_url);

  // Verify the second non-primary subframe also receives the identical
  // generated image URL.
  content::RenderFrameHostWrapper subframe2(
      content::ChildFrameAt(main_rfh.get(), 1));
  ASSERT_TRUE(subframe2.get());

  std::string actual_src = WaitUntilReplacementImageSrcIsSet(subframe2.get());

  EXPECT_EQ(actual_src, success_url.spec());
}

IN_PROC_BROWSER_TEST_F(IndigoImageReplacementManagerBrowserTest,
                       HandlesDelayedResponseAfterNewPrimaryIsRegistered) {
  GURL test_url = embedded_test_server()->GetURL("/empty.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), test_url));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::RenderFrameHostWrapper main_rfh(web_contents->GetPrimaryMainFrame());

  IndigoImageReplacementManager* manager =
      IndigoImageReplacementManager::GetOrCreateForPage(main_rfh->GetPage());
  ASSERT_TRUE(manager);

  // Create the first image replacement (primary).
  MockImageReplacement mock_replacement1(web_contents, 0);
  mojo::Receiver<blink::mojom::ImageReplacement> receiver1(&mock_replacement1);
  manager->RegisterImageReplacement(receiver1.BindNewPipeAndPassRemote(),
                                    /*is_primary=*/true);
  mock_replacement1.WaitForStartReplacement();
  mock_replacement1.WaitForRenderReplacement();

  // Wait for the first generate request to arrive.
  fake_api_.WaitForGenerateRequest(0);

  // Create a second primary image replacement. This should automatically reset
  // the first one.
  MockImageReplacement mock_replacement2(web_contents, 1);
  mojo::Receiver<blink::mojom::ImageReplacement> receiver2(&mock_replacement2);
  manager->RegisterImageReplacement(receiver2.BindNewPipeAndPassRemote(),
                                    /*is_primary=*/true);
  mock_replacement2.WaitForStartReplacement();
  mock_replacement2.WaitForRenderReplacement();

  // Wait for first image replacement to be reset.
  mock_replacement1.WaitForDisconnect();

  // Wait for the second generate request to arrive.
  fake_api_.WaitForGenerateRequest(1);

  // First generate request fails. This should not reset the second image
  // replacement.
  fake_api_.SendErrorResponse(0);

  // Second generate request succeeds.
  GURL success_url(
      "data:image/"
      "png;base64,iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR42mNk+"
      "M9QDwADhgGAWjR9awAAAABJRU5ErkJggg==");
  fake_api_.SendSuccessResponse(success_url, 1);

  // Second image replacement should successfully receive generated image URL.
  content::RenderFrameHostWrapper subframe(
      content::ChildFrameAt(main_rfh.get(), 1));
  ASSERT_TRUE(subframe.get());
  std::string actual_src = WaitUntilReplacementImageSrcIsSet(subframe.get());
  EXPECT_EQ(actual_src, success_url.spec());
}

}  // namespace indigo
