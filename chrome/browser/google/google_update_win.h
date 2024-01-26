// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GOOGLE_GOOGLE_UPDATE_WIN_H_
#define CHROME_BROWSER_GOOGLE_GOOGLE_UPDATE_WIN_H_

#include <wrl/client.h>

#include <optional>
#include <string>

#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "chrome/updater/app/server/win/updater_legacy_idl.h"
#include "ui/gfx/native_widget_types.h"

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

// These values are used for a histogram. Do not reorder.
enum GoogleUpdateErrorCode {
  // The upgrade completed successfully (or hasn't been started yet).
  GOOGLE_UPDATE_NO_ERROR = 0,
  // Google Update only supports upgrading if Chrome is installed in the default
  // location. This error will appear for developer builds and with
  // installations unzipped to random locations.
  CANNOT_UPGRADE_CHROME_IN_THIS_DIRECTORY = 1,
  // Failed to create Google Update JobServer COM class. DEPRECATED.
  // GOOGLE_UPDATE_JOB_SERVER_CREATION_FAILED = 2,
  // Failed to create Google Update OnDemand COM class.
  GOOGLE_UPDATE_ONDEMAND_CLASS_NOT_FOUND = 3,
  // Google Update OnDemand COM class reported an error during a check for
  // update (or while upgrading).
  GOOGLE_UPDATE_ONDEMAND_CLASS_REPORTED_ERROR = 4,
  // A call to GetResults failed. DEPRECATED.
  // GOOGLE_UPDATE_GET_RESULT_CALL_FAILED = 5,
  // A call to GetVersionInfo failed. DEPRECATED
  // GOOGLE_UPDATE_GET_VERSION_INFO_FAILED = 6,
  // An error occurred while upgrading (or while checking for update).
  // Check the Google Update log in %TEMP% for more details.
  GOOGLE_UPDATE_ERROR_UPDATING = 7,
  // Updates can not be downloaded because the administrator has disabled all
  // types of updating.
  GOOGLE_UPDATE_DISABLED_BY_POLICY = 8,
  // Updates can not be downloaded because the administrator has disabled
  // manual (on-demand) updates.  Automatic background updates are allowed.
  GOOGLE_UPDATE_DISABLED_BY_POLICY_AUTO_ONLY = 9,
  NUM_ERROR_CODES
};

// A delegate by which a caller of BeginUpdateCheck is notified of the status
// and results of an update check.
class UpdateCheckDelegate {
 public:
  virtual ~UpdateCheckDelegate() {}

  // Invoked following a successful update check. |new_version|, if not empty,
  // indicates the new version that is available. Otherwise (if |new_version| is
  // empty), Chrome is up to date. This method will only be invoked when
  // BeginUpdateCheck is called with |install_update_if_possible| == false.
  virtual void OnUpdateCheckComplete(const std::u16string& new_version) = 0;

  // Invoked zero or more times during an upgrade. |progress|, a number between
  // 0 and 100 (inclusive), is an estimation as to what percentage of the
  // upgrade has completed. |new_version| indicates the version that is being
  // download and installed. This method will only be invoked when
  // BeginUpdateCheck is called with |install_update_if_possible| == true.
  virtual void OnUpgradeProgress(int progress,
                                 const std::u16string& new_version) = 0;

  // Invoked following a successful upgrade. |new_version| indicates the version
  // to which Chrome was updated. This method will only be invoked when
  // BeginUpdateCheck is called with |install_update_if_possible| == true.
  virtual void OnUpgradeComplete(const std::u16string& new_version) = 0;

  // Invoked following an unrecoverable error, indicated by |error_code|.
  // |html_error_message|, if not empty, must be a localized string containing
  // all information required by users to act on the error as well as for
  // support staff to diagnose it (i.e. |error_code| and any other related
  // state information).  |new_version|, if not empty, indicates the version
  // to which an upgrade attempt was made.
  virtual void OnError(GoogleUpdateErrorCode error_code,
                       const std::u16string& html_error_message,
                       const std::u16string& new_version) = 0;

 protected:
  UpdateCheckDelegate() {}
};

// Begins an asynchronous update check. If a new version is
// available and |install_update_if_possible| is true, the new version will be
// automatically downloaded and installed. |elevation_window| is the window
// which should own any necessary elevation UI. Methods on |delegate| will be
// invoked on the caller's thread to provide feedback on the operation, with
// messages localized to |locale| if possible.
void BeginUpdateCheck(
    const std::string& locale,
    bool install_update_if_possible,
    gfx::AcceleratedWidget elevation_window,
    const base::WeakPtr<UpdateCheckDelegate>& delegate);

// The state from a completed update check.
struct UpdateState {
  UpdateState();
  UpdateState(const UpdateState&);
  UpdateState(UpdateState&&);
  UpdateState& operator=(UpdateState&&);
  ~UpdateState();

  // GOOGLE_UPDATE_NO_ERROR if the last check or update succeeded; otherwise,
  // the nature of the failure.
  GoogleUpdateErrorCode error_code = GOOGLE_UPDATE_NO_ERROR;

  // The next version available or an empty string if either no update is
  // available or an error occurred before the new version was discovered.
  std::u16string new_version;

  // S_OK if the last check or update succeeded; otherwise, the failing error
  // from Google Update or COM.
  HRESULT hresult = S_OK;

  // If present, the process exit code from the failed run of the installer.
  std::optional<int> installer_exit_code;
};

// Returns the state from the most recent completed update check or no value if
// no such check has taken place.
std::optional<UpdateState> GetLastUpdateState();

// A type of callback supplied by tests to provide a custom IGoogleUpdate3Web
// implementation (see src/google_update/google_update_idl.idl).
using GoogleUpdate3ClassFactory = base::RepeatingCallback<HRESULT(
    Microsoft::WRL::ComPtr<IGoogleUpdate3Web>*)>;

// For use by tests that wish to provide a custom IGoogleUpdate3Web
// implementation independent of Google Update's.
void SetGoogleUpdateFactoryForTesting(
    GoogleUpdate3ClassFactory google_update_factory);

void SetUpdateDriverTaskRunnerForTesting(
    base::SingleThreadTaskRunner* task_runner);

#endif  // CHROME_BROWSER_GOOGLE_GOOGLE_UPDATE_WIN_H_
