// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/accessibility/live_caption/live_caption_unavailability_notifier.h"

#include <memory>
#include <utility>

#include "base/containers/contains.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/accessibility/caption_bubble_context_browser.h"
#include "chrome/browser/accessibility/live_caption/live_caption_controller_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "components/live_caption/live_caption_controller.h"
#include "components/live_caption/pref_names.h"
#include "components/live_caption/views/caption_bubble.h"
#include "components/live_caption/views/caption_bubble_model.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "media/mojo/mojom/renderer_extensions.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "url/origin.h"

namespace captions {

// static
void LiveCaptionUnavailabilityNotifier::Create(
    content::RenderFrameHost* frame_host,
    mojo::PendingReceiver<media::mojom::MediaFoundationRendererNotifier>
        receiver) {
  CHECK(frame_host);
  // The object is bound to the lifetime of |frame_host| and the mojo
  // connection. See DocumentService for details.
  new LiveCaptionUnavailabilityNotifier(*frame_host, std::move(receiver));
}

LiveCaptionUnavailabilityNotifier::LiveCaptionUnavailabilityNotifier(
    content::RenderFrameHost& frame_host,
    mojo::PendingReceiver<media::mojom::MediaFoundationRendererNotifier>
        receiver)
    : DocumentService<media::mojom::MediaFoundationRendererNotifier>(
          frame_host,
          std::move(receiver)) {
  content::WebContents* web_contents = GetWebContents();
  if (!web_contents)
    return;
  context_ = CaptionBubbleContextBrowser::Create(web_contents);

  Profile* profile =
      Profile::FromBrowserContext(render_frame_host().GetBrowserContext());
  profile_prefs_ = profile->GetPrefs();

  pref_change_registrar_ = std::make_unique<PrefChangeRegistrar>();
  pref_change_registrar_->Init(profile_prefs_);

  // Unretained is safe because |this| owns the pref_change_registrar_.
  pref_change_registrar_->Add(
      prefs::kLiveCaptionEnabled,
      base::BindRepeating(&LiveCaptionUnavailabilityNotifier::
                              OnSpeechRecognitionAvailabilityChanged,
                          base::Unretained(this)));
}

LiveCaptionUnavailabilityNotifier::~LiveCaptionUnavailabilityNotifier() {
  LiveCaptionController* live_caption_controller = GetLiveCaptionController();
  if (live_caption_controller)
    live_caption_controller->OnAudioStreamEnd(context_.get());
}

void LiveCaptionUnavailabilityNotifier::MediaFoundationRendererCreated(
    mojo::PendingReceiver<media::mojom::MediaFoundationRendererObserver>
        observer) {
  observers_.Add(this, std::move(observer));
  if (ShouldDisplayMediaFoundationRendererError()) {
    DisplayMediaFoundationRendererError();
  }
}

content::WebContents* LiveCaptionUnavailabilityNotifier::GetWebContents() {
  return content::WebContents::FromRenderFrameHost(&render_frame_host());
}

LiveCaptionController*
LiveCaptionUnavailabilityNotifier::GetLiveCaptionController() {
  Profile* profile =
      Profile::FromBrowserContext(render_frame_host().GetBrowserContext());
  if (!profile)
    return nullptr;

  return LiveCaptionControllerFactory::GetForProfile(profile);
}

bool LiveCaptionUnavailabilityNotifier::
    ShouldDisplayMediaFoundationRendererError() {
  if (!profile_prefs_->GetBoolean(prefs::kLiveCaptionEnabled))
    return false;

  if (observers_.empty())
    return false;

  return !ErrorSilencedForOrigin();
}

bool LiveCaptionUnavailabilityNotifier::ErrorSilencedForOrigin() {
  using SelectConstVersion = const std::string& (base::Value::*)() const;
  return base::Contains(
      profile_prefs_->GetList(
          prefs::kLiveCaptionMediaFoundationRendererErrorSilenced),
      render_frame_host().GetLastCommittedOrigin().Serialize(),
      static_cast<SelectConstVersion>(&base::Value::GetString));
}

void LiveCaptionUnavailabilityNotifier::DisplayMediaFoundationRendererError() {
  LiveCaptionController* live_caption_controller = GetLiveCaptionController();
  if (!live_caption_controller)
    return;

  // This will trigger the caption bubble to display a message informing the
  // user that Live Caption is unavailable and link them to the settings page
  // where they can disable the media foundation renderer to enable Live
  // Caption. The error message may be overwritten if recognition events are
  // received from another audio stream.
  live_caption_controller->OnError(
      context_.get(),
      CaptionBubbleErrorType::kMediaFoundationRendererUnsupported,
      base::BindRepeating(&LiveCaptionUnavailabilityNotifier::
                              OnMediaFoundationRendererErrorClicked,
                          weak_factory_.GetWeakPtr()),
      base::BindRepeating(
          &LiveCaptionUnavailabilityNotifier::
              OnMediaFoundationRendererErrorDoNotShowAgainCheckboxClicked,
          weak_factory_.GetWeakPtr()));
}

void LiveCaptionUnavailabilityNotifier::
    OnMediaFoundationRendererErrorDoNotShowAgainCheckboxClicked(
        CaptionBubbleErrorType error_type,
        bool checked) {
  PrefService* prefs =
      Profile::FromBrowserContext(render_frame_host().GetBrowserContext())
          ->GetPrefs();
  ScopedListPrefUpdate update(
      prefs, prefs::kLiveCaptionMediaFoundationRendererErrorSilenced);
  if (checked) {
    update->Append(render_frame_host().GetLastCommittedOrigin().Serialize());
  } else {
    update->EraseIf([&](const base::Value& value) {
      return value.GetString() ==
             render_frame_host().GetLastCommittedOrigin().Serialize();
    });
  }
}

void LiveCaptionUnavailabilityNotifier::
    OnMediaFoundationRendererErrorClicked() {
  Profile* profile =
      Profile::FromBrowserContext(render_frame_host().GetBrowserContext());
  chrome::ShowSiteSettings(
      profile, render_frame_host().GetLastCommittedOrigin().GetURL());
}

void LiveCaptionUnavailabilityNotifier::
    OnSpeechRecognitionAvailabilityChanged() {
  if (ShouldDisplayMediaFoundationRendererError()) {
    DisplayMediaFoundationRendererError();
  }
}

}  // namespace captions
