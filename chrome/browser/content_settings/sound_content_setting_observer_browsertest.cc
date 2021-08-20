// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/content_settings/sound_content_setting_observer.h"

#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/content_settings/sound_content_setting_observer.h"
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
  explicit MultipleFramesObserver(content::WebContents* web_contents)
      : content::WebContentsObserver(web_contents) {}
  ~MultipleFramesObserver() override = default;

  const size_t kMaxFrameSize = 2u;

  void RenderFrameCreated(
      content::RenderFrameHost* render_frame_host) override {
    // Creates TestAutoplayConfigurationClient for |render_frame_host| and
    // overrides AutoplayConfigurationClient interface.
    frame_to_client_map_[render_frame_host] =
        std::make_unique<TestAutoplayConfigurationClient>();
    OverrideInterface(render_frame_host,
                      frame_to_client_map_[render_frame_host].get());
    if (quit_on_sub_frame_created_) {
      if (frame_to_client_map_.size() == kMaxFrameSize)
        std::move(quit_on_sub_frame_created_).Run();
    }
  }

  void RenderFrameDeleted(
      content::RenderFrameHost* render_frame_host) override {
    frame_to_client_map_.erase(render_frame_host);
  }

  // Waits for sub frame creations.
  void WaitForSubFrame() {
    if (frame_to_client_map_.size() == kMaxFrameSize)
      return;
    base::RunLoop run_loop;
    quit_on_sub_frame_created_ = run_loop.QuitClosure();
    run_loop.Run();
  }

  // Returns TestAutoplayConfigurationClient. If |request_main_frame| is true,
  // it searches for the main frame and return it. Otherwise, it returns a
  // sub frame.
  TestAutoplayConfigurationClient* GetTestClient(bool request_main_frame) {
    for (auto& client : frame_to_client_map_) {
      bool is_main_frame = client.first->GetMainFrame() == client.first;
      bool expected = request_main_frame ? is_main_frame : !is_main_frame;
      if (expected)
        return client.second.get();
    }
    NOTREACHED();
    return nullptr;
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

  std::map<content::RenderFrameHost*,
           std::unique_ptr<TestAutoplayConfigurationClient>>
      frame_to_client_map_;
  base::OnceClosure quit_on_sub_frame_created_;
};

}  // namespace

class SoundContentSettingObserverBrowserTest : public InProcessBrowserTest {
 public:
  SoundContentSettingObserverBrowserTest()
      : prerender_helper_(base::BindRepeating(
            &SoundContentSettingObserverBrowserTest::web_contents,
            base::Unretained(this))) {
    // TODO(crbug.com/1233349): In cases where a new AudioContext is created
    // in a blocking script, the optimizations with this feature can cause
    // hangs. See bug for more details.
    feature_list_.InitAndDisableFeature(
        features::kNavigationThreadingOptimizations);
  }
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
  ui_test_utils::NavigateToURL(browser(), url);

  // Start the audio on the current main frame.
  base::RunLoop run_loop;
  TestAudioStateObserver audio_state_observer(web_contents(),
                                              run_loop.QuitClosure());
  EXPECT_EQ("OK", content::EvalJs(web_contents(), "StartOscillator();",
                                  content::EXECUTE_SCRIPT_USE_MANUAL_REPLY));
  run_loop.Run();

  SoundContentSettingObserver* observer =
      SoundContentSettingObserver::FromWebContents(web_contents());
  // `logged_site_muted_ukm_` should be set.
  EXPECT_TRUE(observer->HasLoggedSiteMutedUkmForTesting());

  // Loads a page in the prerender.
  auto prerender_url = embedded_test_server()->GetURL("/simple.html");
  int host_id = prerender_helper()->AddPrerender(prerender_url);
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
  content_settings->SetWebsiteSettingDefaultScope(
      url, url, ContentSettingsType::SOUND,
      std::make_unique<base::Value>(CONTENT_SETTING_ALLOW));

  // Loads a simple page.
  ui_test_utils::NavigateToURL(browser(), url);

  content::test::PrerenderHostRegistryObserver registry_observer(
      *web_contents());
  auto prerender_url = embedded_test_server()->GetURL("/iframe.html");

