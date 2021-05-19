// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/accessibility/caption_controller.h"

#include <memory>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/accessibility/live_caption_speech_recognition_host.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/caption_bubble_controller.h"
#include "chrome/common/pref_names.h"
#include "components/live_caption/caption_util.h"
#include "components/live_caption/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/soda/constants.h"
#include "components/soda/soda_installer.h"
#include "components/sync_preferences/pref_service_syncable.h"
#include "content/public/browser/browser_accessibility_state.h"
#include "media/base/media_switches.h"
#include "ui/native_theme/native_theme.h"

namespace {

const char* const kCaptionStylePrefsToObserve[] = {
    prefs::kAccessibilityCaptionsTextSize,
    prefs::kAccessibilityCaptionsTextFont,
    prefs::kAccessibilityCaptionsTextColor,
    prefs::kAccessibilityCaptionsTextOpacity,
    prefs::kAccessibilityCaptionsBackgroundColor,
    prefs::kAccessibilityCaptionsTextShadow,
    prefs::kAccessibilityCaptionsBackgroundOpacity};

}  // namespace

namespace captions {

CaptionController::CaptionController(PrefService* profile_prefs)
    : profile_prefs_(profile_prefs) {}

CaptionController::~CaptionController() {
  if (enabled_) {
    enabled_ = false;
    StopLiveCaption();
  }
}

// static
void CaptionController::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterBooleanPref(
      prefs::kLiveCaptionEnabled, false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);

  // Initially default the language to en-US.
  registry->RegisterStringPref(prefs::kLiveCaptionLanguageCode, "en-US",
                               user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
}

void CaptionController::Init() {
  base::UmaHistogramBoolean("Accessibility.LiveCaption.FeatureEnabled",
                            media::IsLiveCaptionFeatureEnabled());

  // Hidden behind a feature flag.
  if (!media::IsLiveCaptionFeatureEnabled())
    return;

  base::UmaHistogramBoolean(
      "Accessibility.LiveCaption.UseSodaForLiveCaption",
      base::FeatureList::IsEnabled(media::kUseSodaForLiveCaption));
  pref_change_registrar_ = std::make_unique<PrefChangeRegistrar>();
  pref_change_registrar_->Init(profile_prefs_);
  auto* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line &&
      command_line->HasSwitch(switches::kEnableLiveCaptionPrefForTesting)) {
    profile_prefs_->SetBoolean(prefs::kLiveCaptionEnabled, true);
  }

  pref_change_registrar_->Add(
      prefs::kLiveCaptionEnabled,
      base::BindRepeating(&CaptionController::OnLiveCaptionEnabledChanged,
                          base::Unretained(this)));
  pref_change_registrar_->Add(
      prefs::kLiveCaptionLanguageCode,
      base::BindRepeating(&CaptionController::OnLiveCaptionLanguageChanged,
                          base::Unretained(this)));

  enabled_ = IsLiveCaptionEnabled();
  if (enabled_) {
    StartLiveCaption();
  } else {
    StopLiveCaption();
  }

  // BrowserAccessibilityState can outlive |this|, use WeakPtr to ensure that
  // callback is not called on destroyed object.
  content::BrowserAccessibilityState::GetInstance()
      ->AddUIThreadHistogramCallback(base::BindOnce(
          &CaptionController::UpdateAccessibilityCaptionHistograms,
          weak_ptr_factory_.GetWeakPtr()));
}

void CaptionController::OnLiveCaptionEnabledChanged() {
  bool enabled = IsLiveCaptionEnabled();
  if (enabled == enabled_)
    return;
  enabled_ = enabled;

  if (enabled) {
    StartLiveCaption();
  } else {
    StopLiveCaption();
    speech::SodaInstaller::GetInstance()->SetUninstallTimer(
        profile_prefs_, g_browser_process->local_state());
  }
}

void CaptionController::OnLiveCaptionLanguageChanged() {
  if (enabled_)
    speech::SodaInstaller::GetInstance()->InstallLanguage(
        profile_prefs_->GetString(prefs::kLiveCaptionLanguageCode),
        g_browser_process->local_state());
}

bool CaptionController::IsLiveCaptionEnabled() {
  PrefService* profile_prefs = profile_prefs_;
  return profile_prefs->GetBoolean(prefs::kLiveCaptionEnabled);
}

