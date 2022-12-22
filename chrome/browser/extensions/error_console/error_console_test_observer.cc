// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/error_console/error_console_test_observer.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"

namespace extensions {

ErrorConsoleTestObserver::ErrorConsoleTestObserver(size_t errors_expected,
                                                   Profile* profile)
    : errors_expected_(errors_expected), profile_(profile) {
  observation_.Observe(ErrorConsole::Get(profile_.get()));
}

ErrorConsoleTestObserver::~ErrorConsoleTestObserver() = default;

void ErrorConsoleTestObserver::EnableErrorCollection() {
  // Errors are collected for extensions when the user preferences are set to
  // enable developer mode.
  profile_->GetPrefs()->SetBoolean(prefs::kExtensionsUIDeveloperMode, true);
}

void ErrorConsoleTestObserver::OnErrorAdded(const ExtensionError* error) {
  ++errors_observed_;
  if (errors_observed_ >= errors_expected_) {
    run_loop_.Quit();
  }
}

void ErrorConsoleTestObserver::WaitForErrors() {
  if (errors_observed_ < errors_expected_) {
    run_loop_.Run();
  }
}

}  // namespace extensions
