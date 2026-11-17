// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chrome_browser_interface_binders.h"

#include <memory>

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/top_chrome/top_chrome_webui_profile.h"
#include "chrome/browser/ui/webui/top_chrome/top_chrome_webui_user_profile.h"
#include "chrome/common/chrome_features.h"
#include "chrome/services/speech/buildflags/buildflags.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/live_caption/caption_util.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "media/mojo/mojom/speech_recognition.mojom.h"
#include "mojo/public/cpp/bindings/binder_map.h"
#include "mojo/public/cpp/bindings/generic_pending_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

#if BUILDFLAG(ENABLE_SPEECH_SERVICE)

namespace {

// Returns whether the Chrome frame binders bind `Interface` for `frame`.
template <typename Interface>
bool BindsInterface(content::RenderFrameHost* frame) {
  mojo::BinderMapWithContext<content::RenderFrameHost*> map;
  chrome::internal::PopulateChromeFrameBinders(&map, frame);
  mojo::Remote<Interface> remote;
  mojo::GenericPendingReceiver receiver(remote.BindNewPipeAndPassReceiver());
  EXPECT_TRUE(map.TryBind(frame, &receiver));
  remote.FlushForTesting();
  return remote.is_connected();
}

class ChromeBrowserInterfaceBindersTopChromeWebUITest
    : public InProcessBrowserTest {
 private:
  base::test::ScopedFeatureList feature_list_{
      features::kTopChromeWebUISharedRenderer};
};

// A WebUI in TopChromeWebUIProfile, which is shared by the windows of all
// profiles, doesn't bind SpeechRecognitionClientBrowserInterface, which Live
// Caption binds first.
IN_PROC_BROWSER_TEST_F(ChromeBrowserInterfaceBindersTopChromeWebUITest,
                       DoesNotBindSpeechRecognitionClientBrowserInterface) {
  Profile* top_chrome_webui_profile = webui::GetTopChromeWebUIProfile();
  ASSERT_TRUE(top_chrome_webui_profile);
  std::unique_ptr<content::WebContents> contents = content::WebContents::Create(
      content::WebContents::CreateParams(top_chrome_webui_profile));
  webui::SetUserProfile(contents.get(), chrome_test_utils::GetProfile(this));
  content::RenderFrameHost* frame = contents->GetPrimaryMainFrame();

  EXPECT_FALSE(
      BindsInterface<media::mojom::SpeechRecognitionClientBrowserInterface>(
          frame));

  // A tab of the user profile still binds it where Live Caption is supported.
  if (captions::IsLiveCaptionFeatureSupported()) {
    EXPECT_TRUE(
        BindsInterface<media::mojom::SpeechRecognitionClientBrowserInterface>(
            chrome_test_utils::GetActiveWebContents(this)
                ->GetPrimaryMainFrame()));
  }
}

}  // namespace

#endif  // BUILDFLAG(ENABLE_SPEECH_SERVICE)
