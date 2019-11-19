// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
#ifndef CHROME_BROWSER_CHROMEOS_CROSTINI_CROSTINI_FEATURES_H_
#define CHROME_BROWSER_CHROMEOS_CROSTINI_CROSTINI_FEATURES_H_

#include "base/macros.h"

class Profile;

namespace crostini {

// CrostiniFeatures provides an interface for querying which parts of crostini
// are enabled or allowed.
class CrostiniFeatures {
 public:
  static CrostiniFeatures* Get();

  // Returns true if crostini is allowed to run for |profile|.
  // Otherwise, returns false, e.g. if crostini is not available on the device,
  // or it is in the flow to set up managed account creation.
  virtual bool IsAllowed(Profile* profile);

  // When |check_policy| is true, returns true if fully interactive crostini UI
  // may be shown. Implies crostini is allowed to run.
  // When check_policy is false, returns true if crostini UI is not forbidden by
  // hardware, flags, etc, even if it is forbidden by the enterprise policy. The
  // UI uses this to indicate that crostini is available but disabled by policy.
  virtual bool IsUIAllowed(Profile*, bool check_policy = true);

  // Returns whether if Crostini has been enabled, i.e. the user has launched it
  // at least once and not deleted it.
  virtual bool IsEnabled(Profile* profile);

  // Returns true if policy allows export import UI.
  virtual bool IsExportImportUIAllowed(Profile*);

  // Returns whether user is allowed root access to Crostini. Always returns
  // true when advanced access controls feature flag is disabled.
  virtual bool IsRootAccessAllowed(Profile*);

  // TODO(crbug.com/1004708): Move other functions from crostini_util to here.

 protected:
  static void SetForTesting(CrostiniFeatures* features);

  CrostiniFeatures();
  virtual ~CrostiniFeatures();

  DISALLOW_COPY_AND_ASSIGN(CrostiniFeatures);
};

}  // namespace crostini

#endif  // CHROME_BROWSER_CHROMEOS_CROSTINI_CROSTINI_FEATURES_H_
