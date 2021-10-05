// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sessions/exit_type_service.h"

#include "base/memory/ptr_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/exit_type_service_factory.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"

namespace {

// Value written to prefs for ExitType::kCrashed and ExitType::kForcedShutdown.
const char kPrefExitTypeCrashed[] = "Crashed";
const char kPrefExitTypeNormal[] = "Normal";
const char kPrefExitTypeForcedShutdown[] = "SessionEnded";

// Converts the `kSessionExitType` pref to the corresponding EXIT_TYPE.
ExitType SessionTypePrefValueToExitType(const std::string& value) {
  if (value == kPrefExitTypeForcedShutdown)
    return ExitType::kForcedShutdown;
  if (value == kPrefExitTypeCrashed)
    return ExitType::kCrashed;
  return ExitType::kClean;
}

// Converts an ExitType into a string that is written to prefs.
std::string ExitTypeToSessionTypePrefValue(ExitType type) {
  switch (type) {
    case ExitType::kClean:
      return kPrefExitTypeNormal;
    case ExitType::kForcedShutdown:
      return kPrefExitTypeForcedShutdown;
    case ExitType::kCrashed:
      return kPrefExitTypeCrashed;
  }
}

}  // namespace

ExitTypeService::~ExitTypeService() {
  SetCurrentSessionExitType(ExitType::kClean);
}

// static
ExitTypeService* ExitTypeService::GetInstanceForProfile(Profile* profile) {
  return ExitTypeServiceFactory::GetForProfile(profile);
}

// static
ExitType ExitTypeService::GetLastSessionExitType(Profile* profile) {
  ExitTypeService* tracker =
      GetInstanceForProfile(profile->GetOriginalProfile());
  // `tracker` may be null for certain profile types (such as signin profile
  // on chromeos).
  return tracker ? tracker->last_session_exit_type() : ExitType::kClean;
}

void ExitTypeService::SetCurrentSessionExitType(ExitType exit_type) {
  // This may be invoked multiple times during shutdown. Only persist the value
  // first passed in (unless it's a reset to the crash state, which happens when
  // foregrounding the app on mobile).
  if (exit_type == ExitType::kCrashed ||
      current_session_exit_type_ == ExitType::kCrashed) {
    current_session_exit_type_ = exit_type;
    profile_->GetPrefs()->SetString(prefs::kSessionExitType,
                                    ExitTypeToSessionTypePrefValue(exit_type));
  }
}

ExitTypeService::ExitTypeService(Profile* profile)
    : profile_(profile),
      last_session_exit_type_(SessionTypePrefValueToExitType(
          profile_->GetPrefs()->GetString(prefs::kSessionExitType))),
      current_session_exit_type_(ExitType::kCrashed) {
  // Mark the session as open.
  profile_->GetPrefs()->SetString(prefs::kSessionExitType,
                                  kPrefExitTypeCrashed);
}
