// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/indigo/indigo_image_replacement_manager.h"

#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
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
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/tokens/tokens.h"

namespace indigo {

namespace {

class MockImageReplacement : public blink::mojom::ImageReplacement {
 public:
  explicit MockImageReplacement(content::WebContents* web_contents)
      : web_contents_(web_contents) {}

  void StartReplacement(mojo::PendingRemote<blink::mojom::ImageReplacementHost>
                            host_remote) override {
    host_remote_.Bind(std::move(host_remote));

    // Create a subframe in the main frame.
    EXPECT_TRUE(
        content::ExecJs(web_contents_,
                        "const iframe = document.createElement('iframe');"
                        "document.body.appendChild(iframe);"));

    // Find the subframe RFH.
    // Note: ExecJs uses a frame associated interface, so it is guaranteed that
    // frame creation will happen before it returns.
    content::RenderFrameHostWrapper subframe(
        content::ChildFrameAt(web_contents_->GetPrimaryMainFrame(), 0));
    ASSERT_TRUE(subframe.get());

    host_remote_->ReplacementFrameAttached(subframe->GetFrameToken(),
                                           gfx::QuadF());

    start_replacement_future_.SetValue();
  }

  void RenderReplacement() override { render_replacement_future_.SetValue(); }

  void WaitForStartReplacement() {
    EXPECT_TRUE(start_replacement_future_.Wait());
  }

  void WaitForRenderReplacement() {
    EXPECT_TRUE(render_replacement_future_.Wait());
  }

 private:
  raw_ptr<content::WebContents> web_contents_;
  mojo::Remote<blink::mojom::ImageReplacementHost> host_remote_;
  base::test::TestFuture<void> start_replacement_future_;
  base::test::TestFuture<void> render_replacement_future_;
};

}  // namespace

class IndigoImageReplacementManagerBrowserTest : public InProcessBrowserTest {
 public:
  IndigoImageReplacementManagerBrowserTest() {
    feature_list_.InitAndEnableFeature(features::kIndigo);
  }

 protected:
  void SetUpOnMainThread() override {
    ASSERT_TRUE(embedded_test_server()->Start());
  }

 private:
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

  manager->RegisterImageReplacement(receiver.BindNewPipeAndPassRemote());
  mock_replacement.WaitForStartReplacement();

  GURL component_extension_url = extensions::Extension::GetResourceURL(
      extensions::Extension::GetBaseURLFromExtensionId(
          extension_misc::kIndigoExtensionId),
      "index.html");
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

}  // namespace indigo
