// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/media/glic_media_integration.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "base/containers/flat_map.h"
#include "base/memory/raw_ref.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/no_destructor.h"
#include "base/scoped_observation.h"
#include "base/supports_user_data.h"
#include "chrome/browser/accessibility/live_caption/live_caption_controller_factory.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/media/glic_media_context.h"
#include "chrome/browser/glic/media/glic_media_page_cache.h"
#include "chrome/browser/glic/media/media_transcript_provider_impl.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"
#include "chrome/browser/media/webrtc/media_stream_capture_indicator.h"
#include "chrome/browser/picture_in_picture/picture_in_picture_window_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/speech/speech_recognition_client_browser_interface.h"
#include "chrome/browser/speech/speech_recognition_client_browser_interface_factory.h"
#include "components/live_caption/caption_controller_base.h"
#include "components/live_caption/caption_util.h"
#include "components/live_caption/live_caption_controller.h"
#include "components/live_caption/pref_names.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/proto/features/common_quality_data.pb.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/peer_connection_tracker_host_observer.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"

namespace {

constexpr char kGlicMediaIntegrationKey[] = "GlicMediaIntegration";
constexpr char kGlicMediaPeerConnectionTrackerKey[] =
    "GlicMediaPeerConnectionTracker";
constexpr char kGlicMediaTrackedWebContentsKey[] =
    "GlicMediaTrackedWebContents";

void NotifyAvailabilityChanged(Profile* profile) {
  if (!profile || profile->ShutdownStarted() ||
      !captions::IsHeadlessCaptionFeatureSupported() ||
      !profile->GetPrefs()->GetBoolean(prefs::kHeadlessCaptionEnabled)) {
    return;
  }
  if (auto* client_browser_interface =
          SpeechRecognitionClientBrowserInterfaceFactory::GetForProfileIfExists(
              profile)) {
    client_browser_interface->OnLiveCaptionAvailabilityChanged();
  }
}

class GlicMediaPeerConnectionTracker {
 public:
  static GlicMediaPeerConnectionTracker& GetInstance() {
    static base::NoDestructor<GlicMediaPeerConnectionTracker> instance;
    return *instance;
  }

  GlicMediaPeerConnectionTracker() = default;
  GlicMediaPeerConnectionTracker(const GlicMediaPeerConnectionTracker&) =
      delete;
  GlicMediaPeerConnectionTracker& operator=(
      const GlicMediaPeerConnectionTracker&) = delete;

  void AddProfileRegistration() {
    if (registration_count_++ == 0) {
      observer_ = std::make_unique<PeerConnectionObserver>(*this);
    }
  }

  void RemoveProfileRegistration() {
    CHECK_GT(registration_count_, 0u);
    if (--registration_count_ == 0) {
      observer_.reset();
      active_peer_connections_.clear();
    }
  }

  bool HasActivePeerConnection(content::WebContents* web_contents) const {
    if (!web_contents || web_contents->IsBeingDestroyed()) {
      return false;
    }
    return std::ranges::any_of(
        active_peer_connections_, [web_contents](const auto& entry) {
          if (entry.second.web_contents.get() != web_contents) {
            return false;
          }
          auto* rfh = content::RenderFrameHost::FromID(entry.first.first);
          return rfh && rfh->IsActive();
        });
  }

  void HandlePeerConnectionAdded(
      content::GlobalRenderFrameHostId render_frame_host_id,
      int lid) {
    auto* rfh = content::RenderFrameHost::FromID(render_frame_host_id);
    if (!rfh ||
        rfh->IsInLifecycleState(
            content::RenderFrameHost::LifecycleState::kPendingDeletion)) {
      return;
    }

    auto* wc = content::WebContents::FromRenderFrameHost(rfh);
    if (!wc || wc->IsBeingDestroyed()) {
      return;
    }

    Profile* profile = Profile::FromBrowserContext(wc->GetBrowserContext());
    if (!profile) {
      return;
    }

    active_peer_connections_[{render_frame_host_id, lid}] = {
        wc->GetWeakPtr(), profile->GetWeakPtr()};
    if (!wc->GetUserData(kGlicMediaTrackedWebContentsKey)) {
      wc->SetUserData(kGlicMediaTrackedWebContentsKey,
                      std::make_unique<TrackedWebContentsObserver>(
                          *this, wc, profile->GetWeakPtr()));
    }

    if (rfh->IsActive()) {
      NotifyAvailabilityChanged(profile);
    }
  }

