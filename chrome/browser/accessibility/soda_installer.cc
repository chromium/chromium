// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/accessibility/soda_installer.h"

namespace speech {

SodaInstaller::SodaInstaller() = default;

SodaInstaller::~SodaInstaller() = default;

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
