// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/chrome_content_browser_client.h"
#include "chrome/browser/translate/translate_frame_binder.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_client.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/fenced_frame_test_util.h"
#include "content/public/test/prerender_test_util.h"
#include "mojo/public/cpp/bindings/binder_map.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace translate {

namespace {

class TestTranslateDriverBindingContentBrowserClient
    : public ChromeContentBrowserClient {
 public:
  TestTranslateDriverBindingContentBrowserClient() = default;
  ~TestTranslateDriverBindingContentBrowserClient() override = default;

  void RegisterBrowserInterfaceBindersForFrame(
      content::RenderFrameHost* render_frame_host,
      mojo::BinderMapWithContext<content::RenderFrameHost*>* map) override {
    ChromeContentBrowserClient::RegisterBrowserInterfaceBindersForFrame(
        render_frame_host, map);
    // Override binding for translate::mojom::ContentTranslateDriver.
    map->Add<translate::mojom::ContentTranslateDriver>(base::BindRepeating(
        &TestTranslateDriverBindingContentBrowserClient::BindTest,
        weak_factory_.GetWeakPtr()));
  }

  void BindTest(content::RenderFrameHost* render_frame_host,
                mojo::PendingReceiver<translate::mojom::ContentTranslateDriver>
                    receiver) {
    // It should be called on the active page.
    ASSERT_EQ(render_frame_host->GetLifecycleState(),
              content::RenderFrameHost::LifecycleState::kActive);

    translate::BindContentTranslateDriver(render_frame_host,
                                          std::move(receiver));

    // Check if |render_frame_host| is a primary main frame again here even
    // though BindContentTranslateDriver method already checks it, since it's
    // difficult to know if the method succeeds to bind the given receiver.
    if (render_frame_host->IsInPrimaryMainFrame())
      render_frame_binding_map_[render_frame_host] = true;

    if (quit_on_binding_)
      std::move(quit_on_binding_).Run();
  }

  bool WaitForBinding(content::RenderFrameHost* render_frame_host,
                      base::OnceClosure callback) {
    if (IsBound(render_frame_host))
      return false;
    quit_on_binding_ = std::move(callback);
    return true;
  }

  bool IsBound(content::RenderFrameHost* render_frame_host) {
    return render_frame_binding_map_[render_frame_host];
  }

 private:
  base::OnceClosure quit_on_binding_;
  std::map<content::RenderFrameHost*, bool> render_frame_binding_map_;
  base::WeakPtrFactory<TestTranslateDriverBindingContentBrowserClient>
      weak_factory_{this};
};

}  // namespace

class TranslateFrameBinderBrowserTest : public InProcessBrowserTest {
 public:
  TranslateFrameBinderBrowserTest() = default;
  ~TranslateFrameBinderBrowserTest() override = default;

  void SetUpOnMainThread() override {
    ASSERT_TRUE(test_server_handle_ =
                    embedded_test_server()->StartAndReturnHandle());
  }

  content::WebContents* web_contents() const {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

 private:
  net::test_server::EmbeddedTestServerHandle test_server_handle_;
};

class TranslateFrameBinderPrerenderBrowserTest
    : public TranslateFrameBinderBrowserTest {
 public:
  TranslateFrameBinderPrerenderBrowserTest()
      : prerender_helper_(base::BindRepeating(
            &TranslateFrameBinderPrerenderBrowserTest::web_contents,
            base::Unretained(this))) {}
  ~TranslateFrameBinderPrerenderBrowserTest() override = default;

  void SetUp() override {
    prerender_helper_.RegisterServerRequestMonitor(embedded_test_server());
    TranslateFrameBinderBrowserTest::SetUp();
  }

  content::test::PrerenderTestHelper* prerender_helper() {
    return &prerender_helper_;
  }

 private:
  content::test::PrerenderTestHelper prerender_helper_;
};

// Tests that mojom::ContentTranslateDriver binding is deferred in prerendering
// and it's safe to access WebContents in the binding function since it's
// executed after prerendering activation.
IN_PROC_BROWSER_TEST_F(TranslateFrameBinderPrerenderBrowserTest,
                       NotBindingInPrerendering) {
  TestTranslateDriverBindingContentBrowserClient test_browser_client;
  auto* old_browser_client = SetBrowserClientForTesting(&test_browser_client);

  const GURL kInitialUrl = embedded_test_server()->GetURL("/empty.html");
  const GURL kPrerenderingUrl = embedded_test_server()->GetURL("/title1.html");

  // Navigate to an initial page.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), kInitialUrl));

  content::FrameTreeNodeId host_id =
      prerender_helper()->AddPrerender(kPrerenderingUrl);
  content::RenderFrameHost* prerendered_frame_host =
      prerender_helper()->GetPrerenderedMainFrameHost(host_id);
  content::test::PrerenderHostObserver host_observer(*web_contents(), host_id);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(test_browser_client.IsBound(prerendered_frame_host));

  // Activate the prerendered page.
  prerender_helper()->NavigatePrimaryPage(kPrerenderingUrl);
  EXPECT_TRUE(host_observer.was_activated());

  base::RunLoop run_loop;
  if (test_browser_client.WaitForBinding(prerendered_frame_host,
                                         run_loop.QuitClosure())) {
    run_loop.Run();
  }
  EXPECT_TRUE(test_browser_client.IsBound(prerendered_frame_host));

  content::SetBrowserClientForTesting(old_browser_client);
}

class TranslateFrameBinderFencedFrameBrowserTest
    : public TranslateFrameBinderBrowserTest {
 public:
  TranslateFrameBinderFencedFrameBrowserTest() = default;
  ~TranslateFrameBinderFencedFrameBrowserTest() override = default;
  TranslateFrameBinderFencedFrameBrowserTest(
      const TranslateFrameBinderFencedFrameBrowserTest&) = delete;

  TranslateFrameBinderFencedFrameBrowserTest& operator=(
      const TranslateFrameBinderFencedFrameBrowserTest&) = delete;

  content::test::FencedFrameTestHelper& fenced_frame_test_helper() {
    return fenced_frame_helper_;
  }

 private:
  content::test::FencedFrameTestHelper fenced_frame_helper_;
};

// TODO(crbug.com/40911156): Flaky on multiple platforms.
IN_PROC_BROWSER_TEST_F(TranslateFrameBinderFencedFrameBrowserTest,
                       DISABLED_NotBindingInFencedFrame) {
  TestTranslateDriverBindingContentBrowserClient test_browser_client;
  auto* old_browser_client = SetBrowserClientForTesting(&test_browser_client);

  // Navigate to an initial page.
  const GURL initial_url = embedded_test_server()->GetURL("/empty.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), initial_url));

  // Create a fenced frame.
  const GURL fenced_frame_url =
      embedded_test_server()->GetURL("/fenced_frames/title1.html");
  content::RenderFrameHost* fenced_frame_host =
      fenced_frame_test_helper().CreateFencedFrame(
          web_contents()->GetPrimaryMainFrame(), fenced_frame_url);
  base::RunLoop run_loop;
  if (test_browser_client.WaitForBinding(fenced_frame_host,
                                         run_loop.QuitClosure())) {
    run_loop.Run();
  }
  // Fenced frame should not be bound.
  EXPECT_FALSE(test_browser_client.IsBound(fenced_frame_host));

  content::SetBrowserClientForTesting(old_browser_client);
}

}  // namespace translate