  // Creates MultipleFramesObserver to intercept AddAutoplayFlags() for each
  // frame.
  MultipleFramesObserver observer{web_contents()};

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

// Tests that the page in the prerendering doesn't change the audio state. After
// the page is activated, it can play a sound and change the audio state.
IN_PROC_BROWSER_TEST_F(SoundContentSettingObserverBrowserTest,
                       AudioStateIsNotChangedInPrerendering) {
  // Sets up the embedded test server to serve the test javascript file.
  embedded_test_server()->ServeFilesFromSourceDirectory(kMediaTestDataPath);
  net::test_server::EmbeddedTestServerHandle test_server_handle;
  ASSERT_TRUE(test_server_handle =
                  embedded_test_server()->StartAndReturnHandle());

  // Blocks to play a sound by default.
  HostContentSettingsMap* content_settings =
      HostContentSettingsMapFactory::GetForProfile(browser()->profile());
  content_settings->SetDefaultContentSetting(ContentSettingsType::SOUND,
                                             CONTENT_SETTING_BLOCK);

  // Loads a simple page as a primary page.
  GURL url = embedded_test_server()->GetURL("/simple.html");
  ui_test_utils::NavigateToURL(browser(), url);

  content::test::PrerenderHostRegistryObserver registry_observer(
      *web_contents());
  // Loads a page in the prerender.
  auto prerender_url =
      embedded_test_server()->GetURL("/webaudio_oscillator.html");
  prerender_helper()->AddPrerenderAsync(prerender_url);
  registry_observer.WaitForTrigger(prerender_url);
  auto host_id = prerender_helper()->GetHostForUrl(prerender_url);
  EXPECT_NE(content::RenderFrameHost::kNoFrameTreeNodeId, host_id);

  content::test::PrerenderHostObserver host_observer(*web_contents(), host_id);
  content::RenderFrameHost* render_frame_host =
      prerender_helper()->GetPrerenderedMainFrameHost(host_id);
  EXPECT_TRUE(content::WaitForRenderFrameReady(render_frame_host));
  // Tries to start the audio on the prerendered page.
  {
    base::RunLoop run_loop;
    TestAudioStateObserver audio_state_observer(web_contents(),
                                                run_loop.QuitClosure());
    content::ExecuteScriptAsync(render_frame_host, "StartOscillator();");
    // The prerendering page should not start the audio.
    EXPECT_FALSE(audio_state_observer.audio_state_changed());
  }
  // The prerendering should not affect the current status.
  SoundContentSettingObserver* observer =
      SoundContentSettingObserver::FromWebContents(web_contents());
  EXPECT_FALSE(observer->HasLoggedSiteMutedUkmForTesting());

  // Activates the page from the prerendering.
  prerender_helper()->NavigatePrimaryPage(prerender_url);
  // Makes sure that the page is activated from the prerendering.
  EXPECT_TRUE(host_observer.was_activated());
  EXPECT_TRUE(content::WaitForRenderFrameReady(web_contents()->GetMainFrame()));
  // Tries to start the audio on the primary page.
  {
    base::RunLoop run_loop;
    TestAudioStateObserver audio_state_observer(web_contents(),
                                                run_loop.QuitClosure());
    EXPECT_EQ("OK", content::EvalJs(web_contents(), "StartOscillator();",
                                    content::EXECUTE_SCRIPT_USE_MANUAL_REPLY));
    run_loop.Run();
    // The page should try to start the audio.
    EXPECT_TRUE(audio_state_observer.audio_state_changed());
  }
  // The page starts logging for the muted site since the setting is
  // CONTENT_SETTING_BLOCK.
  EXPECT_TRUE(observer->HasLoggedSiteMutedUkmForTesting());
}

// Tests that the page-specific settings for `ContentSettingsType::SOUND` is not
// updated in the prerendered page to make sure that it's fine that
// SoundContentSettingObserver::CheckSoundBlocked() access to the main frame
// from WebContents.
IN_PROC_BROWSER_TEST_F(SoundContentSettingObserverBrowserTest,
                       NotUpdateCheckSoundBlockedInPrerendering) {
  net::test_server::EmbeddedTestServerHandle test_server_handle;
  ASSERT_TRUE(test_server_handle =
                  embedded_test_server()->StartAndReturnHandle());

  // Load a simple page.
  GURL url = embedded_test_server()->GetURL("/simple.html");
  ui_test_utils::NavigateToURL(browser(), url);

  const std::string kPlayingSoundScript(
      "var context = new window.AudioContext();"
      "var oscillator = context.createOscillator();"
      "oscillator.connect(context.destination);"
      "oscillator.start();");

  base::RunLoop run_loop;
  TestAudioStateObserver audio_state_observer(web_contents(),
                                              run_loop.QuitClosure());
  ASSERT_TRUE(
      content::ExecJs(web_contents()->GetMainFrame(), kPlayingSoundScript));
  run_loop.Run();
  // The page should try to start the audio.
  EXPECT_TRUE(audio_state_observer.audio_state_changed());

  // Since the main frame is playing a sound, WebContents knows it is audible.
  EXPECT_TRUE(web_contents()->IsCurrentlyAudible());

  // Change the profile-wide settings to block sound by default.
  HostContentSettingsMap* content_settings =
      HostContentSettingsMapFactory::GetForProfile(browser()->profile());
  content_settings->SetDefaultContentSetting(ContentSettingsType::SOUND,
                                             CONTENT_SETTING_BLOCK);
  // Check that the primary page's content settings know that sound is blocked.
  auto* primary_page_settings =
      content_settings::PageSpecificContentSettings::GetForFrame(
          web_contents()->GetMainFrame());
  EXPECT_TRUE(
      primary_page_settings->IsContentBlocked(ContentSettingsType::SOUND));

  // Start a prerendered page.
  content::test::PrerenderHostRegistryObserver registry_observer(
      *web_contents());
  auto prerender_url = embedded_test_server()->GetURL("/empty.html");
  int host_id = prerender_helper()->AddPrerender(prerender_url);
  content::test::PrerenderHostObserver host_observer(*web_contents(), host_id);
  content::RenderFrameHost* render_frame_host =
      prerender_helper()->GetPrerenderedMainFrameHost(host_id);

  // Reset the flag to test if audio state is changed by running the script to
  // play a sound on the prerendered page.
  audio_state_observer.set_audio_state_changed(false);
  // Try to play a sound in the prerendering page.
  content::ExecuteScriptAsync(render_frame_host, kPlayingSoundScript);
  // The prerendering page should not start the audio.
  EXPECT_FALSE(audio_state_observer.audio_state_changed());
  // The main page is still audible as the prerendering doesn't affect playing a
  // sound.
  EXPECT_TRUE(web_contents()->IsCurrentlyAudible());

  // Set the profile-wide contents settings to block sound again, to see if this
  // updates the page-specific settings for the prerendered page or primary
  // page.
  content_settings->SetDefaultContentSetting(ContentSettingsType::SOUND,
                                             CONTENT_SETTING_BLOCK);
  auto* prerendered_frame_settings =
      content_settings::PageSpecificContentSettings::GetForFrame(
          render_frame_host);
  // Audio is not blocked on the prerendered page since it doesn't play a sound.
  EXPECT_FALSE(
      prerendered_frame_settings->IsContentBlocked(ContentSettingsType::SOUND));
  // Audio is blocked on the current page since it is playing a sound.
  EXPECT_TRUE(
      primary_page_settings->IsContentBlocked(ContentSettingsType::SOUND));

  {
    base::RunLoop run_loop;
    TestAudioStateObserver audio_state_observer(web_contents(),
                                                run_loop.QuitClosure(), true);

    // Activate the prerendering page.
    prerender_helper()->NavigatePrimaryPage(prerender_url);
    EXPECT_TRUE(host_observer.was_activated());

    // Since audio stream status is posted to UI thread, wait until it's
    // updated.
    run_loop.Run();
    EXPECT_TRUE(audio_state_observer.audio_state_changed());
  }

  auto* activated_page_settings =
      content_settings::PageSpecificContentSettings::GetForFrame(
          web_contents()->GetMainFrame());
  // It should be false since the page is activated from the prerendering.
  // TODO(crbug.com/1228567): If AudioContext activation is handled, the audio
  // would be played and the status also would be updated.
  EXPECT_FALSE(
      activated_page_settings->IsContentBlocked(ContentSettingsType::SOUND));
  EXPECT_FALSE(web_contents()->IsCurrentlyAudible());
}
