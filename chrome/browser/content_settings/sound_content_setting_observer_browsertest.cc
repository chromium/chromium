// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/content_settings/sound_content_setting_observer.h"

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/content_settings/browser/page_specific_content_settings.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/fenced_frame_test_util.h"
#include "content/public/test/prerender_test_util.h"
#include "content/public/test/test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/mojom/autoplay/autoplay.mojom-test-utils.h"
#include "third_party/blink/public/mojom/autoplay/autoplay.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace {

const char* kMediaTestDataPath = "media/test/data";

// Observes a WebContents and waits until the audio state is updated.
class TestAudioStateObserver : public content::WebContentsObserver {
 public:
  TestAudioStateObserver(content::WebContents* web_contents,
                         base::OnceClosure quit_closure,
                         bool is_audible = false)
      : content::WebContentsObserver(web_contents),
        quit_closure_(std::move(quit_closure)) {
    if (!is_audible)
      DCHECK(!web_contents->IsCurrentlyAudible());
  }
  ~TestAudioStateObserver() override = default;

  // WebContentsObserver:
  void OnAudioStateChanged(bool audible) override {
    if (quit_closure_)
      std::move(quit_closure_).Run();
    audio_state_changed_ = true;
  }

  void set_audio_state_changed(bool value) { audio_state_changed_ = value; }
  bool audio_state_changed() { return audio_state_changed_; }

 private:
  base::OnceClosure quit_closure_;
  bool audio_state_changed_ = false;
};

// A helper class that intercepts AddExpectedOriginAndFlags().
class TestAutoplayConfigurationClient
    : public blink::mojom::AutoplayConfigurationClientInterceptorForTesting {
 public:
  TestAutoplayConfigurationClient() = default;
  ~TestAutoplayConfigurationClient() override = default;

  TestAutoplayConfigurationClient(const TestAutoplayConfigurationClient&) =
      delete;
  TestAutoplayConfigurationClient& operator=(
      const TestAutoplayConfigurationClient&) = delete;

  AutoplayConfigurationClient* GetForwardingInterface() override {
    return nullptr;
  }

  void AddExpectedOriginAndFlags(const ::url::Origin& origin, int32_t flags) {
    expected_origin_flags_map_.emplace(origin, flags);
  }

  void WaitForAddAutoplayFlags() {
    if (expected_origin_flags_map_.empty())
      return;
    base::RunLoop run_loop;
    quit_closure_ = run_loop.QuitClosure();
    run_loop.Run();
  }

  void AddAutoplayFlags(const ::url::Origin& origin, int32_t flags) override {
    auto it = expected_origin_flags_map_.find(origin);
    EXPECT_TRUE(it != expected_origin_flags_map_.end());
    EXPECT_EQ(it->second, flags);
    expected_origin_flags_map_.erase(it);
    if (expected_origin_flags_map_.empty() && quit_closure_)
      std::move(quit_closure_).Run();
  }

  void BindReceiver(mojo::ScopedInterfaceEndpointHandle handle) {
    receiver_.reset();
    receiver_.Bind(
        mojo::PendingAssociatedReceiver<
            blink::mojom::AutoplayConfigurationClient>(std::move(handle)));
  }

 private:
  base::OnceClosure quit_closure_;
  std::map<const ::url::Origin, int32_t> expected_origin_flags_map_;
  mojo::AssociatedReceiver<blink::mojom::AutoplayConfigurationClient> receiver_{
      this};
};

// A helper class that creates TestAutoplayConfigurationClient per a frame and
// overrides binding for blink::mojom::AutoplayConfigurationClient.
class MultipleFramesObserver : public content::WebContentsObserver {
 public:
  explicit MultipleFramesObserver(content::WebContents* web_contents,
                                  size_t num_frames)
      : content::WebContentsObserver(web_contents), num_frames_(num_frames) {}
  ~MultipleFramesObserver() override = default;

