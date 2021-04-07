// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/accessibility/soda_installer.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/public/cpp/ash_pref_names.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
#include "base/feature_list.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/soda/pref_names.h"
#include "media/base/media_switches.h"

namespace {

constexpr int kSodaCleanUpDelayInDays = 30;

}  // namespace

namespace speech {

SodaInstaller::SodaInstaller() = default;

SodaInstaller::~SodaInstaller() = default;

void SodaInstaller::Init(PrefService* profile_prefs,
                         PrefService* global_prefs) {
  if (!base::FeatureList::IsEnabled(media::kUseSodaForLiveCaption))
    return;

  if (IsAnyFeatureUsingSodaEnabled(profile_prefs)) {
    global_prefs->SetTime(prefs::kSodaScheduledDeletionTime, base::Time());
    speech::SodaInstaller::GetInstance()->InstallSoda(profile_prefs);
    speech::SodaInstaller::GetInstance()->InstallLanguage(profile_prefs);
  } else {
    base::Time deletion_time =
        global_prefs->GetTime(prefs::kSodaScheduledDeletionTime);
    if (!deletion_time.is_null() && deletion_time < base::Time::Now()) {
      UninstallSoda(global_prefs);
    }
  }
}

void SodaInstaller::SetUninstallTimer(PrefService* profile_prefs,
                                      PrefService* global_prefs) {
  // Do not schedule uninstallation if any SODA client features are still
  // enabled.
  if (IsAnyFeatureUsingSodaEnabled(profile_prefs))
    return;

  // Schedule deletion.
  global_prefs->SetTime(
      prefs::kSodaScheduledDeletionTime,
      base::Time::Now() + base::TimeDelta::FromDays(kSodaCleanUpDelayInDays));
}

void SodaInstaller::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void SodaInstaller::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void SodaInstaller::NotifyOnSodaInstalled() {
  for (Observer& observer : observers_)
    observer.OnSodaInstalled();
}

void SodaInstaller::NotifyOnSodaError() {
  for (Observer& observer : observers_)
    observer.OnSodaError();
}

void SodaInstaller::NotifyOnSodaProgress(int percent) {
  for (Observer& observer : observers_)
    observer.OnSodaProgress(percent);
}

void SodaInstaller::NotifySodaInstalledForTesting() {
  soda_binary_installed_ = true;
  language_installed_ = true;
  NotifyOnSodaInstalled();
}

bool SodaInstaller::IsAnyFeatureUsingSodaEnabled(PrefService* prefs) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // TODO(crbug.com/1165437): Add Projector feature.
  return prefs->GetBoolean(prefs::kLiveCaptionEnabled) ||
         prefs->GetBoolean(ash::prefs::kAccessibilityDictationEnabled);
#else  // !BUILDFLAG(IS_CHROMEOS_ASH)
  return prefs->GetBoolean(prefs::kLiveCaptionEnabled);
#endif
}

}  // namespace speech
