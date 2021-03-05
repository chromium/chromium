// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/accessibility/soda_installer.h"

#include "chrome/browser/browser_process.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"

namespace speech {

SodaInstaller::SodaInstaller() = default;

SodaInstaller::~SodaInstaller() = default;

void SodaInstaller::Init(PrefService* prefs) {
  if (prefs->GetBoolean(prefs::kLiveCaptionEnabled)) {
    g_browser_process->local_state()->SetTime(prefs::kSodaScheduledDeletionTime,
                                              base::Time());
    speech::SodaInstaller::GetInstance()->InstallSoda(prefs);
    speech::SodaInstaller::GetInstance()->InstallLanguage(prefs);
  } else {
    PrefService* global_prefs = g_browser_process->local_state();
    base::Time deletion_time =
        global_prefs->GetTime(prefs::kSodaScheduledDeletionTime);
    if (!deletion_time.is_null() && deletion_time < base::Time::Now()) {
      UninstallSoda(global_prefs);
    }
  }
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

}  // namespace speech
