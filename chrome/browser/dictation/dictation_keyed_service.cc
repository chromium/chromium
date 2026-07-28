// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dictation/dictation_keyed_service.h"

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/dictation/connector_component_extension.h"
#include "chrome/browser/dictation/dictation_keyed_service_factory.h"
#include "chrome/browser/dictation/features.h"
#include "chrome/browser/dictation/listener_stream_provider.h"
#include "chrome/browser/dictation/logging.h"
#include "chrome/browser/dictation/metrics.h"
#include "chrome/browser/dictation/onboarding_manager.h"
#include "chrome/browser/dictation/session_controller.h"
#include "chrome/browser/dictation/session_ui_impl.h"
#include "chrome/browser/dictation/target.h"
#include "chrome/browser/glic/host/guest_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/global_browser_collection.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"

namespace dictation {

namespace {
constexpr int kVoiceTypingSettingsDisabled = 2;

tabs::TabInterface* GetActiveTabFromGlic(content::WebContents* web_contents) {
  if (!glic::IsGlicGuest(web_contents)) {
    return nullptr;
  }

  content::WebContents* outermost_web_contents =
      web_contents->GetOutermostWebContents();
  gfx::NativeWindow native_window =
      outermost_web_contents->GetTopLevelNativeWindow();
  BrowserWindowInterface* browser =
      GlobalBrowserCollection::GetInstance()->FindBrowserWithWindow(
          native_window);
  return browser ? browser->GetActiveTabInterface() : nullptr;
}

tabs::TabInterface* GetTabFromTargetId(
    const content::GlobalDOMNodeId& target_id) {
  content::RenderFrameHost* rfh = target_id.document.AsRenderFrameHostIfValid();
  if (!rfh) {
    return nullptr;
  }

  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(rfh);
  if (!web_contents) {
    return nullptr;
  }

  // Use normal tab lookup first
  if (auto* tab = tabs::TabInterface::MaybeGetFromContents(web_contents)) {
    return tab;
  }

  // If the Glic side panel is being targeted, then associate the session with
  // the active tab of the window.
  if (tabs::TabInterface* tab = GetActiveTabFromGlic(web_contents)) {
    return tab;
  }

  return nullptr;
}

}  // namespace

// static
DictationKeyedService* DictationKeyedService::Get(
    content::BrowserContext* context) {
  return DictationKeyedServiceFactory::GetDictationKeyedService(context);
}

DictationKeyedService::SessionState::SessionState(
    SessionControllerDelegate& delegate,
    const TargetDetails& target_details)
    : controller_(delegate), target_details_(target_details) {}

DictationKeyedService::SessionState::~SessionState() = default;

DictationKeyedService::DictationKeyedService(Profile* profile)
    : profile_(profile),
      connector_extension_(profile),
      onboarding_manager_(*this, *profile->GetPrefs()) {
  CHECK(base::FeatureList::IsEnabled(kDictation));
  pref_change_registrar_.Init(profile_->GetPrefs());
  pref_change_registrar_.Add(
      prefs::kVoiceTypingSettings,
      base::BindRepeating(&DictationKeyedService::OnPrefChanged,
                          base::Unretained(this)));
  // `kDictation` is implicitly enabled by virtue of DictationKeyedService being
  // created (`CHECK`ed above).
  bool disabled_by_policy =
      profile_->GetPrefs()->GetInteger(prefs::kVoiceTypingSettings) ==
      kVoiceTypingSettingsDisabled;
  RecordDictationIsEnabledOnProfileInit(!disabled_by_policy);
}

DictationKeyedService::~DictationKeyedService() = default;

void DictationKeyedService::Shutdown() {
  EndSession();
}

std::unique_ptr<StreamProvider> DictationKeyedService::CreateStreamProvider(
    SessionController& controller) const {
  return std::make_unique<ListenerStreamProvider>(profile_, controller);
}

std::unique_ptr<SessionUi> DictationKeyedService::CreateUi(
    SessionController& controller) const {
  CHECK(session_);
  tabs::TabInterface* tab =
      GetTabFromTargetId(session_->target_details_.target_id);

  // We must have a tab since this is called synchronously from session
  // creation.
  CHECK(tab);

  return std::make_unique<SessionUiImpl>(*tab, controller);
}

void DictationKeyedService::StartSession(
    tabs::TabInterface& tab,
    const TargetDetails& target_details,
    DictationSessionEntryPoint entry_point) {
  CHECK(IsEnabledAndReady());
  CHECK(!session_);

  if (onboarding_manager_.ShowOnboardingIfNeeded(tab, target_details,
                                                 entry_point)) {
    // If onboarding is shown, it will call StartSession again if needed.
    return;
  }

  RecordDictationSessionStartSource(entry_point);

  session_.emplace(*this, target_details);

  session_->controller_.ResetUi();

  session_->controller_.StartDictationStream(
      target_details, DictationStreamStartTrigger::kSessionStart);
}

void DictationKeyedService::EndSession() {
  session_.reset();
}

bool DictationKeyedService::ShouldShowContextMenuItem() const {
  return IsEnabledAndReady();
}

void DictationKeyedService::ContextMenuHandler(
    const TargetDetails& target_details) {
  // Policy could have changed to disabled while the context menu was open.
  if (!IsEnabledAndReady()) {
    return;
  }

  tabs::TabInterface* tab = GetTabFromTargetId(target_details.target_id);
  if (!tab) {
    return;
  }

  if (!session_) {
    VT_LOG() << "Starting new session";
    StartSession(*tab, target_details,
                 DictationSessionEntryPoint::kContextMenu);
  } else {
    if (session_->controller_.attached_stream_provider()) {
      session_->controller_.EndDictationStream();
    }

    tabs::TabInterface* old_tab =
        GetTabFromTargetId(session_->target_details_.target_id);

    session_->target_details_ = target_details;

    // If the menu is triggered from a new tab, we need to re-create the UI in
    // that tab (removing it from the old tab).
    if (tab != old_tab) {
      VT_LOG() << "Moving session to new tab: " << tab;
      session_->controller_.ResetUi();
    }

    VT_LOG() << "Starting in existing session";
    session_->controller_.StartDictationStream(
        target_details,
        DictationStreamStartTrigger::kContextMenuExistingSession);
  }
}

bool DictationKeyedService::IsEnabledAndReady() const {
  CHECK(profile_);
  bool disabled_by_policy =
      profile_->GetPrefs()->GetInteger(prefs::kVoiceTypingSettings) ==
      kVoiceTypingSettingsDisabled;

  // Until the connector extension is available consider the feature disabled.
  return !connector_extension_.IsPending() && !disabled_by_policy;
}

void DictationKeyedService::OnPrefChanged() {
  if (!IsEnabledAndReady()) {
    EndSession();
  }
}

void DictationKeyedService::UpdateAudioLevel(float audio_level) {
  if (session_) {
    session_->controller_.UpdateAudioLevel(audio_level);
  }
}

}  // namespace dictation
