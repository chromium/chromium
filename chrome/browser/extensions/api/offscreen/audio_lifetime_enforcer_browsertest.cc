// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/offscreen/audio_lifetime_enforcer.h"

#include <optional>

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/profiles/profile.h"
#include "components/version_info/channel.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "extensions/browser/extension_util.h"
#include "extensions/browser/offscreen_document_host.h"
#include "extensions/browser/process_manager.h"
#include "extensions/common/extension.h"
#include "extensions/test/test_extension_dir.h"

namespace extensions {

namespace {

// A helper class to wait until a given WebContents is audible or inaudible.
class AudioWaiter : public content::WebContentsObserver {
 public:
  explicit AudioWaiter(content::WebContents* contents)
      : content::WebContentsObserver(contents) {}

  void WaitForAudible() {
    DCHECK(!expected_state_);

    if (web_contents()->IsCurrentlyAudible())
      return;

    expected_state_ = true;
    run_loop_.Run();
  }

  void WaitForInaudible() {
    DCHECK(!expected_state_);

    if (!web_contents()->IsCurrentlyAudible())
      return;

    expected_state_ = false;
    run_loop_.Run();
  }

 private:
  void OnAudioStateChanged(bool audible) override {
    if (!expected_state_.has_value()) {
      // We aren't waiting for an event yet, so don't quit the run loop.
      // Otherwise, we'd quit it now, and it would immediately quit when we
      // later try to wait for it in WaitFor(In)Audible().
      return;
    }

    // Otherwise, the expected state should be equal to the new state (since
    // there are only two possible states, and if it were already in the
    // appropriate state, we wouldn't have waited).
    EXPECT_EQ(expected_state_, audible);
    run_loop_.QuitWhenIdle();
  }

  base::RunLoop run_loop_;

  // The eventual desired state.
  std::optional<bool> expected_state_;
};

}  // namespace

class AudioLifetimeEnforcerBrowserTest : public ExtensionApiTest {
 public:
  AudioLifetimeEnforcerBrowserTest() = default;
  ~AudioLifetimeEnforcerBrowserTest() override = default;

  // Creates a new OffscreenDocumentHost and waits for it to load.
  std::unique_ptr<OffscreenDocumentHost> CreateOffscreenDocument(
      const Extension& extension,
      const GURL& url) {
    scoped_refptr<content::SiteInstance> site_instance =
        ProcessManager::Get(profile())->GetSiteInstanceForURL(url);

    content::TestNavigationObserver navigation_observer(url);
    navigation_observer.StartWatchingNewWebContents();
    auto offscreen_document = std::make_unique<OffscreenDocumentHost>(
        extension, site_instance.get(), url);
    offscreen_document->CreateRendererSoon();
    navigation_observer.Wait();
    EXPECT_TRUE(navigation_observer.last_navigation_succeeded());

    return offscreen_document;
  }

  scoped_refptr<const Extension> LoadOffscreenDocumentExtension() {
    static constexpr char kManifest[] =
        R"({
             "name": "Offscreen Document Test",
             "manifest_version": 3,
             "version": "0.1",
             "permissions": ["offscreen"]
           })";
    test_dir_.WriteManifest(kManifest);
    test_dir_.WriteFile(FILE_PATH_LITERAL("background.js"), "// Blank.");
    test_dir_.WriteFile(FILE_PATH_LITERAL("offscreen.html"),
                        "<html>offscreen</html>");

    return LoadExtension(test_dir_.UnpackedPath());
  }

 private:
  TestExtensionDir test_dir_;
};