void CaptionController::StartLiveCaption() {
  DCHECK(enabled_);
  if (!base::FeatureList::IsEnabled(media::kUseSodaForLiveCaption)) {
    CreateUI();
    return;
  }

  // The SodaInstaller determines whether SODA is already on the device and
  // whether or not to download. Once SODA is on the device and ready, the
  // SODAInstaller calls OnSodaInstalled on its observers. The UI is created at
  // that time.
  if (speech::SodaInstaller::GetInstance()->IsSodaInstalled()) {
    CreateUI();
  } else {
    speech::SodaInstaller::GetInstance()->AddObserver(this);
    speech::SodaInstaller::GetInstance()->Init(
        profile_prefs_, g_browser_process->local_state());
  }
}

void CaptionController::StopLiveCaption() {
  DCHECK(!enabled_);
  speech::SodaInstaller::GetInstance()->RemoveObserver(this);
  DestroyUI();
}

void CaptionController::OnSodaInstalled() {
  // Live Caption should always be enabled when this is called. If Live Caption
  // has been disabled, then this should not be observing the SodaInstaller
  // anymore.
  DCHECK(enabled_);
  speech::SodaInstaller::GetInstance()->RemoveObserver(this);
  CreateUI();
}

void CaptionController::CreateUI() {
  DCHECK(enabled_);
  if (is_ui_constructed_)
    return;
  DCHECK(!base::FeatureList::IsEnabled(media::kUseSodaForLiveCaption) ||
         speech::SodaInstaller::GetInstance()->IsSodaInstalled());
  is_ui_constructed_ = true;

  caption_bubble_controller_ = CaptionBubbleController::Create();
  caption_bubble_controller_->UpdateCaptionStyle(caption_style_);

  // Observe native theme changes for caption style updates.
  ui::NativeTheme::GetInstanceForWeb()->AddObserver(this);

  // Observe caption style prefs.
  for (const char* const pref_name : kCaptionStylePrefsToObserve) {
    DCHECK(!pref_change_registrar_->IsObserved(pref_name));
    pref_change_registrar_->Add(
        pref_name,
        base::BindRepeating(&CaptionController::OnCaptionStyleUpdated,
                            base::Unretained(this)));
  }
  OnCaptionStyleUpdated();
}

void CaptionController::DestroyUI() {
  DCHECK(!enabled_);
  if (!is_ui_constructed_)
    return;
  is_ui_constructed_ = false;
  caption_bubble_controller_.reset(nullptr);

  // Remove native theme observer.
  ui::NativeTheme::GetInstanceForWeb()->RemoveObserver(this);

  // Remove prefs to observe.
  for (const char* const pref_name : kCaptionStylePrefsToObserve) {
    DCHECK(pref_change_registrar_->IsObserved(pref_name));
    pref_change_registrar_->Remove(pref_name);
  }
}

void CaptionController::UpdateAccessibilityCaptionHistograms() {
  base::UmaHistogramBoolean("Accessibility.LiveCaption", enabled_);
}

bool CaptionController::DispatchTranscription(
    LiveCaptionSpeechRecognitionHost* live_caption_speech_recognition_host,
    const media::mojom::SpeechRecognitionResultPtr& result) {
  if (!caption_bubble_controller_)
    return false;
  return caption_bubble_controller_->OnTranscription(
      live_caption_speech_recognition_host, result);
}

void CaptionController::OnError(
    LiveCaptionSpeechRecognitionHost* live_caption_speech_recognition_host) {
  if (!caption_bubble_controller_)
    return;
  caption_bubble_controller_->OnError(live_caption_speech_recognition_host);
}

void CaptionController::OnAudioStreamEnd(
    LiveCaptionSpeechRecognitionHost* live_caption_speech_recognition_host) {
  if (!caption_bubble_controller_)
    return;
  caption_bubble_controller_->OnAudioStreamEnd(
      live_caption_speech_recognition_host);
}

void CaptionController::OnLanguageIdentificationEvent(
    const media::mojom::LanguageIdentificationEventPtr& event) {
  // TODO(crbug.com/1175357): Implement the UI for language identification.
}

void CaptionController::OnCaptionStyleUpdated() {
  // Metrics are recorded when passing the caption prefs to the browser, so do
  // not duplicate them here.
  caption_style_ = GetCaptionStyleFromUserSettings(profile_prefs_,
                                                   false /* record_metrics */);
  caption_bubble_controller_->UpdateCaptionStyle(caption_style_);
}

}  // namespace captions
