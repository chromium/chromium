// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/mac/key_window_notifier.h"

KeyWindowNotifier::KeyWindowNotifier() = default;
KeyWindowNotifier::~KeyWindowNotifier() = default;

void KeyWindowNotifier::AddObserver(Observer* obs) {
  observers_.AddObserver(obs);
}

void KeyWindowNotifier::RemoveObserver(Observer* obs) {
  observers_.RemoveObserver(obs);
}

void KeyWindowNotifier::NotifyNoKeyWindow() {
  for (Observer& obs : observers_) {
    obs.OnNoKeyWindow();
  }
}