  void RenderFrameCreated(
      content::RenderFrameHost* render_frame_host) override {
    // Creates TestAutoplayConfigurationClient for |render_frame_host| and
    // overrides AutoplayConfigurationClient interface.
    frame_to_client_map_[render_frame_host] =
        std::make_unique<TestAutoplayConfigurationClient>();
    OverrideInterface(render_frame_host,
                      frame_to_client_map_[render_frame_host].get());
    if (render_frame_host->GetParent() && quit_on_sub_frame_created_) {
      if (frame_to_client_map_.size() == num_frames_)
        std::move(quit_on_sub_frame_created_).Run();
    } else if (render_frame_host->IsFencedFrameRoot() &&
               quit_on_fenced_frame_created_) {
      if (frame_to_client_map_.size() == num_frames_)
        std::move(quit_on_fenced_frame_created_).Run();
    }
  }

  void RenderFrameDeleted(
      content::RenderFrameHost* render_frame_host) override {
    frame_to_client_map_.erase(render_frame_host);
  }

  // Waits for sub frame creations.
  void WaitForSubFrame() {
    if (frame_to_client_map_.size() == num_frames_)
      return;
    base::RunLoop run_loop;
    quit_on_sub_frame_created_ = run_loop.QuitClosure();
    run_loop.Run();
  }

  // Wait for fenced frame creation.
  void WaitForFencedFrame() {
    if (frame_to_client_map_.size() == num_frames_)
      return;
    base::RunLoop run_loop;
    quit_on_fenced_frame_created_ = run_loop.QuitClosure();
    run_loop.Run();
  }

  // Returns TestAutoplayConfigurationClient. If |request_main_frame| is true,
  // it searches for the main frame and return it. Otherwise, it returns a
  // sub frame.
  TestAutoplayConfigurationClient* GetTestClient(bool request_main_frame) {
    return GetTestClientWithFilter(base::BindLambdaForTesting(
        [&request_main_frame](content::RenderFrameHost* rfh) {
          bool is_main_frame = rfh->GetMainFrame() == rfh;
          return request_main_frame ? is_main_frame : !is_main_frame;
        }));
  }

  TestAutoplayConfigurationClient* GetTestClientForFencedFrame() {
    return GetTestClientWithFilter(
        base::BindLambdaForTesting([](content::RenderFrameHost* rfh) {
          return rfh->IsFencedFrameRoot();
        }));
  }

 private:
  void OverrideInterface(content::RenderFrameHost* render_frame_host,
                         TestAutoplayConfigurationClient* client) {
    render_frame_host->GetRemoteAssociatedInterfaces()
        ->OverrideBinderForTesting(
            blink::mojom::AutoplayConfigurationClient::Name_,
            base::BindRepeating(&TestAutoplayConfigurationClient::BindReceiver,
                                base::Unretained(client)));
  }

  using Filter = base::RepeatingCallback<bool(content::RenderFrameHost*)>;
  TestAutoplayConfigurationClient* GetTestClientWithFilter(Filter filter) {
    for (auto& client : frame_to_client_map_) {
      if (filter.Run(client.first)) {
        return client.second.get();
      }
    }
    NOTREACHED_IN_MIGRATION();
    return nullptr;
  }

  std::map<content::RenderFrameHost*,
           std::unique_ptr<TestAutoplayConfigurationClient>>
      frame_to_client_map_;
  base::OnceClosure quit_on_sub_frame_created_;
  base::OnceClosure quit_on_fenced_frame_created_;
  size_t num_frames_;
};

}  // namespace

class SoundContentSettingObserverBrowserTest : public InProcessBrowserTest {
 public:
  SoundContentSettingObserverBrowserTest()
      : prerender_helper_(base::BindRepeating(
            &SoundContentSettingObserverBrowserTest::web_contents,
            base::Unretained(this))) {}
  ~SoundContentSettingObserverBrowserTest() override = default;

  content::test::PrerenderTestHelper* prerender_helper() {
    return &prerender_helper_;
  }

  content::WebContents* web_contents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

 private:
  content::test::PrerenderTestHelper prerender_helper_;
  base::test::ScopedFeatureList feature_list_;
};

