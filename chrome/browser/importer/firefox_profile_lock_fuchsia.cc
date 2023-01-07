// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/importer/firefox_profile_lock.h"

// The FirefoxProfileLock is empty on Fuchsia because the filesystem is not
// shared across component, so settings file will have to be imported into the
// Chrome process and will then be accessed read-only. No lock is needed in that
// situation.

void FirefoxProfileLock::Init() {}

void FirefoxProfileLock::Lock() {}

void FirefoxProfileLock::Unlock() {}

bool FirefoxProfileLock::HasAcquired() {
  return true;
}