  void HandlePeerConnectionRemoved(
      content::GlobalRenderFrameHostId render_frame_host_id,
      int lid) {
    auto it = active_peer_connections_.find({render_frame_host_id, lid});
    if (it == active_peer_connections_.end()) {
      return;
    }

    base::WeakPtr<Profile> profile = it->second.profile;
    active_peer_connections_.erase(it);
    CleanupExpiredEntries();

    auto* rfh = content::RenderFrameHost::FromID(render_frame_host_id);
    if (profile && (!rfh || rfh->IsActive())) {
      NotifyAvailabilityChanged(profile.get());
    }
  }

 private:
  using PeerConnectionKey = std::pair<content::GlobalRenderFrameHostId, int>;

  struct TrackedPeerConnection {
    base::WeakPtr<content::WebContents> web_contents;
    base::WeakPtr<Profile> profile;
  };

  class PeerConnectionObserver
      : public content::PeerConnectionTrackerHostObserver {
   public:
    explicit PeerConnectionObserver(GlicMediaPeerConnectionTracker& tracker)
        : tracker_(tracker) {}
    ~PeerConnectionObserver() override = default;

    void OnPeerConnectionAdded(
        content::GlobalRenderFrameHostId render_frame_host_id,
        int lid,
        base::ProcessId /*pid*/,
        const std::string& /*url*/,
        const std::string& /*rtc_configuration*/) override {
      tracker_->HandlePeerConnectionAdded(render_frame_host_id, lid);
    }

    void OnPeerConnectionRemoved(
        content::GlobalRenderFrameHostId render_frame_host_id,
        int lid) override {
      tracker_->HandlePeerConnectionRemoved(render_frame_host_id, lid);
    }

   private:
    raw_ref<GlicMediaPeerConnectionTracker> tracker_;
  };

  class TrackedWebContentsObserver : public base::SupportsUserData::Data,
                                     public content::WebContentsObserver {
   public:
    TrackedWebContentsObserver(GlicMediaPeerConnectionTracker& tracker,
                               content::WebContents* web_contents,
                               base::WeakPtr<Profile> profile)
        : content::WebContentsObserver(web_contents),
          tracker_(tracker),
          profile_(std::move(profile)) {}

    ~TrackedWebContentsObserver() override {
      const size_t erased = tracker_->CleanupExpiredEntries();
      if (erased > 0 && profile_) {
        NotifyAvailabilityChanged(profile_.get());
      }
    }

    void RenderFrameDeleted(content::RenderFrameHost* rfh) override {
      const content::GlobalRenderFrameHostId rfh_id = rfh->GetGlobalId();
      const size_t erased = base::EraseIf(
          tracker_->active_peer_connections_,
          [&rfh_id](const auto& entry) { return entry.first.first == rfh_id; });
      if (erased > 0 && profile_) {
        NotifyAvailabilityChanged(profile_.get());
      }
    }

    void RenderFrameHostStateChanged(
        content::RenderFrameHost* rfh,
        content::RenderFrameHost::LifecycleState old_state,
        content::RenderFrameHost::LifecycleState new_state) override {
      const content::GlobalRenderFrameHostId rfh_id = rfh->GetGlobalId();
      if (new_state ==
          content::RenderFrameHost::LifecycleState::kPendingDeletion) {
        const size_t erased = base::EraseIf(
            tracker_->active_peer_connections_, [&rfh_id](const auto& entry) {
              return entry.first.first == rfh_id;
            });
        if (erased > 0 && profile_) {
          NotifyAvailabilityChanged(profile_.get());
        }
        return;
      }

      const bool active_changed =
          (old_state == content::RenderFrameHost::LifecycleState::kActive) !=
          (new_state == content::RenderFrameHost::LifecycleState::kActive);
      if (active_changed && profile_ &&
          std::ranges::any_of(tracker_->active_peer_connections_,
                              [&rfh_id](const auto& entry) {
                                return entry.first.first == rfh_id;
                              })) {
        NotifyAvailabilityChanged(profile_.get());
      }
    }

   private:
    raw_ref<GlicMediaPeerConnectionTracker> tracker_;
    base::WeakPtr<Profile> profile_;
  };

