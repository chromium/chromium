// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/indigo/indigo_image_replacement_manager.h"

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/test/test_timeouts.h"
#include "chrome/browser/indigo/fake_api.h"
#include "chrome/browser/indigo/indigo_agent_host.h"
#include "chrome/browser/indigo/indigo_page_action_controller.h"
#include "chrome/browser/indigo/onboarding/indigo_onboarding_dialog.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/toasts/api/toast_id.h"
#include "chrome/browser/ui/toasts/toast_controller.h"
#include "chrome/browser/ui/toasts/toast_view.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/indigo/indigo.mojom.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/back_forward_cache_util.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "mojo/public/cpp/base/big_buffer.h"
#include "mojo/public/cpp/bindings/associated_receiver_set.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/common/tokens/tokens.h"

namespace indigo {

namespace {

class FakeIndigoAgent : public chrome::mojom::IndigoAgent {
 public:
  FakeIndigoAgent() = default;
  ~FakeIndigoAgent() override = default;

  void InjectScript(
      const std::string& script_content,
      const GURL& script_url,
      const url::Origin& origin,
      mojo::PendingAssociatedRemote<chrome::mojom::IndigoAgentHost> host,
      InjectScriptCallback callback) override {
    host_.Bind(std::move(host));
    std::move(callback).Run();
  }

  void Invoke(InvokeCallback callback) override {
    std::move(callback).Run();
    if (!invoke_called_future_.IsReady()) {
      invoke_called_future_.SetValue();
    }
    if (invoke_callback_) {
      std::move(invoke_callback_).Run();
    }
  }

  void Reset(ResetCallback callback) override {
    reset_called_ = true;
    if (keep_reset_pending_) {
      pending_reset_callback_ = std::move(callback);
    } else {
      std::move(callback).Run();
    }
    reset_called_future_.SetValue();
  }

  void Bind(mojo::ScopedInterfaceEndpointHandle handle) {
    receivers_.Add(this,
                   mojo::PendingAssociatedReceiver<chrome::mojom::IndigoAgent>(
                       std::move(handle)));
  }

  void WaitForInvoke() { EXPECT_TRUE(invoke_called_future_.Wait()); }

  void WaitForReset() { EXPECT_TRUE(reset_called_future_.Wait()); }

  bool reset_called() const { return reset_called_; }

  void set_keep_reset_pending(bool keep) { keep_reset_pending_ = keep; }

  void CompleteReset() {
    CHECK(pending_reset_callback_);
    std::move(pending_reset_callback_).Run();
  }

  chrome::mojom::IndigoAgentHost* host() { return host_.get(); }

  void set_invoke_callback(base::OnceClosure callback) {
    invoke_callback_ = std::move(callback);
  }

 private:
  mojo::AssociatedReceiverSet<chrome::mojom::IndigoAgent> receivers_;
  mojo::AssociatedRemote<chrome::mojom::IndigoAgentHost> host_;
  base::test::TestFuture<void> invoke_called_future_;
  base::test::TestFuture<void> reset_called_future_;
  base::OnceClosure invoke_callback_;
  bool reset_called_ = false;
  bool keep_reset_pending_ = false;
  ResetCallback pending_reset_callback_;
};

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

bool WaitUntilReplacementImageSrcMatches(content::RenderFrameHost* rfh,
                                         const std::string& expected_src) {
  constexpr std::string_view kScript = R"js(
    (async () => {
      const app = document.body.querySelector('indigo-image-replacement-app');
      if (!app) return false;
      const img = app.shadowRoot?.getElementById('image');
      if (!img) return false;
      if (img.src === $1) {
        return true;
      }
      return new Promise(resolve => {
        const observer = new MutationObserver(() => {
          if (img.src === $1) {
            observer.disconnect();
            resolve(true);
          }
        });
        observer.observe(img, { attributes: true, attributeFilter: ['src'] });
      });
    })();
  )js";
  return content::EvalJs(rfh, content::JsReplace(kScript, expected_src))
      .ExtractBool();
}

bool WaitForOverlayToHide(content::RenderFrameHost* rfh) {
  return content::EvalJs(rfh, R"js(
    (async () => {
      const app = document.body.querySelector('indigo-image-replacement-app');
      if (!app) return false;
      if (!app.showOverlay_) return true;
      return new Promise(resolve => {
        const check = () => {
          if (!app.showOverlay_) {
            resolve(true);
          } else {
            setTimeout(check, 50);
          }
        };
        check();
      });
    })();
  )js")
      .ExtractBool();
}

class MockImageReplacement : public blink::mojom::ImageReplacement {
 public:
  explicit MockImageReplacement(content::WebContents* web_contents,
                                size_t frame_index = 0)
      : web_contents_(web_contents), frame_index_(frame_index) {}

