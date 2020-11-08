// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/accessibility/soda_installer.h"

namespace speech {

SODAInstaller::SODAInstaller() = default;

SODAInstaller::~SODAInstaller() = default;

void SODAInstaller::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void SODAInstaller::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void SODAInstaller::NotifyOnSODAInstalled() {
  for (Observer& observer : observers_)
    observer.OnSODAInstalled();
}

void SODAInstaller::NotifyOnSODAError() {
  for (Observer& observer : observers_)
    observer.OnSODAError();
}

void SODAInstaller::NotifyOnSODAProgress(int percent) {
  for (Observer& observer : observers_)
    observer.OnSODAProgress(percent);
}

}  // namespace speech