  size_t CleanupExpiredEntries() {
    return base::EraseIf(active_peer_connections_, [](const auto& entry) {
      return !entry.second.web_contents;
    });
  }

  size_t registration_count_ = 0;
  std::unique_ptr<PeerConnectionObserver> observer_;
  base::flat_map<PeerConnectionKey, TrackedPeerConnection>
      active_peer_connections_;
};

class GlicMediaPeerConnectionTrackerRegistration
    : public base::SupportsUserData::Data {
 public:
  GlicMediaPeerConnectionTrackerRegistration() {
    GlicMediaPeerConnectionTracker::GetInstance().AddProfileRegistration();
  }
  ~GlicMediaPeerConnectionTrackerRegistration() override {
    GlicMediaPeerConnectionTracker::GetInstance().RemoveProfileRegistration();
  }
};

class GlicMediaIntegrationImpl
    : public glic::GlicMediaIntegration,
      public base::SupportsUserData::Data,
      public MediaStreamCaptureIndicator::Observer,
      public PictureInPictureWindowManager::Observer {
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
  bool IsInitializedForTesting() const override;  // IN-TEST

  // MediaStreamCaptureIndicator::Observer:
  void OnIsCapturingVideoChanged(content::WebContents* web_contents,
                                 bool is_capturing_video) override;
  void OnIsCapturingAudioChanged(content::WebContents* web_contents,
                                 bool is_capturing_audio) override;

  // PictureInPictureWindowManager::Observer:
  void OnEnterPictureInPicture() override;
  void OnExitPictureInPicture() override;

  // GlicMediaIntegrationImpl:
  void OnContextUpdated(glic::GlicMediaContext* context);
  void OnListenerDestroyed();

  // Returns whether `web_contents` should be excluded by origin checks.  This
  // includes subframes.
  bool IsExcludedByOrigin(content::WebContents* web_contents);

 protected:
  void Initialize();
  void OnConsentChanged();

  raw_ptr<Profile> profile_;
  // Don't let the transcript grow unbounded.
  static constexpr size_t max_size_bytes_ = 20000;
  glic::GlicMediaPageCache page_cache_;

  bool initialized_ = false;
  base::WeakPtr<Profile> active_document_pip_profile_;
  scoped_refptr<MediaStreamCaptureIndicator> capture_indicator_;
  base::ScopedObservation<MediaStreamCaptureIndicator,
                          MediaStreamCaptureIndicator::Observer>
      capture_observation_{this};
  base::ScopedObservation<PictureInPictureWindowManager,
                          PictureInPictureWindowManager::Observer>
      pip_observation_{this};
  std::vector<url::Origin> excluded_origins_;
  base::CallbackListSubscription subscription_;

 private:
  bool MatchesProfile(Profile* target_profile) const;
  void OnPrefChanged();

  PrefChangeRegistrar pref_change_registrar_;
  raw_ptr<captions::CaptionControllerBase::Listener> listener_ = nullptr;

  base::WeakPtrFactory<GlicMediaIntegrationImpl> weak_ptr_factory_{this};
};

class CaptionListenerImpl : public captions::CaptionControllerBase::Listener {
 public:
  explicit CaptionListenerImpl(
      base::WeakPtr<GlicMediaIntegrationImpl> integration)
      : integration_(integration) {}
  ~CaptionListenerImpl() override {
    if (integration_) {
      integration_->OnListenerDestroyed();
    }
  }

