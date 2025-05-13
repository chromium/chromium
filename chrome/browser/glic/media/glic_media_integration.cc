// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/media/glic_media_integration.h"

#include "base/feature_list.h"
#include "base/no_destructor.h"
#include "base/supports_user_data.h"
#include "chrome/browser/accessibility/live_caption/live_caption_controller_factory.h"
#include "chrome/browser/glic/media/glic_media_context.h"
#include "chrome/browser/glic/media/glic_media_page_cache.h"
#include "chrome/browser/profiles/profile.h"
#include "components/live_caption/caption_controller_base.h"
#include "components/live_caption/caption_util.h"
#include "components/live_caption/live_caption_controller.h"
#include "components/live_caption/pref_names.h"
#include "components/optimization_guide/content/browser/page_content_proto_provider.h"
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

  // glic::GlicMediaIntegration
  void AppendContext(
      content::WebContents* web_contents,
      optimization_guide::proto::ContentNode* context_root) override;

  void OnContextUpdated(glic::GlicMediaContext* context);

 protected:
  raw_ptr<Profile> profile_;
  // Don't let the transcript grow unbounded.
  static constexpr size_t max_size_bytes_ = 20000;
  glic::GlicMediaPageCache page_cache_;
};

class CaptionListenerImpl : public captions::CaptionControllerBase::Listener {
 public:
  explicit CaptionListenerImpl(Profile* profile) : profile_(profile) {}
  ~CaptionListenerImpl() override = default;

  bool OnTranscription(content::WebContents* web_contents,
                       captions::CaptionBubbleContext*,
                       const media::SpeechRecognitionResult& result) override {
    if (auto* context = glic::GlicMediaContext::GetOrCreateFor(web_contents)) {
      context->OnResult(result);
      static_cast<GlicMediaIntegrationImpl*>(
          glic::GlicMediaIntegration::GetFor(web_contents))
          ->OnContextUpdated(context);
    }

    return true;
  }

  void OnAudioStreamEnd(content::WebContents*,
                        captions::CaptionBubbleContext*) override {}
  void OnLanguageIdentificationEvent(
      content::WebContents*,
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

void GlicMediaIntegrationImpl::AppendContext(
    content::WebContents* web_contents,
    optimization_guide::proto::ContentNode* context_root) {
  context_root->mutable_content_attributes()->set_attribute_type(
      optimization_guide::proto::CONTENT_ATTRIBUTE_TEXT);

  auto* context = glic::GlicMediaContext::GetIfExistsFor(web_contents);
  std::string result;
  if (context != nullptr) {
    result = context->GetContext();
  }

  // Provide a default.
  if (result.length() == 0) {
    result = "There is no transcript available.";
  }

  // Trim to `max_size_bytes_`.  Note that we should utf8-trim.
  if (size_t result_size = result.length()) {
    if (result_size > max_size_bytes_) {
      // Remove the beginning of the result, leaving the end.
      result = result.substr(result_size - max_size_bytes_);
    }
  }

  // Include the entire context in one node.  This could be split into multiple
  // nodes too.
  context_root->mutable_content_attributes()
      ->mutable_text_data()
      ->set_text_content(std::move(result));
}

void GlicMediaIntegrationImpl::OnContextUpdated(
    glic::GlicMediaContext* context) {
  page_cache_.PlaceAtFront(context);
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
