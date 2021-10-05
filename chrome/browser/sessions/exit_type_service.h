// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SESSIONS_EXIT_TYPE_SERVICE_H_
#define CHROME_BROWSER_SESSIONS_EXIT_TYPE_SERVICE_H_

#include "components/keyed_service/core/keyed_service.h"

class Profile;

enum class ExitType {
  // Chrome was shutdown cleanly.
  kClean,

  // Shutdown was forced by the OS. On some OS's (such as Windows) this causes
  // chrome to shutdown differently.
  kForcedShutdown,

  kCrashed,
};

// ExitTypeService tracks the exit status of the last and current time Chrome
// was run.
class ExitTypeService : public KeyedService {
 public:
  ExitTypeService(const ExitTypeService&) = delete;
  ExitTypeService& operator=(const ExitTypeService&) = delete;
  ~ExitTypeService() override;

  // Returns the ExitTypeService for the specified profile. This returns
  // null for non-regular profiles.
  static ExitTypeService* GetInstanceForProfile(Profile* profile);

  // Returns the last session exit type. If supplied an incognito profile,
  // this will return the value for the original profile.
  static ExitType GetLastSessionExitType(Profile* profile);

  // Sets the ExitType for the profile. This may be invoked multiple times
  // during shutdown; only the first such change (the transition from
  // ExitType::kCrashed to one of the other values) is written to prefs, any
  // later calls are ignored.
  //
  // NOTE: this is invoked internally on a normal shutdown, but is public so
  // that it can be invoked when the user logs out/powers down (WM_ENDSESSION),
  // or to handle backgrounding/foregrounding on mobile.
  void SetCurrentSessionExitType(ExitType exit_type);

  ExitType last_session_exit_type() const { return last_session_exit_type_; }

  void SetLastSessionExitTypeForTest(ExitType type) {
    last_session_exit_type_ = type;
  }

 private:
  friend class ExitTypeServiceFactory;

  explicit ExitTypeService(Profile* profile);

  Profile* const profile_;
  ExitType last_session_exit_type_;
  ExitType current_session_exit_type_;
};

#endif  // CHROME_BROWSER_SESSIONS_EXIT_TYPE_SERVICE_H_
