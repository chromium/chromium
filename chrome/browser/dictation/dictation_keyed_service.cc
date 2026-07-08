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
#include "chrome/browser/dictation/onboarding_manager.h"
#include "chrome/browser/dictation/session_controller.h"
#include "chrome/browser/dictation/session_ui_impl.h"
#include "chrome/browser/dictation/target.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"

namespace dictation {

namespace {
constexpr int kVoiceTypingSettingsDisabled = 2;
}  // namespace

// static
DictationKeyedService* DictationKeyedService::Get(
    content::BrowserContext* context) {
  return DictationKeyedServiceFactory::GetDictationKeyedService(context);
}

DictationKeyedService::SessionState::SessionState(
    SessionControllerDelegate& delegate,
    base::WeakPtr<BrowserWindowInterface> window)
    : controller_(delegate), window_(window) {}

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
  if (!session_->window_) {
    return nullptr;
  }

  return std::make_unique<SessionUiImpl>(*session_->window_, controller);
}

void DictationKeyedService::StartSession(BrowserWindowInterface& window,
                                         const TargetId& target_id,
                                         const std::string& selected_text) {
  CHECK(IsEnabled());
  CHECK(!session_);

  if (onboarding_manager_.ShowOnboardingIfNeeded(window, target_id,
                                                 selected_text)) {
    // If onboarding is shown, it will call StartSession again if needed.
    return;
  }

  session_.emplace(*this, window.GetWeakPtr());

  session_->controller_.Initialize();

  session_->controller_.StartDictationStream(target_id, selected_text);
}

void DictationKeyedService::EndSession() {
  session_.reset();
}

bool DictationKeyedService::ShouldShowContextMenuItem() const {
  if (!IsEnabled()) {
    return false;
  }
  return !session_;
}

void DictationKeyedService::ContextMenuHandler(
    content::RenderFrameHost& rfh,
    const std::u16string& selected_text) {
  // Policy could have changed to disabled while the context menu was open.
  if (!IsEnabled()) {
    return;
  }

  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(&rfh);
  if (!web_contents) {
    return;
  }

  tabs::TabInterface* tab =
      tabs::TabInterface::MaybeGetFromContents(web_contents);
  if (!tab) {
    return;
  }

  BrowserWindowInterface* window = tab->GetBrowserWindowInterface();
  if (!window) {
    return;
  }

  // TODO(crbug.com/525856380): Handle changes to the focused element. Identify
  // the targeted element for the dictation Target.
  StartSession(*window, TargetId{rfh.GetWeakDocumentPtr()},
               base::UTF16ToUTF8(selected_text));
}

bool DictationKeyedService::IsEnabled() const {
  CHECK(profile_);
  bool disabled_by_policy =
      profile_->GetPrefs()->GetInteger(prefs::kVoiceTypingSettings) ==
      kVoiceTypingSettingsDisabled;

  // Until the connector extension is available consider the feature disabled.
  return !connector_extension_.IsPending() && !disabled_by_policy;
}

void DictationKeyedService::OnPrefChanged() {
  if (!IsEnabled()) {
    EndSession();
  }
}

}  // namespace dictation