// Tests that the prerending doesn't affect SoundContentSettingObserver status
// with the main frame.
IN_PROC_BROWSER_TEST_F(SoundContentSettingObserverBrowserTest,
                       SoundContentSettingObserverInPrerendering) {
  // Sets up the embedded test server to serve the test javascript file.
  embedded_test_server()->ServeFilesFromSourceDirectory(kMediaTestDataPath);
  net::test_server::EmbeddedTestServerHandle test_server_handle;
  ASSERT_TRUE(test_server_handle =
                  embedded_test_server()->StartAndReturnHandle());

  // Configures to check `logged_site_muted_ukm_` in
  // SoundContentSettingObserver.
  HostContentSettingsMap* content_settings =
      HostContentSettingsMapFactory::GetForProfile(browser()->profile());
  content_settings->SetDefaultContentSetting(ContentSettingsType::SOUND,
                                             CONTENT_SETTING_BLOCK);

  GURL url = embedded_test_server()->GetURL("/webaudio_oscillator.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  // Start the audio on the current main frame.
  base::RunLoop run_loop;
  TestAudioStateObserver audio_state_observer(web_contents(),
                                              run_loop.QuitClosure());
  EXPECT_EQ("OK", content::EvalJs(web_contents(), "StartOscillator();"));
  run_loop.Run();

  SoundContentSettingObserver* observer =
      SoundContentSettingObserver::FromWebContents(web_contents());
  // `logged_site_muted_ukm_` should be set.
  EXPECT_TRUE(observer->HasLoggedSiteMutedUkmForTesting());

  // Loads a page in the prerender.
  auto prerender_url = embedded_test_server()->GetURL("/simple.html");
  content::FrameTreeNodeId host_id =
      prerender_helper()->AddPrerender(prerender_url);
  content::test::PrerenderHostObserver host_observer(*web_contents(), host_id);
  // The prerendering should not affect the current status.
  EXPECT_TRUE(observer->HasLoggedSiteMutedUkmForTesting());

  // Activates the page from the prerendering.
  prerender_helper()->NavigatePrimaryPage(*web_contents(), prerender_url);
  // Makes sure that the page is activated from the prerendering.
  EXPECT_TRUE(host_observer.was_activated());
  // It should be reset.
  EXPECT_FALSE(observer->HasLoggedSiteMutedUkmForTesting());
}

// Tests that a page calls AddAutoplayFlags() even if it's loaded in
// the prerendering. Since AddAutoplayFlags() is called on
// SoundContentSettingObserver::ReadyToCommitNavigation() with NavigationHandle
// URL, it uses a page that has a sub frame to make sure that it's called with
// the correct URL.
IN_PROC_BROWSER_TEST_F(SoundContentSettingObserverBrowserTest,
                       AddAutoplayFlagsInPrerendering) {
  // Sets up the embedded test server to serve the test javascript file.
  net::test_server::EmbeddedTestServerHandle test_server_handle;
  ASSERT_TRUE(test_server_handle =
                  embedded_test_server()->StartAndReturnHandle());

  // Configures SoundContentSettingObserver.
  GURL url = embedded_test_server()->GetURL("/simple.html");
  HostContentSettingsMap* content_settings =
      HostContentSettingsMapFactory::GetForProfile(browser()->profile());
  content_settings->SetContentSettingDefaultScope(
      url, url, ContentSettingsType::SOUND, CONTENT_SETTING_ALLOW);

  // Loads a simple page.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  content::test::PrerenderHostRegistryObserver registry_observer(
      *web_contents());
  auto prerender_url = embedded_test_server()->GetURL("/iframe.html");

  // Creates MultipleFramesObserver to intercept AddAutoplayFlags() for each
  // frame.
  MultipleFramesObserver observer{web_contents(), 2};

  // Loads a page in the prerender.
  prerender_helper()->AddPrerenderAsync(prerender_url);
  registry_observer.WaitForTrigger(prerender_url);
  auto host_id = prerender_helper()->GetHostForUrl(prerender_url);

  // Adds the expected url and flag for the main frame.
  observer.GetTestClient(true)->AddExpectedOriginAndFlags(
      url::Origin::Create(prerender_url),
      blink::mojom::kAutoplayFlagUserException);
  observer.GetTestClient(true)->WaitForAddAutoplayFlags();

  // Makes sure that the sub frame is created.
  observer.WaitForSubFrame();

  auto prerender_url_sub_frame = embedded_test_server()->GetURL("/title1.html");
  // Adds the expected url and flag for the sub frame.
  observer.GetTestClient(false)->AddExpectedOriginAndFlags(
      url::Origin::Create(prerender_url_sub_frame),
      blink::mojom::kAutoplayFlagUserException);
  observer.GetTestClient(false)->WaitForAddAutoplayFlags();

  // Waits until the prerendering is done.
  prerender_helper()->WaitForPrerenderLoadCompletion(prerender_url);

  content::test::PrerenderHostObserver host_observer(*web_contents(), host_id);

  // Adds the expected url and flag for the main frame after activation.
  observer.GetTestClient(true)->AddExpectedOriginAndFlags(
      url::Origin::Create(prerender_url),
      blink::mojom::kAutoplayFlagUserException);

  // Activates the page from the prerendering.
  prerender_helper()->NavigatePrimaryPage(prerender_url);
  observer.GetTestClient(true)->WaitForAddAutoplayFlags();

  // Makes sure that the page is activated from the prerendering.
  EXPECT_TRUE(host_observer.was_activated());
}

