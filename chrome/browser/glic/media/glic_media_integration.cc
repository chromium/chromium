// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/media/glic_media_integration.h"

#include "base/feature_list.h"
#include "base/no_destructor.h"
#include "base/supports_user_data.h"
#include "chrome/browser/accessibility/live_caption/live_caption_controller_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/live_caption/caption_controller_base.h"
#include "components/live_caption/caption_util.h"
#include "components/live_caption/live_caption_controller.h"
#include "components/live_caption/pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_contents.h"
#include "media/base/media_switches.h"

namespace {

constexpr char kGlicMediaIntegrationKey[] = "GlicMediaIntegration";

class GlicMediaIntegrationImpl : public glic::GlicMediaIntegration,
                                 public base::SupportsUserData::Data {
 public:
  explicit GlicMediaIntegrationImpl(Profile*);
  ~GlicMediaIntegrationImpl() override = default;

  void ComputeContext(content::WebContents*,
                      size_t max_size_bytes,
                      ContextCallback) override;

  void OnTranscription(const media::SpeechRecognitionResult&);

 protected:
  // Trim `context_` to `max_size_bytes_`.
  void TrimContext();

  raw_ptr<Profile> profile_;
  std::string context_;
  size_t max_size_bytes_ = 20000;
};

class CaptionListenerImpl : public captions::CaptionControllerBase::Listener {
 public:
  explicit CaptionListenerImpl(Profile* profile) : profile_(profile) {}
  ~CaptionListenerImpl() override = default;

  bool OnTranscription(captions::CaptionBubbleContext*,
                       const media::SpeechRecognitionResult& result) override {
    // could also wp callback, which is probably clearer.
    auto* media_integration = static_cast<GlicMediaIntegrationImpl*>(
        profile_->GetUserData(kGlicMediaIntegrationKey));

    if (!media_integration) {
      return false;
    }

    media_integration->OnTranscription(result);
    return true;
  }

  void OnAudioStreamEnd(captions::CaptionBubbleContext*) override {}
  void OnLanguageIdentificationEvent(
      captions::CaptionBubbleContext*,
      const media::mojom::LanguageIdentificationEventPtr&) override {}

 private:
  raw_ptr<Profile> profile_;
};

GlicMediaIntegrationImpl::GlicMediaIntegrationImpl(Profile* profile)
    : profile_(profile) {
  auto* lc = captions::LiveCaptionControllerFactory::GetForProfile(profile_);
  lc->AddListener(std::make_unique<CaptionListenerImpl>(profile));

  // For now, enable the pref if we get this far.  Do this after getting the
  // Live Caption controller, since it resets the pref to false.
  profile->GetPrefs()->SetBoolean(prefs::kHeadlessCaptionEnabled, true);
}

void GlicMediaIntegrationImpl::OnTranscription(
    const media::SpeechRecognitionResult& result) {
  if (!result.is_final) {
    return;
  }

  context_ += result.transcription;
  TrimContext();
}

void GlicMediaIntegrationImpl::ComputeContext(content::WebContents*,
                                              size_t max_size_bytes,
                                              ContextCallback cb) {
  max_size_bytes_ = max_size_bytes;
  TrimContext();
  std::move(cb).Run(context_);
}

void GlicMediaIntegrationImpl::TrimContext() {
  // Trim to `max_size`.  Note that we should utf8-trim, but this is easier.
  if (size_t context_size = context_.length()) {
    if (context_size > max_size_bytes_) {
      // Remove the beginning of the context, leaving the end.
      context_ = context_.substr(context_size - max_size_bytes_);
    }
  }
}

}  // namespace

namespace glic {

// static
GlicMediaIntegration* GlicMediaIntegration::GetFor(content::WebContents* wc) {
  // This should also check the pref, once it's not toggled automatically.
  // We'll want to install a pref listener, and possibly clean up if the pref
  // is switched off after construction.
  if (!wc || !captions::IsHeadlessCaptionFeatureSupported()) {
    return nullptr;
  }

  Profile* profile = Profile::FromBrowserContext(wc->GetBrowserContext());
  if (!profile) {
    return nullptr;
  }

  auto* data = static_cast<GlicMediaIntegrationImpl*>(
      profile->GetUserData(kGlicMediaIntegrationKey));
  if (!data) {
    auto new_data = std::make_unique<GlicMediaIntegrationImpl>(profile);
    data = new_data.get();
    profile->SetUserData(kGlicMediaIntegrationKey, std::move(new_data));
  }

  return data;
}

}  // namespace glic
