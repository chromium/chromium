// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/media/glic_media_integration.h"

#include "base/supports_user_data.h"
#include "chrome/browser/accessibility/live_caption/live_caption_controller_factory.h"
#include "chrome/browser/glic/media/glic_media_context.h"
#include "chrome/browser/glic/media/glic_media_page_cache.h"
#include "chrome/browser/glic/media/media_transcript_provider_impl.h"
#include "chrome/browser/profiles/profile.h"
#include "components/live_caption/caption_controller_base.h"
#include "components/live_caption/caption_util.h"
#include "components/live_caption/live_caption_controller.h"
#include "components/live_caption/pref_names.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/proto/features/common_quality_data.pb.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/peer_connection_tracker_host_observer.h"
#include "content/public/browser/web_contents.h"

namespace {

constexpr char kGlicMediaIntegrationKey[] = "GlicMediaIntegration";

class GlicMediaPeerConnectionObserver
    : public content::PeerConnectionTrackerHostObserver {
 public:
  ~GlicMediaPeerConnectionObserver() override = default;

  void OnPeerConnectionAdded(
      content::GlobalRenderFrameHostId render_frame_host_id,
      int lid,
      base::ProcessId pid,
      const std::string& url,
      const std::string& rtc_configuration) override {
    auto* rfh = content::RenderFrameHost::FromID(render_frame_host_id);
    if (!rfh) {
      return;
    }

    auto* wc = content::WebContents::FromRenderFrameHost(rfh);
    if (!wc) {
      return;
    }

    // Attribute this to all frames in the WebContents.
    wc->ForEachRenderFrameHost([](content::RenderFrameHost* rfh) {
      if (auto* context =
              glic::GlicMediaContext::GetOrCreateForCurrentDocument(rfh)) {
        context->OnPeerConnectionAdded();
      }
    });
  }

  void OnPeerConnectionRemoved(
      content::GlobalRenderFrameHostId render_frame_host_id,
      int lid) override {
    auto* rfh = content::RenderFrameHost::FromID(render_frame_host_id);
    if (!rfh) {
      return;
    }

    auto* wc = content::WebContents::FromRenderFrameHost(rfh);
    if (!wc) {
      return;
    }

    // Attribute this to all frames in the WebContents.
    wc->ForEachRenderFrameHost([](content::RenderFrameHost* rfh) {
      if (auto* context =
              glic::GlicMediaContext::GetOrCreateForCurrentDocument(rfh)) {
        context->OnPeerConnectionRemoved();
      }
    });
  }
};

class GlicMediaIntegrationImpl : public glic::GlicMediaIntegration,
                                 public base::SupportsUserData::Data {
 public:
  explicit GlicMediaIntegrationImpl(Profile*);
  ~GlicMediaIntegrationImpl() override = default;

  // glic::GlicMediaIntegration:
  void AppendContext(
      content::WebContents* web_contents,
      optimization_guide::proto::ContentNode* context_root) override;
  void AppendContextForFrame(
      content::RenderFrameHost* rfh,
      optimization_guide::proto::ContentNode* context_root) override;
  void OnPeerConnectionAddedForTesting(content::RenderFrameHost*) override;
  void OnPeerConnectionRemovedForTesting(content::RenderFrameHost*) override;
  void SetExcludedOrigins(
      const std::vector<url::Origin>& excluded_origins) override;

  // GlicMediaIntegrationImpl:
  void OnContextUpdated(glic::GlicMediaContext* context);

  // Returns whether `web_contents` should be excluded by origin checks.  This
  // includes subframes.
  bool IsExcludedByOrigin(content::WebContents* web_contents);

 protected:
  raw_ptr<Profile> profile_;
  // Don't let the transcript grow unbounded.
  static constexpr size_t max_size_bytes_ = 20000;
  glic::GlicMediaPageCache page_cache_;

  std::unique_ptr<GlicMediaPeerConnectionObserver> rtc_observer_;
  std::vector<url::Origin> excluded_origins_;
};

class CaptionListenerImpl : public captions::CaptionControllerBase::Listener {
 public:
  explicit CaptionListenerImpl(Profile* profile) : profile_(profile) {}
  ~CaptionListenerImpl() override = default;

  bool OnTranscription(content::RenderFrameHost* rfh,
                       captions::CaptionBubbleContext*,
                       const media::SpeechRecognitionResult& result) override {
    if (!rfh) {
      return false;
    }

    auto* web_contents = content::WebContents::FromRenderFrameHost(rfh);
    auto* integration = static_cast<GlicMediaIntegrationImpl*>(
        glic::GlicMediaIntegration::GetFor(web_contents));
    CHECK(integration);

    if (integration->IsExcludedByOrigin(web_contents)) {
      return false;
    }

    bool continue_transcribing = false;
    if (auto* context =
            glic::GlicMediaContext::GetOrCreateForCurrentDocument(rfh)) {
      continue_transcribing = context->OnResult(result);
      integration->OnContextUpdated(context);
    }

    return continue_transcribing;
  }

