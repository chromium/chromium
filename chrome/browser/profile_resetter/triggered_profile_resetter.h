// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROFILE_RESETTER_TRIGGERED_PROFILE_RESETTER_H_
#define CHROME_BROWSER_PROFILE_RESETTER_TRIGGERED_PROFILE_RESETTER_H_

#include <stddef.h>

#include "base/macros.h"
#include "base/strings/string16.h"
#include "build/build_config.h"
#include "components/keyed_service/core/keyed_service.h"

class Profile;

// This service is responsible for evaluating whether a profile reset trigger
// has been set and not yet consumed by |profile_|. If it has, the profile is
// eligible for reset and a profile reset UI will be shown to the user. The
// intended use case for this is to provide a sanctioned profile reset API for
// third party tools (anti-virus or cleaner tools) that wish to reset users'
// profiles as part of their cleanup flow.
//
// To use this mechanism from a third party tool, perform the following steps:
//   1) Create (or open) the registry key
//      HKCU\Software\$PRODUCT_NAME\TriggeredReset where $PRODUCT_NAME is one
//      of the values "Google\\Chrome" or "Chromium".
//   2) Set a REG_SZ value called "ToolName" to the localized name of the tool.
//      This string (truncated to kMaxToolNameLength) will be displayed in a
//      notification UI. The "ToolName" should be just the name of the tool,
//      e.g. "AwesomeAV".
//   3) Set a REG_QWORD value called "Timestamp" with a timestamp for the reset
//      event. This value should be obtained from a call to
//      ::GetSystemTimeAsFileTime() at the time the reset is requested. This
//      value will be persisted in the profile when it is reset and will be used
//      to avoid multiple resets.
//
// Some considerations:
//
// * Chrome supports multiple profiles. When the above steps are followed,
//   each profile will enter the reset flow as it is opened.
// * New profiles created while any timestamp is present will not get the reset
//   flow.
class TriggeredProfileResetter : public KeyedService {
 public:
  enum : size_t { kMaxToolNameLength = 100 };

  explicit TriggeredProfileResetter(Profile* profile);
  ~TriggeredProfileResetter() override;

  // Causes the TriggeredProfileResetter to look for the presence of a trigger.
  // If a trigger is found, it is disarmed so that future instances of the
  // service will no longer trigger a reset. Subsequent calls to HasResetTrigger
  // will return whether |profile_| is subject to a reset.
  virtual void Activate();

  // Returns true iff the given profile has a trigger set for a reset UI flow
  // according to the description in the class comment. Must call Activate()
  // first.
  virtual bool HasResetTrigger();

  // Clears the reset trigger such that subsequent calls to |HasResetTrigger|
  // will return false.
  virtual void ClearResetTrigger();

  // Returns the name of the tool that performed the reset. This string will be
  // truncated to a length of |kMaxToolNameLength|.
  virtual base::string16 GetResetToolName();

 private:
#if defined(OS_WIN)
  Profile* profile_;
#endif  // defined(OS_WIN)

  bool has_reset_trigger_ = false;
  bool activate_called_ = false;

  base::string16 tool_name_;

  DISALLOW_COPY_AND_ASSIGN(TriggeredProfileResetter);
};

// Exposed for testing.
extern const wchar_t kTriggeredResetRegistryPath[];
extern const wchar_t kTriggeredResetToolName[];
extern const wchar_t kTriggeredResetTimestamp[];

#endif  // CHROME_BROWSER_PROFILE_RESETTER_TRIGGERED_PROFILE_RESETTER_H_