  void StartReplacement(
      mojo::PendingRemote<blink::mojom::ImageReplacementHost> host_remote,
      std::optional<int32_t> tracked_element_feature_id) override {
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
    host_remote_->ReplacementFrameAttached(raw_subframe->GetFrameToken(),
                                           std::move(image_data),
                                           base::Token::CreateRandom());

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
        features::kIndigo,
        {{features::kIndigoGenerateUrl.name, fake_api_.GetGenerateUrl().spec()},
         {features::kIndigoDeleteUrl.name, fake_api_.GetDeleteUrl().spec()}});

    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    script_path_ = temp_dir_.GetPath().AppendASCII("test_script.js");
    ASSERT_TRUE(base::WriteFile(script_path_, ""));

    InProcessBrowserTest::SetUp();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchPath("indigo-script", script_path_);
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
    fake_api_.StartAcceptingConnections(5, 5);
  }

  void SetUpBrowserContextKeyedServices(
      content::BrowserContext* context) override {
    InProcessBrowserTest::SetUpBrowserContextKeyedServices(context);
    IdentityTestEnvironmentProfileAdaptor::
        SetIdentityTestEnvironmentFactoriesOnBrowserContext(context);
  }

  std::unique_ptr<FakeIndigoAgent> SetupAndInvokeIndigoAgent(
      content::RenderFrameHost* rfh) {
    auto fake_agent = std::make_unique<FakeIndigoAgent>();
    rfh->GetRemoteAssociatedInterfaces()->OverrideBinderForTesting(
        chrome::mojom::IndigoAgent::Name_,
        base::BindRepeating(&FakeIndigoAgent::Bind,
                            base::Unretained(fake_agent.get())));
    IndigoAgentHost* host = IndigoAgentHost::GetOrCreateForPage(rfh->GetPage());
    EXPECT_TRUE(host->Invoke());
    fake_agent->WaitForInvoke();
    return fake_agent;
  }

  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor>
      identity_test_env_adaptor_;
  FakeApi fake_api_;
  base::test::ScopedFeatureList feature_list_;
  base::ScopedTempDir temp_dir_;
  base::FilePath script_path_;
};

class IndigoImageReplacementManagerBFCacheBrowserTest
    : public IndigoImageReplacementManagerBrowserTest {
 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    bfcache_feature_list_.InitWithFeaturesAndParameters(
        content::GetDefaultEnabledBackForwardCacheFeaturesForTesting(),
        content::GetDefaultDisabledBackForwardCacheFeaturesForTesting());

    IndigoImageReplacementManagerBrowserTest::SetUpCommandLine(command_line);
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    IndigoImageReplacementManagerBrowserTest::SetUpOnMainThread();
  }

 private:
  base::test::ScopedFeatureList bfcache_feature_list_;
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

  EXPECT_TRUE(
      WaitUntilReplacementImageSrcMatches(subframe.get(), success_url.spec()));
}

