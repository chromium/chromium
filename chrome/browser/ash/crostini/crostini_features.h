// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
#ifndef CHROME_BROWSER_ASH_CROSTINI_CROSTINI_FEATURES_H_
#define CHROME_BROWSER_ASH_CROSTINI_CROSTINI_FEATURES_H_

#include <string>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"

class Profile;

namespace crostini {

// CrostiniFeatures provides an interface for querying which parts of crostini
// are enabled or allowed.
class CrostiniFeatures {
 public:
  static CrostiniFeatures* Get();

  CrostiniFeatures(const CrostiniFeatures&) = delete;
  CrostiniFeatures& operator=(const CrostiniFeatures&) = delete;

  // Returns false if this |profile| will never be allowed to run crostini for
  // the lifetime of this process, otherwise returns true. The return value of
  // this method is guaranteed not to change for a given |profile| within the
  // lifetime of the process. Also provides the |reason| if crostini is
  // disallowed. The |reason| string is to only be used in crosh/vmc error
  // messages.
  virtual bool CouldBeAllowed(Profile* profile, std::string* reason);

  // Returns false if this |profile| will never be allowed to run crostini for
  // the lifetime of this process, otherwise returns true. The return value of
  // this method is guaranteed not to change for a given |profile| within the
  // lifetime of the process.
  virtual bool CouldBeAllowed(Profile* profile);

  // Returns true if |profile| is allowed to run crostini at this moment. This
  // method will never return true if CouldBeAllowed returns false for the same
  // profile, but otherwise may change return value at any time. Also provides
  // the reason if crostini is disallowed.
  virtual bool IsAllowedNow(Profile* profile, std::string* reason);

  // Returns true if |profile| is allowed to run crostini at this moment. This
  // method will never return true if CouldBeAllowed returns false for the same
  // profile, but otherwise may change return value at any time.
  virtual bool IsAllowedNow(Profile* profile);

  // Returns whether if Crostini has been enabled, i.e. the user has launched it
  // at least once and not deleted it.
  virtual bool IsEnabled(Profile* profile);

  // Returns true if policy allows export import UI.
  virtual bool IsExportImportUIAllowed(Profile*);

  // Returns whether user is allowed root access to Crostini. Always returns
  // true when advanced access controls feature flag is disabled.
  virtual bool IsRootAccessAllowed(Profile*);

  // Returns true if container upgrade ui is allowed by flag.
  virtual bool IsContainerUpgradeUIAllowed(Profile*);

  using CanChangeAdbSideloadingCallback =
      base::OnceCallback<void(bool can_change_adb_sideloading)>;

  // Checks whether the user is allowed to enable and disable ADB sideloading
  // based on whether the user is the owner, whether the user and the device
  // are managed, and feature flag and policies for managed case. Once this is
  // established, the callback is invoked and passed a boolean indicating
  // whether changes to ADB sideloading are allowed.
  virtual void CanChangeAdbSideloading(
      Profile* profile,
      CanChangeAdbSideloadingCallback callback);

  // Returns whether the user is allowed to configure port forwarding into
  // Crostini. If the user is not managed or if the policy is unset or true,
  // then this returns true, else if the policy is set to false, this returns
  // false.
  virtual bool IsPortForwardingAllowed(Profile* profile);

  // Returns true if user is allowed to use multiple (non-default) containers.
  virtual bool IsMultiContainerAllowed(Profile*);

  // TODO(crbug.com/40647881): Move other functions from crostini_util to here.

 protected:
  static void SetForTesting(CrostiniFeatures* features);

  CrostiniFeatures();
  virtual ~CrostiniFeatures();

 private:
  base::WeakPtrFactory<CrostiniFeatures> weak_factory_{this};
};

}  // namespace crostini

#endif  // CHROME_BROWSER_ASH_CROSTINI_CROSTINI_FEATURES_H_
