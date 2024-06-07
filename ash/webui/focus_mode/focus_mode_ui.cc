// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/focus_mode/focus_mode_ui.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/url_constants.h"
#include "ash/webui/common/trusted_types_util.h"
#include "ash/webui/grit/ash_focus_mode_resources.h"
#include "ash/webui/grit/ash_focus_mode_resources_map.h"
#include "base/strings/stringprintf.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/common/bindings_policy.h"
#include "content/public/common/url_constants.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/common/web_preferences/web_preferences.h"
#include "ui/webui/webui_allowlist.h"

namespace ash {

class FocusModeTrackProvider : public focus_mode::mojom::TrackProvider {
 public:
  void GetTrack(GetTrackCallback callback) override {
    // TODO: Get the actual track from focus mode.
    auto track = focus_mode::mojom::TrackDefinition::New(
        "Title", "Artist", GURL{},
        GURL{"https://www.gstatic.com/chromeos-focusmode/tracks/flow/"
             "Release_20240516.mp3"});
    std::move(callback).Run(std::move(track));
  }

  void SetMediaClient(
      mojo::PendingRemote<focus_mode::mojom::MediaClient> client) override {
    client_remote_.reset();
    client_remote_.Bind(std::move(client));
  }

  void BindInterface(
      mojo::PendingReceiver<focus_mode::mojom::TrackProvider> receiver) {
    receiver_.reset();
    receiver_.Bind(std::move(receiver));
  }

 private:
  mojo::Remote<focus_mode::mojom::MediaClient> client_remote_;
  mojo::Receiver<focus_mode::mojom::TrackProvider> receiver_{this};
};

FocusModeUI::FocusModeUI(content::WebUI* web_ui)
    : ui::MojoWebUIController(web_ui),
      track_provider_(std::make_unique<FocusModeTrackProvider>()) {
  // Set up the chrome://focus-mode-media source. Note that for the trusted
  // page, we need to pass the *host* as second parameter.
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      web_ui->GetWebContents()->GetBrowserContext(),
      chrome::kChromeUIFocusModeMediaHost);

  // This is needed so that the page can load the iframe from chrome-untrusted.
  web_ui->AddRequestableScheme(content::kChromeUIUntrustedScheme);

  // Setup chrome://focus-mode-media main page.
  source->AddResourcePath("", IDR_ASH_FOCUS_MODE_FOCUS_MODE_HTML);
  // Add chrome://focus-mode-media content.
  source->AddResourcePaths(
      base::make_span(kAshFocusModeResources, kAshFocusModeResourcesSize));

  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::DefaultSrc, "default-src 'self';");
  // Enables the page to load the untrusted page in an iframe.
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::FrameSrc,
      base::StringPrintf("frame-src %s;", chrome::kChromeUIFocusModePlayerURL));
  ash::EnableTrustedTypesCSP(source);

  // This sets the untrusted page to be in a web app scope. This in turn enables
  // autoplay of audio on the page. Without this, the page would require user
  // interaction in order to play audio, which isn't possible since the web UI
  // is hidden. See AutoPlayPolicy::GetAutoplayPolicyForDocument for more info.
  auto* web_contents = web_ui->GetWebContents();
  auto prefs = web_contents->GetOrCreateWebPreferences();
  prefs.web_app_scope = GURL(chrome::kChromeUIFocusModePlayerURL);
  web_contents->SetWebPreferences(prefs);
}

FocusModeUI::~FocusModeUI() = default;

void FocusModeUI::BindInterface(
    mojo::PendingReceiver<focus_mode::mojom::TrackProvider> receiver) {
  track_provider_->BindInterface(std::move(receiver));
}

WEB_UI_CONTROLLER_TYPE_IMPL(FocusModeUI)

FocusModeUIConfig::FocusModeUIConfig()
    : WebUIConfig(content::kChromeUIScheme,
                  chrome::kChromeUIFocusModeMediaHost) {}

std::unique_ptr<content::WebUIController>
FocusModeUIConfig::CreateWebUIController(content::WebUI* web_ui,
                                         const GURL& url) {
  return std::make_unique<FocusModeUI>(web_ui);
}

bool FocusModeUIConfig::IsWebUIEnabled(
    content::BrowserContext* browser_context) {
  return ash::features::IsFocusModeEnabled();
}

}  // namespace ash