IN_PROC_BROWSER_TEST_F(IndigoImageReplacementManagerBrowserTest,
                       HandlesFailureFromGenerateRequest) {
  GURL test_url = embedded_test_server()->GetURL("/empty.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), test_url));

  tabs::TabInterface* tab = browser()->GetActiveTabInterface();
  content::WebContents* web_contents = tab->GetContents();
  content::RenderFrameHostWrapper main_rfh(web_contents->GetPrimaryMainFrame());

  // Set up IndigoAgent host.
  std::unique_ptr<FakeIndigoAgent> fake_agent =
      SetupAndInvokeIndigoAgent(main_rfh.get());

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

  // IndigoAgentHost::Reset should have been called.
  fake_agent->WaitForReset();

  // An error toast should be displayed.
  ToastController* const toast_controller =
      ToastController::MaybeGetForTabInterface(tab);
  ASSERT_TRUE(toast_controller && toast_controller->IsShowingToast());
  EXPECT_EQ(toast_controller->GetCurrentToastId(), ToastId::kIndigoInvokeError);
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

  // Set up IndigoAgent host.
  std::unique_ptr<FakeIndigoAgent> fake_agent =
      SetupAndInvokeIndigoAgent(main_rfh.get());

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

  // IndigoAgentHost::Reset should not have been called.
  EXPECT_FALSE(fake_agent->reset_called());
}

IN_PROC_BROWSER_TEST_F(IndigoImageReplacementManagerBrowserTest,
                       DisconnectsAllIfPrimaryDisconnectsBeforeGeneratedImage) {
  GURL test_url = embedded_test_server()->GetURL("/empty.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), test_url));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::RenderFrameHostWrapper main_rfh(web_contents->GetPrimaryMainFrame());

  // Set up IndigoAgent host.
  std::unique_ptr<FakeIndigoAgent> fake_agent =
      SetupAndInvokeIndigoAgent(main_rfh.get());

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

  // IndigoAgentHost::Reset should have been called.
  fake_agent->WaitForReset();

  // An error toast should be displayed.
  ToastController* const toast_controller =
      ToastController::MaybeGetForWebContents(web_contents);
  EXPECT_TRUE(toast_controller && toast_controller->IsShowingToast());
  EXPECT_EQ(toast_controller->GetCurrentToastId(), ToastId::kIndigoInvokeError);
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

  EXPECT_TRUE(
      WaitUntilReplacementImageSrcMatches(subframe2.get(), success_url.spec()));
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
  EXPECT_TRUE(
      WaitUntilReplacementImageSrcMatches(subframe.get(), success_url.spec()));
}

IN_PROC_BROWSER_TEST_F(IndigoImageReplacementManagerBrowserTest,
                       DeleteOriginalPhotoResetsReplacements) {
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

  auto* tab = tabs::TabInterface::GetFromContents(web_contents);
  ASSERT_TRUE(tab);
  auto* controller = IndigoPageActionController::From(tab);
  ASSERT_TRUE(controller);

  controller->OnDeleteOriginalPhoto(nullptr);

  fake_api_.WaitForDeleteRequest();
  fake_api_.SendDeleteSuccessResponse();

  mock_replacement.WaitForDisconnect();
}

IN_PROC_BROWSER_TEST_F(IndigoImageReplacementManagerBrowserTest,
                       ResetsReplacementsOnSameDocumentNavigation) {
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

  // Trigger a same-document navigation using pushState so that the path changes
  // and it is not treated as a fragment-only navigation.
  content::TestNavigationObserver nav_observer(web_contents);
  ASSERT_TRUE(content::ExecJs(web_contents,
                              "history.pushState({}, '', '/new-path.html');"));
  nav_observer.Wait();

  // Verify that the replacements are reset, which disconnects the receiver.
  mock_replacement.WaitForDisconnect();
}

IN_PROC_BROWSER_TEST_F(IndigoImageReplacementManagerBrowserTest,
                       IgnoresReplacementDuringReset) {
  GURL test_url = embedded_test_server()->GetURL("/empty.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), test_url));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::RenderFrameHostWrapper main_rfh(web_contents->GetPrimaryMainFrame());

  // 1) Setup IndigoAgent host and get the fake agent.
  std::unique_ptr<FakeIndigoAgent> fake_agent =
      SetupAndInvokeIndigoAgent(main_rfh.get());

  // Enable keeping reset pending in the fake agent.
  fake_agent->set_keep_reset_pending(true);

  // 2) Trigger a same-document navigation. This will start the reset process on
  // the browser side, incrementing `pending_reset_ack_count_` and calling
  // `FakeIndigoAgent::Reset()`.
  content::TestNavigationObserver nav_observer(web_contents);
  ASSERT_TRUE(content::ExecJs(web_contents,
                              "history.pushState({}, '', '/new-path.html');"));
  nav_observer.Wait();

  // 3) Now register a primary replacement via the agent host's Mojo interface,
  // while the reset is still pending. This explicitly happens before Reset is
  // processed in the renderer process to simulate a replacement being
  // registered while a reset is ongoing.
  MockImageReplacement mock_replacement(web_contents);
  mojo::Receiver<blink::mojom::ImageReplacement> receiver(&mock_replacement);
  auto pending_remote = receiver.BindNewPipeAndPassRemote();

  base::test::TestFuture<void> disconnect_future;
  receiver.set_disconnect_handler(disconnect_future.GetCallback());

  base::test::TestFuture<void> start_replacement_callback_future;
  fake_agent->host()->StartImageReplacement(
      std::move(pending_remote),
      /*is_primary=*/true, start_replacement_callback_future.GetCallback());

  // Wait for the Mojo call callback to complete.
  EXPECT_TRUE(start_replacement_callback_future.Wait());

  // Verify that FakeIndigoAgent::Reset was indeed called.
  fake_agent->WaitForReset();

  // Since a reset is pending, the IndigoAgentHost should ignore the
  // registration, resulting in the receiver being disconnected and
  // StartReplacement not being called.
  EXPECT_TRUE(disconnect_future.Wait());
  mock_replacement.ExpectStartReplacementToNotBeCalled();

  // 4) Complete the reset process.
  fake_agent->CompleteReset();

  // 5) Invoke the agent host again to start the new session.
  base::test::TestFuture<void> invoke_future;
  fake_agent->set_invoke_callback(invoke_future.GetCallback());
  IndigoAgentHost* host =
      IndigoAgentHost::GetOrCreateForPage(main_rfh->GetPage());
  EXPECT_TRUE(host->Invoke());
  EXPECT_TRUE(invoke_future.Wait());

  // Subsequent registrations should succeed.
  MockImageReplacement mock_replacement2(web_contents);
  mojo::Receiver<blink::mojom::ImageReplacement> receiver2(&mock_replacement2);
  base::test::TestFuture<void> start_replacement_callback_future2;
  fake_agent->host()->StartImageReplacement(
      receiver2.BindNewPipeAndPassRemote(),
      /*is_primary=*/true, start_replacement_callback_future2.GetCallback());

  EXPECT_TRUE(start_replacement_callback_future2.Wait());
  mock_replacement2.WaitForStartReplacement();
}

// Note: This tests currently verifies that a page with Indigo ImageReplacements
// is never put into BFCache because the replacement subframes load an extension
// page and we don't currently support putting extension frames in BFCache. If
// we do ever support this in the future, we should add logic to reset active
// ImageReplacements when the page is put into BFCache (we do not want the
// replacements to be kept around in this case).
IN_PROC_BROWSER_TEST_F(IndigoImageReplacementManagerBFCacheBrowserTest,
                       PageEmbeddingExtensionFrameEvictedFromBackForwardCache) {
  GURL url_a = embedded_test_server()->GetURL("a.com", "/empty.html");
  GURL url_b = embedded_test_server()->GetURL("b.com", "/empty.html");

  // 1) Navigate to A.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url_a));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::RenderFrameHostWrapper rfh_a(web_contents->GetPrimaryMainFrame());

  IndigoImageReplacementManager* manager =
      IndigoImageReplacementManager::GetOrCreateForPage(rfh_a->GetPage());
  ASSERT_TRUE(manager);

  MockImageReplacement mock_replacement(web_contents);
  mojo::Receiver<blink::mojom::ImageReplacement> receiver(&mock_replacement);

  manager->RegisterImageReplacement(receiver.BindNewPipeAndPassRemote(),
                                    /*is_primary=*/true);
  mock_replacement.WaitForStartReplacement();
  mock_replacement.WaitForRenderReplacement();

  // 2) Navigate away to B. Since page A embeds an extension subframe, it cannot
  // enter the back-forward cache and must be deleted.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url_b));

  ASSERT_TRUE(rfh_a.WaitUntilRenderFrameDeleted());
  mock_replacement.WaitForDisconnect();
}

IN_PROC_BROWSER_TEST_F(IndigoImageReplacementManagerBrowserTest,
                       RegenerateImageFlow) {
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

  // First generate request arrives and succeeds with success_url1.
  fake_api_.WaitForGenerateRequest(0);
  EXPECT_TRUE(fake_api_.RequestHasValidProductImage(kImageBytes, 0));
  GURL success_url1(
      "data:image/"
      "png;base64,iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR42mNk+"
      "M9QDwADhgGAWjR9awAAAABJRU5ErkJggg==");
  fake_api_.SendSuccessResponse(success_url1, 0);

  // Verify both frames show success_url1.
  content::RenderFrameHostWrapper subframe1(
      content::ChildFrameAt(main_rfh.get(), 0));
  ASSERT_TRUE(subframe1.get());
  EXPECT_TRUE(WaitUntilReplacementImageSrcMatches(subframe1.get(),
                                                  success_url1.spec()));

  content::RenderFrameHostWrapper subframe2(
      content::ChildFrameAt(main_rfh.get(), 1));
  ASSERT_TRUE(subframe2.get());
  EXPECT_TRUE(WaitUntilReplacementImageSrcMatches(subframe2.get(),
                                                  success_url1.spec()));

  // Call RegenerateImage() to trigger a new generation.
  EXPECT_TRUE(manager->RegenerateImage());

  // Second generate request arrives and succeeds with success_url2.
  fake_api_.WaitForGenerateRequest(1);
  EXPECT_TRUE(fake_api_.RequestHasValidProductImage(kImageBytes, 1));
  GURL success_url2(
      "data:image/"
      "png;base64,iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR42mNk"
      "YPjfDwAEhQGA6R1ykwAAAABJRU5ErkJggg==");
  fake_api_.SendSuccessResponse(success_url2, 1);

  // Verify both frames correctly update to show success_url2.
  EXPECT_TRUE(WaitUntilReplacementImageSrcMatches(subframe1.get(),
                                                  success_url2.spec()));
  EXPECT_TRUE(WaitUntilReplacementImageSrcMatches(subframe2.get(),
                                                  success_url2.spec()));
}

IN_PROC_BROWSER_TEST_F(IndigoImageReplacementManagerBrowserTest,
                       RegenerateInOneTabDoesNotAffectAnotherTab) {
  GURL test_url = embedded_test_server()->GetURL("/empty.html");

  // Setup Tab 1 with an image replacement.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), test_url));
  content::WebContents* web_contents1 =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::RenderFrameHostWrapper main_rfh1(
      web_contents1->GetPrimaryMainFrame());

  IndigoImageReplacementManager* manager1 =
      IndigoImageReplacementManager::GetOrCreateForPage(main_rfh1->GetPage());
  ASSERT_TRUE(manager1);

  MockImageReplacement mock_replacement1(web_contents1, 0);
  mojo::Receiver<blink::mojom::ImageReplacement> receiver1(&mock_replacement1);
  manager1->RegisterImageReplacement(receiver1.BindNewPipeAndPassRemote(),
                                     /*is_primary=*/true);
  mock_replacement1.WaitForStartReplacement();
  mock_replacement1.WaitForRenderReplacement();

  // Setup Tab 2 with an image replacement.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), test_url, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  content::WebContents* web_contents2 =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_NE(web_contents1, web_contents2);
  content::RenderFrameHostWrapper main_rfh2(
      web_contents2->GetPrimaryMainFrame());

  IndigoImageReplacementManager* manager2 =
      IndigoImageReplacementManager::GetOrCreateForPage(main_rfh2->GetPage());
  ASSERT_TRUE(manager2);

  MockImageReplacement mock_replacement2(web_contents2, 0);
  mojo::Receiver<blink::mojom::ImageReplacement> receiver2(&mock_replacement2);
  manager2->RegisterImageReplacement(receiver2.BindNewPipeAndPassRemote(),
                                     /*is_primary=*/true);
  mock_replacement2.WaitForStartReplacement();
  mock_replacement2.WaitForRenderReplacement();

  // Process first generate request (for Tab 1).
  fake_api_.WaitForGenerateRequest(0);
  EXPECT_TRUE(fake_api_.RequestHasValidProductImage(kImageBytes, 0));
  GURL success_url1(
      "data:image/"
      "png;base64,iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR42mNk+"
      "M9QDwADhgGAWjR9awAAAABJRU5ErkJggg==");
  fake_api_.SendSuccessResponse(success_url1, 0);

  content::RenderFrameHostWrapper subframe1(
      content::ChildFrameAt(main_rfh1.get(), 0));
  ASSERT_TRUE(subframe1.get());
  EXPECT_TRUE(WaitUntilReplacementImageSrcMatches(subframe1.get(),
                                                  success_url1.spec()));
  EXPECT_TRUE(WaitForOverlayToHide(subframe1.get()));

  // Process second generate request (for Tab 2).
  fake_api_.WaitForGenerateRequest(1);
  EXPECT_TRUE(fake_api_.RequestHasValidProductImage(kImageBytes, 1));
  GURL success_url2(
      "data:image/"
      "png;base64,iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR42mNk"
      "YPjfDwAEhQGA6R1ykwAAAABJRU5ErkJggg==");
  fake_api_.SendSuccessResponse(success_url2, 1);

  content::RenderFrameHostWrapper subframe2(
      content::ChildFrameAt(main_rfh2.get(), 0));
  ASSERT_TRUE(subframe2.get());
  EXPECT_TRUE(WaitUntilReplacementImageSrcMatches(subframe2.get(),
                                                  success_url2.spec()));
  EXPECT_TRUE(WaitForOverlayToHide(subframe2.get()));

  // Call Regenerate in Tab 1.
  EXPECT_TRUE(manager1->RegenerateImage());

  // Process third generate request (for Tab 1 regeneration).
  fake_api_.WaitForGenerateRequest(2);
  EXPECT_TRUE(fake_api_.RequestHasValidProductImage(kImageBytes, 2));
  GURL success_url3(
      "data:image/"
      "png;base64,iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR42mP8"
      "z8BQDwAEhQGA6R1ykwAAAABJRU5ErkJggg==");
  fake_api_.SendSuccessResponse(success_url3, 2);

  // Tab 1's replacement should have the regenerated image.
  EXPECT_TRUE(WaitUntilReplacementImageSrcMatches(subframe1.get(),
                                                  success_url3.spec()));

  // Tab 2's replacement's animation/overlay should not be active and its image
  // should be unchanged.
  EXPECT_EQ(false, content::EvalJs(subframe2.get(), R"js(
    (() => {
      const app = document.body.querySelector('indigo-image-replacement-app');
      return app?.showOverlay_ ?? false;
    })();
  )js"));
  EXPECT_TRUE(WaitUntilReplacementImageSrcMatches(subframe2.get(),
                                                  success_url2.spec()));
}

IN_PROC_BROWSER_TEST_F(IndigoImageReplacementManagerBrowserTest,
                       SecondRegenerateCancelsPreviousRequest) {
  GURL test_url = embedded_test_server()->GetURL("/empty.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), test_url));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::RenderFrameHostWrapper main_rfh(web_contents->GetPrimaryMainFrame());

  IndigoImageReplacementManager* manager =
      IndigoImageReplacementManager::GetOrCreateForPage(main_rfh->GetPage());
  ASSERT_TRUE(manager);

  // Register primary replacement.
  MockImageReplacement mock_replacement(web_contents);
  mojo::Receiver<blink::mojom::ImageReplacement> receiver(&mock_replacement);
  manager->RegisterImageReplacement(receiver.BindNewPipeAndPassRemote(),
                                    /*is_primary=*/true);
  mock_replacement.WaitForStartReplacement();
  mock_replacement.WaitForRenderReplacement();

  // First generate request arrives and succeeds.
  fake_api_.WaitForGenerateRequest(0);
  GURL success_url1(
      "data:image/"
      "png;base64,iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR42mNk+"
      "M9QDwADhgGAWjR9awAAAABJRU5ErkJggg==");
  fake_api_.SendSuccessResponse(success_url1, 0);

  content::RenderFrameHostWrapper subframe(
      content::ChildFrameAt(main_rfh.get(), 0));
  ASSERT_TRUE(subframe.get());
  EXPECT_TRUE(
      WaitUntilReplacementImageSrcMatches(subframe.get(), success_url1.spec()));

  // Call RegenerateImage() the first time.
  EXPECT_TRUE(manager->RegenerateImage());
  // Wait for the second generate request (Regenerate #1) to arrive.
  fake_api_.WaitForGenerateRequest(1);

  // Call RegenerateImage() the second time immediately (before responding to
  // the first regenerate).
  EXPECT_TRUE(manager->RegenerateImage());
  // Wait for the third generate request (Regenerate #2) to arrive.
  fake_api_.WaitForGenerateRequest(2);

  // Respond to the first regenerate request with success.
  // Since it was cancelled, its callback should be invalidated, so this
  // response should NOT change the image src from success_url1.
  GURL success_url2(
      "data:image/"
      "png;base64,iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAQAAAC1HAwCAAAAC0lEQVR42mNk"
      "YAAAAAYAAjCB0C8AAAAASUVORK5CYII=");
  fake_api_.SendSuccessResponse(success_url2, 1);

  // Wait a short bit and assert the image source is still success_url1 (it
  // has NOT updated to success_url2).
  base::RunLoop run_loop;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(), TestTimeouts::tiny_timeout());
  run_loop.Run();
  EXPECT_EQ(content::EvalJs(subframe.get(), R"js(
    document.body.querySelector(
      'indigo-image-replacement-app').shadowRoot.getElementById('image').src
  )js"),
            success_url1.spec());

  // Respond to the second regenerate request (index 2) with success.
  // This should successfully update the image.
  GURL success_url3(
      "data:image/"
      "png;base64,iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR42mNk"
      "YPjfDwAEhQGA6R1ykwAAAABJRU5ErkJggg==");
  fake_api_.SendSuccessResponse(success_url3, 2);

  // Verify that the image replacement is now showing the response from the
  // second regenerate request.
  EXPECT_TRUE(
      WaitUntilReplacementImageSrcMatches(subframe.get(), success_url3.spec()));
}

IN_PROC_BROWSER_TEST_F(IndigoImageReplacementManagerBrowserTest,
                       ReplacePhotoTriggersRegenerate) {
  GURL test_url = embedded_test_server()->GetURL("/empty.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), test_url));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::RenderFrameHostWrapper main_rfh(web_contents->GetPrimaryMainFrame());

  IndigoImageReplacementManager* manager =
      IndigoImageReplacementManager::GetOrCreateForPage(main_rfh->GetPage());
  ASSERT_TRUE(manager);

  // Register primary replacement.
  MockImageReplacement mock_replacement(web_contents, 0);
  mojo::Receiver<blink::mojom::ImageReplacement> receiver(&mock_replacement);
  manager->RegisterImageReplacement(receiver.BindNewPipeAndPassRemote(),
                                    /*is_primary=*/true);
  mock_replacement.WaitForStartReplacement();
  mock_replacement.WaitForRenderReplacement();

  // First generate request arrives and succeeds.
  fake_api_.WaitForGenerateRequest(0);
  GURL success_url1(
      "data:image/"
      "png;base64,iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR42mNk+"
      "M9QDwADhgGAWjR9awAAAABJRU5ErkJggg==");
  fake_api_.SendSuccessResponse(success_url1, 0);

  content::RenderFrameHostWrapper subframe(
      content::ChildFrameAt(main_rfh.get(), 0));
  ASSERT_TRUE(subframe.get());
  EXPECT_TRUE(
      WaitUntilReplacementImageSrcMatches(subframe.get(), success_url1.spec()));

  // Set up the onboarding dialog factory to intercept the onboarding dialog
  // and capture the callback.
  auto* tab = tabs::TabInterface::GetFromContents(web_contents);
  ASSERT_TRUE(tab);
  auto* controller = IndigoPageActionController::From(tab);
  ASSERT_TRUE(controller);

  base::OnceCallback<void(const OnboardingResult&)> onboarding_callback;
  IndigoPageActionController::TestApi(controller)
      .SetOnboardingDialogFactory(base::BindLambdaForTesting(
          [&](tabs::TabInterface& tab, const GURL& url,
              base::OnceCallback<void(const OnboardingResult&)> callback)
              -> std::unique_ptr<IndigoOnboardingDialog> {
            onboarding_callback = std::move(callback);
            return nullptr;
          }));

  // Trigger "Replace original photo" which should show the onboarding
  // dialog.
  controller->OnReplaceOriginalPhoto(nullptr);
  ASSERT_FALSE(onboarding_callback.is_null());

  // Simulate successful completion of the onboarding dialog.
  OnboardingResult result;
  result.acknowledge_chrome_disclaimer = true;
  std::move(onboarding_callback).Run(result);

  // Second generate request (regeneration) should arrive.
  fake_api_.WaitForGenerateRequest(1);
  GURL success_url2(
      "data:image/"
      "png;base64,iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR42mNk"
      "YPjfDwAEhQGA6R1ykwAAAABJRU5ErkJggg==");
  fake_api_.SendSuccessResponse(success_url2, 1);

  // Verify the frame correctly updates to show success_url2.
  EXPECT_TRUE(
      WaitUntilReplacementImageSrcMatches(subframe.get(), success_url2.spec()));
}

IN_PROC_BROWSER_TEST_F(IndigoImageReplacementManagerBrowserTest,
                       GetPrimaryTrackedElementIdAfterPrimaryDisconnect) {
  GURL test_url = embedded_test_server()->GetURL("/empty.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), test_url));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::RenderFrameHostWrapper main_rfh(web_contents->GetPrimaryMainFrame());

  IndigoImageReplacementManager* manager =
      IndigoImageReplacementManager::GetOrCreateForPage(main_rfh->GetPage());
  ASSERT_TRUE(manager);

  // Register primary replacement.
  MockImageReplacement mock_replacement(web_contents, 0);
  mojo::Receiver<blink::mojom::ImageReplacement> receiver(&mock_replacement);
  manager->RegisterImageReplacement(receiver.BindNewPipeAndPassRemote(),
                                    /*is_primary=*/true);
  mock_replacement.WaitForStartReplacement();
  mock_replacement.WaitForRenderReplacement();

  // Process generate request.
  fake_api_.WaitForGenerateRequest(0);
  GURL success_url(
      "data:image/"
      "png;base64,iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR42mNk+"
      "M9QDwADhgGAWjR9awAAAABJRU5ErkJggg==");
  fake_api_.SendSuccessResponse(success_url, 0);

  // Wait until replacement image is loaded in the subframe.
  content::RenderFrameHostWrapper subframe(
      content::ChildFrameAt(main_rfh.get(), 0));
  ASSERT_TRUE(subframe.get());
  EXPECT_TRUE(
      WaitUntilReplacementImageSrcMatches(subframe.get(), success_url.spec()));

  // Before disconnect, the primary tracked element ID should be valid.
  EXPECT_TRUE(manager->GetPrimaryTrackedElementId().has_value());

  // Set up disconnect handler on the receiver to wait for the browser-side
  // IndigoImageReplacement to be destroyed.
  base::test::TestFuture<void> receiver_disconnect_future;
  receiver.set_disconnect_handler(receiver_disconnect_future.GetCallback());

  // Disconnect the primary replacement (drops the ImageReplacementHost remote).
  mock_replacement.Disconnect();

  // Wait for the browser-side remote to be destroyed, which disconnects the
  // client-side receiver.
  EXPECT_TRUE(receiver_disconnect_future.Wait());

  // Verify that calling GetPrimaryTrackedElementId() does not crash and returns
  // std::nullopt.
  EXPECT_FALSE(manager->GetPrimaryTrackedElementId().has_value());
}

}  // namespace indigo