// Tests that an offscreen document is considered active while playing audio and
// notifies of inactivity when audio stops.
IN_PROC_BROWSER_TEST_F(AudioLifetimeEnforcerBrowserTest,
                       DocumentActiveWhilePlayingAudio) {
  scoped_refptr<const Extension> extension = LoadOffscreenDocumentExtension();
  ASSERT_TRUE(extension);

  const GURL offscreen_url = extension->GetResourceURL("offscreen.html");
  std::unique_ptr<OffscreenDocumentHost> offscreen_document =
      CreateOffscreenDocument(*extension, offscreen_url);

  content::WebContents* contents = offscreen_document->host_contents();

  bool terminate_called = false;
  int notify_inactive_calls = 0;
  auto on_notify_inactive = [&notify_inactive_calls]() {
    ++notify_inactive_calls;
  };
  auto on_terminate_called = [&terminate_called]() { terminate_called = true; };
  AudioLifetimeEnforcer audio_enforcer(
      offscreen_document.get(), base::BindLambdaForTesting(on_terminate_called),
      base::BindLambdaForTesting(on_notify_inactive));

  // On creation, the document is considered active (we give it some leeway
  // because the audio needs to load).
  EXPECT_TRUE(audio_enforcer.IsActive());
  EXPECT_EQ(0, notify_inactive_calls);

  // Load an audio tag and play it.
  static constexpr char kPlayAudio[] =
      R"(const audioTag = document.createElement('audio');
         audioTag.src = '_test_resources/long_audio.ogg';
         document.body.appendChild(audioTag);
         audioTag.play();
         'done';)";

  {
    AudioWaiter audio_waiter(contents);
    EXPECT_EQ("done", content::EvalJs(contents, kPlayAudio));
    audio_waiter.WaitForAudible();
  }

  // The document should be considered active.
  EXPECT_EQ(0, notify_inactive_calls);
  EXPECT_FALSE(terminate_called);
  EXPECT_TRUE(audio_enforcer.IsActive());

  // Next, prepare to stop the audio.
  static constexpr char kStopAudio[] =
      R"(document.body.getElementsByTagName('audio')[0].pause();
         'done';)";

  // Override the timeout. We can't do this at the top of the test because
  // otherwise, the document would immediately be considered inactive.
  auto timeout_override =
      AudioLifetimeEnforcer::SetTimeoutForTesting(base::Seconds(0));

  {
    // Stop the audio.
    AudioWaiter audio_waiter(contents);
    EXPECT_TRUE(content::ExecJs(contents, kStopAudio));
    audio_waiter.WaitForInaudible();
  }

  // The document should no longer be active.
  EXPECT_EQ(1, notify_inactive_calls);
  EXPECT_FALSE(audio_enforcer.IsActive());
  EXPECT_FALSE(terminate_called);
}

// Tests that an offscreen document is considered inactive if it never plays
// audio.
IN_PROC_BROWSER_TEST_F(AudioLifetimeEnforcerBrowserTest,
                       DocumentInactiveIfNeverPlayedAudio) {
  // Override the timeout to be immediate.
  auto timeout_override =
      AudioLifetimeEnforcer::SetTimeoutForTesting(base::Seconds(0));

  scoped_refptr<const Extension> extension = LoadOffscreenDocumentExtension();
  ASSERT_TRUE(extension);

  const GURL offscreen_url = extension->GetResourceURL("offscreen.html");
  std::unique_ptr<OffscreenDocumentHost> offscreen_document =
      CreateOffscreenDocument(*extension, offscreen_url);

  bool terminate_called = false;
  int notify_inactive_calls = 0;
  auto on_notify_inactive = [&notify_inactive_calls]() {
    ++notify_inactive_calls;
  };
  auto on_terminate_called = [&terminate_called]() { terminate_called = true; };
  AudioLifetimeEnforcer audio_enforcer(
      offscreen_document.get(), base::BindLambdaForTesting(on_terminate_called),
      base::BindLambdaForTesting(on_notify_inactive));

  // On creation, the document is considered active...
  EXPECT_EQ(0, notify_inactive_calls);
  EXPECT_FALSE(terminate_called);
  EXPECT_TRUE(audio_enforcer.IsActive());

  // ... but if it doesn't play audio after a set amount of time (here, 0
  // seconds), it is inactive (and thus, terminated).
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, notify_inactive_calls);
  EXPECT_FALSE(terminate_called);
  EXPECT_FALSE(audio_enforcer.IsActive());
}

}  // namespace extensions
