// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/glic_fre_store_impl.h"

#include <memory>
#include <string_view>
#include <utility>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/glic/gemini_enterprise/geic_fre.h"
#include "chrome/browser/glic/gemini_enterprise/geic_pref_names.h"
#include "chrome/browser/glic/glic_metrics.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/glic/public/glic_fre_store.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "components/prefs/pref_service.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/background/glic/glic_launcher_configuration.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/profile_browser_collection.h"
#include "chrome/browser/ui/user_education/browser_user_education_interface.h"
#include "components/feature_engagement/public/event_constants.h"
#endif  // !BUILDFLAG(IS_ANDROID)

namespace glic {

namespace {

class GeminiGlicFreStore final : public GlicFreStore {
 public:
  ~GeminiGlicFreStore() override = default;

  prefs::FreStatus GetStatus(Profile* profile) const override {
    if (base::FeatureList::IsEnabled(
            features::kGlicExperimentalTriggeringOptInBypass) ||
        GlicEnabling::IsRetailDemoModeDesktop()) {
      return prefs::FreStatus::kCompleted;
    }
    return static_cast<prefs::FreStatus>(
        profile->GetPrefs()->GetInteger(prefs::kGlicCompletedFre));
  }

  void SetStatus(Profile* profile, prefs::FreStatus status) const override {
    profile->GetPrefs()->SetInteger(prefs::kGlicCompletedFre,
                                    std::to_underlying(status));
  }

  void CompleteOnboarding(Profile* profile) const override {
    if (auto* glic_service = GlicKeyedService::Get(profile)) {
      glic_service->metrics()->OnTrustFirstOnboardingAccept();
    }
    SetStatus(profile, prefs::FreStatus::kCompleted);

#if !BUILDFLAG(IS_ANDROID)  // NEEDS_ANDROID_IMPL
    GlicLauncherConfiguration::CheckDefaultBrowserToEnableLauncher();

    BrowserWindowInterface* browser =
        ProfileBrowserCollection::GetForProfile(profile)->FindTabbedBrowser();
    if (auto* interface = BrowserUserEducationInterface::From(browser)) {
      interface->NotifyAdditionalConditionEvent(
          feature_engagement::events::kGlicOnboardingCompleted);
    }
#endif  // !BUILDFLAG(IS_ANDROID)
  }

  void ApplyDevSwitches(Profile* profile) const override {
    auto* command_line = base::CommandLine::ForCurrentProcess();
    if (command_line->HasSwitch(::switches::kGlicAlwaysOpenFre)) {
      // Unset the status to ensure the FRE is shown for testing convenience.
      SetStatus(profile, prefs::FreStatus::kNotStarted);
    } else if (command_line->HasSwitch(::switches::kGlicAutomation) ||
               command_line->HasSwitch(::switches::kGlicAlwaysSkipFre)) {
      // Skip the FRE if automation is enabled.
      SetStatus(profile, prefs::FreStatus::kCompleted);
    }
  }

  void OnPrimaryAccountCleared(Profile* profile) const override {
    SetStatus(profile, prefs::FreStatus::kNotStarted);
  }

  // The Gemini section already waits for the FRE through
  // `ProfileEnablement::ShouldShowSettingsPage()`.
  bool HidesSettingsUntilCompleted() const override { return false; }

  std::string_view GetStatusPrefName() const override {
    return prefs::kGlicCompletedFre;
  }
};

// It can only be completed: it can't be dismissed.
class GeminiEnterpriseGlicFreStore final : public GlicFreStore {
 public:
  ~GeminiEnterpriseGlicFreStore() override = default;

  prefs::FreStatus GetStatus(Profile* profile) const override {
    // Not enforced until the GE web app can complete it.
    // TODO(b/576766756): Remove once `features::kGeicFre` is enabled.
    if (!base::FeatureList::IsEnabled(features::kGeicFre)) {
      return prefs::FreStatus::kCompleted;
    }
    // Fails closed if the GEiC FRE status is unavailable.
    return geic::GetFreStatus(profile) == geic::prefs::GeicFreStatus::kCompleted
               ? prefs::FreStatus::kCompleted
               : prefs::FreStatus::kNotStarted;
  }

  void SetStatus(Profile* profile, prefs::FreStatus status) const override {
    // The GEiC FRE can't be dismissed, and is only reset with
    // `--geic-reset-fre`, so only completion is stored.
    if (status == prefs::FreStatus::kCompleted) {
      geic::CompleteFre(profile);
    }
  }

  void CompleteOnboarding(Profile* profile) const override {
    // The consumer onboarding side effects (metrics, OS launcher, IPH) don't
    // apply.
    // TODO(b/576047042): Add GEiC onboarding metrics.
    SetStatus(profile, prefs::FreStatus::kCompleted);
  }

  void ApplyDevSwitches(Profile* profile) const override {
    // The consumer FRE switches (e.g. `--glic-automation`) intentionally don't
    // apply. `--geic-always-skip-fre` is applied when the status is read (see
    // `geic::GetFreStatus()`), so that nothing is persisted.
    if (base::CommandLine::ForCurrentProcess()->HasSwitch(
            ::switches::kGeicResetFre)) {
      // Clears the GEiC FRE status for all GE instances.
      profile->GetPrefs()->ClearPref(geic::prefs::kGeicCompletedFre);
    }
  }

  void OnPrimaryAccountCleared(Profile* profile) const override {
    // Intentionally a no-op: this is only called on Android, where GEiC isn't
    // supported.
  }

  bool HidesSettingsUntilCompleted() const override { return true; }

  // Keyed by GE instance, so changes for other GE instances are also
  // reported.
  std::string_view GetStatusPrefName() const override {
    return geic::prefs::kGeicCompletedFre;
  }
};

}  // namespace

std::unique_ptr<GlicFreStore> CreateGeminiFreStore() {
  return std::make_unique<GeminiGlicFreStore>();
}

std::unique_ptr<GlicFreStore> CreateGeminiEnterpriseFreStore() {
  return std::make_unique<GeminiEnterpriseGlicFreStore>();
}

}  // namespace glic
