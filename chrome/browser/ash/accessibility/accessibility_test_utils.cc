// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/accessibility/accessibility_test_utils.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"

namespace ash {

ExtensionConsoleErrorObserver::ExtensionConsoleErrorObserver(
    Profile* profile,
    const char* extension_id) {
  error_console_ = extensions::ErrorConsole::Get(profile);
  profile->GetPrefs()->SetBoolean(prefs::kExtensionsUIDeveloperMode, true);
  error_console_->SetReportingForExtension(
      extension_id, extensions::ExtensionError::Type::RUNTIME_ERROR,
      true /* enabled */);
  error_console_->SetReportingForExtension(
      extension_id, extensions::ExtensionError::Type::INTERNAL_ERROR,
      true /* enabled */);
  error_console_->AddObserver(this);
}

ExtensionConsoleErrorObserver::~ExtensionConsoleErrorObserver() {
  if (error_console_)
    error_console_->RemoveObserver(this);
}

void ExtensionConsoleErrorObserver::OnErrorAdded(
    const extensions::ExtensionError* error) {
  // Add a non-fatal failure to the test. Thus the test can continue
  // executing in case the warning/error is helpful in debugging.
  ADD_FAILURE() << "Found extension console warning or error with message: "
                << error->message();
  errors_.push_back(error->message());
}

void ExtensionConsoleErrorObserver::OnErrorConsoleDestroyed() {
  error_console_ = nullptr;
}

bool ExtensionConsoleErrorObserver::HasErrorsOrWarnings() {
  return !errors_.empty();
}

std::string ExtensionConsoleErrorObserver::GetErrorOrWarningAt(
    size_t index) const {
  return errors_.size() > index ? base::UTF16ToUTF8(errors_[index])
                                : std::string();
}

size_t ExtensionConsoleErrorObserver::GetErrorsAndWarningsCount() const {
  return errors_.size();
}

}  // namespace ash