  bool IsTranscriptionAllowed(content::RenderFrameHost* rfh) override {
    if (!rfh || !integration_) {
      return false;
    }

    auto* web_contents = content::WebContents::FromRenderFrameHost(rfh);
    if (!web_contents || integration_->IsExcludedByOrigin(web_contents)) {
      return false;
    }

    // Disallow transcription if the document is excluded (e.g. due to active
    // WebRTC peer connections or media capture) to avoid creating unnecessary
    // SODA instances.
    return !glic::GlicMediaContext::IsExcludedFromTranscript(rfh);
  }

  bool OnTranscription(content::RenderFrameHost* rfh,
                       captions::CaptionBubbleContext*,
                       const media::SpeechRecognitionResult& result) override {
    if (!IsTranscriptionAllowed(rfh)) {
      return false;
    }

    auto* web_contents = content::WebContents::FromRenderFrameHost(rfh);
    auto* integration = static_cast<GlicMediaIntegrationImpl*>(
        glic::GlicMediaIntegration::GetFor(web_contents));
    CHECK(integration);

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
  base::WeakPtr<GlicMediaIntegrationImpl> integration_;
};

GlicMediaIntegrationImpl::GlicMediaIntegrationImpl(Profile* profile)
    : profile_(profile) {
  if (glic::GlicEnabling::HasConsentedForProfile(profile_)) {
    Initialize();
  } else {
    auto* keyed_service = glic::GlicKeyedService::Get(profile);
    if (keyed_service) {
      subscription_ = keyed_service->enabling().RegisterOnConsentChanged(
          base::BindRepeating(&GlicMediaIntegrationImpl::OnConsentChanged,
                              base::Unretained(this)));
    }
  }
}

void GlicMediaIntegrationImpl::Initialize() {
  // Initialization should only happen once.
  CHECK(!initialized_);
  initialized_ = true;

  if (auto* dispatcher = MediaCaptureDevicesDispatcher::GetInstance()) {
    capture_indicator_ = dispatcher->GetMediaStreamCaptureIndicator();
    if (capture_indicator_) {
      capture_observation_.Observe(capture_indicator_.get());
    }
  }

  if (auto* pip_window_manager = PictureInPictureWindowManager::GetInstance()) {
    pip_observation_.Observe(pip_window_manager);
    auto* opener_wc = pip_window_manager->GetWebContents();
    Profile* opener_profile =
        opener_wc ? Profile::FromBrowserContext(opener_wc->GetBrowserContext())
                  : nullptr;
    if (opener_profile && pip_window_manager->GetChildWebContents() &&
        MatchesProfile(opener_profile)) {
      active_document_pip_profile_ = opener_profile->GetWeakPtr();
    }
  }

  // Default to turning off for YT. Set this before enabling the headless
  // caption pref so that any synchronous availability notifications already
  // see the excluded origins.
  std::vector<url::Origin> excluded_origins = {
      url::Origin::Create(GURL("https://www.youtube.com")),
      url::Origin::Create(GURL("http://www.youtube.com"))};
  SetExcludedOrigins(std::move(excluded_origins));

  pref_change_registrar_.Init(profile_->GetPrefs());
  pref_change_registrar_.Add(
      glic::prefs::kGlicMediaUnderstandingEnabled,
      base::BindRepeating(&GlicMediaIntegrationImpl::OnPrefChanged,
                          base::Unretained(this)));

  OnPrefChanged();

  // For now, enable the pref if we get this far.  Do this after getting the
  // Live Caption controller, since it resets the pref to false.
  profile_->GetPrefs()->SetBoolean(prefs::kHeadlessCaptionEnabled, true);
}

bool GlicMediaIntegrationImpl::MatchesProfile(Profile* target_profile) const {
  return target_profile && (target_profile == profile_ ||
                            (!profile_->IsOffTheRecord() &&
                             target_profile->GetOriginalProfile() == profile_));
}

void GlicMediaIntegrationImpl::OnPrefChanged() {
  bool enabled = profile_->GetPrefs()->GetBoolean(
      glic::prefs::kGlicMediaUnderstandingEnabled);
  auto* lc = captions::LiveCaptionControllerFactory::GetForProfile(profile_);
  if (enabled) {
    if (!listener_) {
      auto listener =
          std::make_unique<CaptionListenerImpl>(weak_ptr_factory_.GetWeakPtr());
      listener_ = listener.get();
      lc->AddListener(std::move(listener));
    }
  } else {
    if (listener_) {
      lc->RemoveSoon(listener_);
      listener_ = nullptr;

      // Discard all transcripts when disabled.
      for (base::UnsafeLinkNode<glic::GlicMediaPageCache::Entry>* node =
               page_cache_.head();
           node != page_cache_.end(); node = node->next()) {
        auto* entry = static_cast<glic::GlicMediaPageCache::Entry*>(node);
        auto* context = static_cast<glic::GlicMediaContext*>(entry);
        context->ClearAllTranscripts();
      }
    }
  }
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

void GlicMediaIntegrationImpl::OnConsentChanged() {
  if (glic::GlicEnabling::HasConsentedForProfile(profile_)) {
    Initialize();
    // Unsubscribe.
    subscription_ = {};
  }
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
    if (context && context->HasTranscriptChunks()) {
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
  if (IsExcludedByOrigin(content::WebContents::FromRenderFrameHost(rfh))) {
    return;
  }

  auto* context = glic::GlicMediaContext::GetForCurrentDocument(rfh);
  if (!context) {
    return;
  }

  auto chunks = context->GetTranscriptChunks();
  if (chunks.empty()) {
    return;
  }
  std::vector<std::string_view> pieces;
  pieces.reserve(chunks.size());
  for (const auto& chunk : chunks) {
    pieces.push_back(chunk.text);
  }

  std::string result = base::JoinString(pieces, "");

  // Trim to `max_size_bytes_`.  Note that we should utf8-trim.
  if (size_t result_size = result.length()) {
    if (result_size > max_size_bytes_) {
      // Remove the beginning of the result, leaving the end.
      size_t start_index = result_size - max_size_bytes_;
      // Ensure we don't cut in the middle of a UTF-8 multi-byte character.
      // UTF-8 continuation bytes start with 10xxxxxx (0x80 to 0xBF).
      while (start_index < result_size &&
             (static_cast<unsigned char>(result[start_index]) & 0xC0) == 0x80) {
        start_index++;
      }
      result = std::move(result).substr(start_index);
    }
  }

  // Include the entire context in one node.  This could be split into multiple
  // nodes too.
  auto* content_attributes = context_root->mutable_content_attributes();
  content_attributes->set_attribute_type(
      optimization_guide::proto::CONTENT_ATTRIBUTE_TEXT);
  content_attributes->mutable_text_data()->set_text_content(std::move(result));
}

void GlicMediaIntegrationImpl::OnIsCapturingVideoChanged(
    content::WebContents* web_contents,
    bool /*is_capturing_video*/) {
  if (!web_contents) {
    return;
  }
  Profile* target_profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  if (MatchesProfile(target_profile)) {
    NotifyAvailabilityChanged(target_profile);
  }
}

void GlicMediaIntegrationImpl::OnIsCapturingAudioChanged(
    content::WebContents* web_contents,
    bool /*is_capturing_audio*/) {
  if (!web_contents) {
    return;
  }
  Profile* target_profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  if (MatchesProfile(target_profile)) {
    NotifyAvailabilityChanged(target_profile);
  }
}

void GlicMediaIntegrationImpl::OnEnterPictureInPicture() {
  active_document_pip_profile_.reset();
  auto* pip_window_manager = PictureInPictureWindowManager::GetInstance();
  if (!pip_window_manager) {
    return;
  }
  auto* opener_wc = pip_window_manager->GetWebContents();
  Profile* opener_profile =
      opener_wc ? Profile::FromBrowserContext(opener_wc->GetBrowserContext())
                : nullptr;
  if (opener_profile && pip_window_manager->GetChildWebContents() &&
      MatchesProfile(opener_profile)) {
    active_document_pip_profile_ = opener_profile->GetWeakPtr();
    NotifyAvailabilityChanged(opener_profile);
  }
}

void GlicMediaIntegrationImpl::OnExitPictureInPicture() {
  if (Profile* pip_profile = active_document_pip_profile_.get()) {
    active_document_pip_profile_.reset();
    NotifyAvailabilityChanged(pip_profile);
  } else {
    active_document_pip_profile_.reset();
  }
}

void GlicMediaIntegrationImpl::OnContextUpdated(
    glic::GlicMediaContext* context) {
  page_cache_.PlaceAtFront(context);
}

void GlicMediaIntegrationImpl::OnListenerDestroyed() {
  listener_ = nullptr;
}

void GlicMediaIntegrationImpl::OnPeerConnectionAddedForTesting(
    content::RenderFrameHost* rfh) {
  SimulatePeerConnectionAddedForTesting(rfh);  // IN-TEST
}

void GlicMediaIntegrationImpl::OnPeerConnectionRemovedForTesting(
    content::RenderFrameHost* rfh) {
  SimulatePeerConnectionRemovedForTesting(rfh);  // IN-TEST
}

void GlicMediaIntegrationImpl::SetExcludedOrigins(
    const std::vector<url::Origin>& excluded_origins) {
  excluded_origins_ = excluded_origins;
}

bool GlicMediaIntegrationImpl::IsInitializedForTesting() const {
  return initialized_;
}

}  // namespace

namespace glic {

// static
void GlicMediaIntegration::EnsurePeerConnectionTrackerInitialized(
    Profile* profile) {
  if (!profile || !captions::IsHeadlessCaptionFeatureSupported()) {
    return;
  }
  if (!profile->GetUserData(kGlicMediaPeerConnectionTrackerKey)) {
    profile->SetUserData(
        kGlicMediaPeerConnectionTrackerKey,
        std::make_unique<GlicMediaPeerConnectionTrackerRegistration>());
  }
}

// static
bool GlicMediaIntegration::HasActivePeerConnection(
    content::WebContents* web_contents) {
  return GlicMediaPeerConnectionTracker::GetInstance().HasActivePeerConnection(
      web_contents);
}

// static
void GlicMediaIntegration::SimulatePeerConnectionAddedForTesting(
    content::RenderFrameHost* rfh,
    int lid) {
  if (!rfh) {
    return;
  }
  if (auto* browser_context = rfh->GetBrowserContext();
      browser_context &&
      !browser_context->GetUserData(kGlicMediaPeerConnectionTrackerKey)) {
    browser_context->SetUserData(
        kGlicMediaPeerConnectionTrackerKey,
        std::make_unique<GlicMediaPeerConnectionTrackerRegistration>());
  }
  GlicMediaPeerConnectionTracker::GetInstance().HandlePeerConnectionAdded(
      rfh->GetGlobalId(), lid);
}

// static
void GlicMediaIntegration::SimulatePeerConnectionRemovedForTesting(
    content::RenderFrameHost* rfh,
    int lid) {
  if (!rfh) {
    return;
  }
  GlicMediaPeerConnectionTracker::GetInstance().HandlePeerConnectionRemoved(
      rfh->GetGlobalId(), lid);
}

// static
GlicMediaIntegration* GlicMediaIntegration::GetFor(
    content::WebContents* web_contents) {
  if (!web_contents) {
    return nullptr;
  }

  auto* integration =
      GetFor(Profile::FromBrowserContext(web_contents->GetBrowserContext()));
  if (!integration) {
    return nullptr;
  }

  if (!optimization_guide::MediaTranscriptProvider::GetFor(web_contents)) {
    optimization_guide::MediaTranscriptProvider::SetFor(
        web_contents, std::make_unique<glic::MediaTranscriptProviderImpl>());
  }

  return integration;
}

GlicMediaIntegration* GlicMediaIntegration::GetFor(Profile* profile) {
  if (!profile) {
    return nullptr;
  }

  // This should also check the pref, once it's not toggled automatically.
  // We'll want to install a pref listener, and possibly clean up if the pref
  // is switched off after construction.
  if (!captions::IsHeadlessCaptionFeatureSupported()) {
    return nullptr;
  }

  EnsurePeerConnectionTrackerInitialized(profile);

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