  void OnAudioStreamEnd(content::RenderFrameHost*,
                        captions::CaptionBubbleContext*) override {}
  void OnLanguageIdentificationEvent(
      content::RenderFrameHost*,
      captions::CaptionBubbleContext*,
      const media::mojom::LanguageIdentificationEventPtr&) override {}

 private:
  raw_ptr<Profile> profile_;
};

GlicMediaIntegrationImpl::GlicMediaIntegrationImpl(Profile* profile)
    : profile_(profile),
      rtc_observer_(std::make_unique<GlicMediaPeerConnectionObserver>()) {
  auto* lc = captions::LiveCaptionControllerFactory::GetForProfile(profile_);
  lc->AddListener(std::make_unique<CaptionListenerImpl>(profile));

  // For now, enable the pref if we get this far.  Do this after getting the
  // Live Caption controller, since it resets the pref to false.
  profile->GetPrefs()->SetBoolean(prefs::kHeadlessCaptionEnabled, true);

  // Default to turning off for YT.
  std::vector<url::Origin> excluded_origins = {
      url::Origin::Create(GURL("https://www.youtube.com")),
      url::Origin::Create(GURL("http://www.youtube.com"))};
  SetExcludedOrigins(std::move(excluded_origins));
}

bool GlicMediaIntegrationImpl::IsExcludedByOrigin(
    content::WebContents* web_contents) {
  content::RenderFrameHost* rfh = web_contents->GetPrimaryMainFrame();
  // Walk the frame tree.
  bool exclude_this = false;
  const auto& excluded_origins = excluded_origins_;
  rfh->ForEachRenderFrameHostWithAction(
      [&exclude_this, &excluded_origins](content::RenderFrameHost* rfh) {
        auto& origin = rfh->GetLastCommittedOrigin();
        for (auto& excluded_origin : excluded_origins) {
          exclude_this |= origin == excluded_origin;
        }
        return exclude_this
                   ? content::RenderFrameHost::FrameIterationAction::kStop
                   : content::RenderFrameHost::FrameIterationAction::kContinue;
      });

  return exclude_this;
}

void GlicMediaIntegrationImpl::AppendContext(
    content::WebContents* web_contents,
    optimization_guide::proto::ContentNode* context_root) {
  if (!web_contents) {
    return;
  }
  if (base::FeatureList::IsEnabled(
          optimization_guide::features::kAnnotatedPageContentWithMediaData)) {
    return;
  }
  // Walk the tree and find a transcript.
  content::RenderFrameHost* rfh = nullptr;
  web_contents->ForEachRenderFrameHost([&rfh](content::RenderFrameHost* host) {
    auto* context = glic::GlicMediaContext::GetForCurrentDocument(host);
    if (!context) {
      return;
    }
    if (context->GetContext() != "") {
      rfh = host;
    }
  });
  if (rfh) {
    AppendContextForFrame(rfh, context_root);
  }
}

void GlicMediaIntegrationImpl::AppendContextForFrame(
    content::RenderFrameHost* rfh,
    optimization_guide::proto::ContentNode* context_root) {
  if (!rfh) {
    return;
  }
  context_root->mutable_content_attributes()->set_attribute_type(
      optimization_guide::proto::CONTENT_ATTRIBUTE_TEXT);

  auto* context = glic::GlicMediaContext::GetForCurrentDocument(rfh);
  std::string result;
  if (context != nullptr &&
      !IsExcludedByOrigin(content::WebContents::FromRenderFrameHost(rfh))) {
    result = context->GetContext();
  }

  if (result.length() == 0) {
    return;
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

void GlicMediaIntegrationImpl::OnPeerConnectionAddedForTesting(
    content::RenderFrameHost* rfh) {
  auto id = rfh->GetGlobalId();
  rtc_observer_->OnPeerConnectionAdded(id, /*lid=*/0, /*pid=*/{}, /*url=*/"",
                                       /*rtc_configuration=*/"");
}

void GlicMediaIntegrationImpl::OnPeerConnectionRemovedForTesting(
    content::RenderFrameHost* rfh) {
  auto id = rfh->GetGlobalId();
  rtc_observer_->OnPeerConnectionRemoved(id, /*lid=*/0);
}

void GlicMediaIntegrationImpl::SetExcludedOrigins(
    const std::vector<url::Origin>& excluded_origins) {
  excluded_origins_ = excluded_origins;
}

}  // namespace

namespace glic {

// static
GlicMediaIntegration* GlicMediaIntegration::GetFor(
    content::WebContents* web_contents) {
  // This should also check the pref, once it's not toggled automatically.
  // We'll want to install a pref listener, and possibly clean up if the pref
  // is switched off after construction.
  if (!web_contents || !captions::IsHeadlessCaptionFeatureSupported()) {
    return nullptr;
  }

  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
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

  if (!optimization_guide::MediaTranscriptProvider::GetFor(web_contents)) {
    optimization_guide::MediaTranscriptProvider::SetFor(
        web_contents, std::make_unique<glic::MediaTranscriptProviderImpl>());
  }

  return data;
}

}  // namespace glic
