// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ARC_ARC_UI_AVAILABILITY_REPORTER_H_
#define CHROME_BROWSER_CHROMEOS_ARC_ARC_UI_AVAILABILITY_REPORTER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/time/time.h"

class Profile;

namespace arc {

// Helper class that reports ARC UI availability to UMA. It observes major UI
// instances are all connected.
class ArcUiAvailabilityReporter {
 public:
  // Helper that notifies |ArcUiAvailabilityReporter| that connection is ready.
  class ConnectionNotifierBase;

  enum class Mode {
    kOobeProvisioning,
    kInSessionProvisioning,
    kAlreadyProvisioned
  };

  ArcUiAvailabilityReporter(Profile* profile, Mode mode);
  ~ArcUiAvailabilityReporter();

  static std::string GetHistogramNameForMode(Mode mode);

  // Called when any tracked instance is connected.
  void MaybeReport();

 private:
  Profile* const profile_;
  const Mode mode_;
  const base::TimeTicks start_ticks_;

  std::vector<std::unique_ptr<ConnectionNotifierBase>> connection_notifiers_;

  DISALLOW_COPY_AND_ASSIGN(ArcUiAvailabilityReporter);
};

}  // namespace arc

#endif  // CHROME_BROWSER_CHROMEOS_ARC_ARC_UI_AVAILABILITY_REPORTER_H_