class SoundContentSettingObserverFencedFrameBrowserTest
    : public InProcessBrowserTest {
 public:
  SoundContentSettingObserverFencedFrameBrowserTest() = default;
  ~SoundContentSettingObserverFencedFrameBrowserTest() override = default;

  // TODO(crbug.com/40285326): This fails with the field trial testing config.
  void SetUpCommandLine(base::CommandLine* command_line) override {
    InProcessBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch("disable-field-trial-config");
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    https_server_.ServeFilesFromSourceDirectory(GetChromeTestDataDir());
  }

  content::test::FencedFrameTestHelper& fenced_frame_test_helper() {
    return fenced_frame_test_helper_;
  }

  content::WebContents* web_contents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  net::EmbeddedTestServer& https_server() { return https_server_; }

 private:
  content::test::FencedFrameTestHelper fenced_frame_test_helper_;
  base::test::ScopedFeatureList feature_list_;
  net::EmbeddedTestServer https_server_{net::EmbeddedTestServer::TYPE_HTTPS};
};

IN_PROC_BROWSER_TEST_F(SoundContentSettingObserverFencedFrameBrowserTest,
                       AddAutoplayFlagsInFencedFrame) {
  // Sets up the embedded test server to serve the test javascript file.
  net::test_server::EmbeddedTestServerHandle test_server_handle;
  ASSERT_TRUE(test_server_handle = https_server().StartAndReturnHandle());

  // Configures SoundContentSettingObserver and explicitly allows the SOUND
  // setting for the primary page's URL.
  GURL url = https_server().GetURL("/simple.html");
  HostContentSettingsMap* content_settings =
      HostContentSettingsMapFactory::GetForProfile(browser()->profile());
  content_settings->SetContentSettingDefaultScope(
      url, url, ContentSettingsType::SOUND, CONTENT_SETTING_ALLOW);

  // Loads a simple page.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  auto fenced_frame_url = https_server().GetURL("/fenced_frames/title1.html");

  // Create a blank fenced frame.
  EXPECT_TRUE(fenced_frame_test_helper().CreateFencedFrame(
      web_contents()->GetPrimaryMainFrame(), GURL()));

  // Creates MultipleFramesObserver to observe fenced frame creation and
  // intercept AddAutoplayFlags() for it.
  MultipleFramesObserver observer{web_contents(), 1};

  // Navigate a fenced frame and wait for the autoplay flag to be set.
  content::ExecuteScriptAsync(web_contents()->GetPrimaryMainFrame(),
                              content::JsReplace(R"(
                              document.getElementsByTagName('fencedframe')[0].
                              config = new FencedFrameConfig($1);
                           )",
                                                 fenced_frame_url));

  observer.WaitForFencedFrame();
  observer.GetTestClientForFencedFrame()->AddExpectedOriginAndFlags(
      url::Origin::Create(fenced_frame_url),
      blink::mojom::kAutoplayFlagUserException);
  observer.GetTestClientForFencedFrame()->WaitForAddAutoplayFlags();
}
