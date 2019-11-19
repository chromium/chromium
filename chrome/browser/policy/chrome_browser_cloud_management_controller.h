// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_CHROME_BROWSER_CLOUD_MANAGEMENT_CONTROLLER_H_
#define CHROME_BROWSER_POLICY_CHROME_BROWSER_CLOUD_MANAGEMENT_CONTROLLER_H_

#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/observer_list.h"
#include "base/time/time.h"

class PrefService;

namespace network {
class SharedURLLoaderFactory;
}

namespace enterprise_reporting {
class ReportScheduler;
}

namespace policy {
class ChromeBrowserCloudManagementRegistrar;
class ConfigurationPolicyProvider;
class MachineLevelUserCloudPolicyManager;
class MachineLevelUserCloudPolicyFetcher;
class ChromeBrowserCloudManagementRegisterWatcher;

// A class that setups and manages all CBCM related features.
class ChromeBrowserCloudManagementController {
 public:
  // Chrome browser cloud management enrollment result.
  enum class RegisterResult {
    kNoEnrollmentNeeded,  // The device won't be enrolled without an enrollment
                          // token.
    kEnrollmentSuccessBeforeDialogDisplayed,  // The enrollment process is
                                              // finished before dialog
                                              // displayed.
    kEnrollmentSuccess,  // The device has been enrolled successfully
    kQuitDueToFailure,   // The enrollment has failed or aborted, user choose to
                         // quit Chrome.
    kRestartDueToFailure,  // The enrollment has failed, user choose to restart
    kEnrollmentFailedSilently,  // The enrollment has failed, admin choose to
                                // ignore the error message.
    kEnrollmentFailedSilentlyBeforeDialogDisplayed,  // The enrollment has
                                                     // failed before dialog
                                                     // displayed, admin choose
                                                     // to ignore the error
                                                     // message.
  };

  class Observer {
   public:
    virtual ~Observer() {}

    // Called when policy enrollment is finished.
    // |succeeded| is true if |dm_token| is returned from the server.
    virtual void OnPolicyRegisterFinished(bool succeeded) {}
  };

  // Directory name under the user-data-dir where the policy data is stored.
  static const base::FilePath::CharType kPolicyDir[];

  // The Chrome browser cloud management is only enabled on Chrome by default.
  // However, it can be enabled on Chromium by command line switch for test and
  // development purpose.
  static bool IsEnabled();

  ChromeBrowserCloudManagementController();
  virtual ~ChromeBrowserCloudManagementController();

  static std::unique_ptr<MachineLevelUserCloudPolicyManager>
  CreatePolicyManager(ConfigurationPolicyProvider* platform_provider);

  void Init(PrefService* local_state,
            scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);

  bool WaitUntilPolicyEnrollmentFinished();

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Returns whether the enterprise startup dialog is being diaplayed.
  bool IsEnterpriseStartupDialogShowing();

 protected:
  void NotifyPolicyRegisterFinished(bool succeeded);

 private:
  bool GetEnrollmentTokenAndClientId(std::string* enrollment_token,
                                     std::string* client_id);
  void RegisterForCloudManagementWithEnrollmentTokenCallback(
      const std::string& dm_token,
      const std::string& client_id);

  void CreateReportSchedulerAsync(
      scoped_refptr<base::SequencedTaskRunner> task_runner);
  void CreateReportScheduler();

  base::ObserverList<Observer, true>::Unchecked observers_;

  std::unique_ptr<ChromeBrowserCloudManagementRegistrar>
      cloud_management_registrar_;
  std::unique_ptr<MachineLevelUserCloudPolicyFetcher> policy_fetcher_;
  // This is an observer of the controller and needs to be declared after the
  // |observers_|.
  std::unique_ptr<ChromeBrowserCloudManagementRegisterWatcher>
      cloud_management_register_watcher_;

  // Time at which the enrollment process was started.  Used to log UMA metric.
  base::Time enrollment_start_time_;

  std::unique_ptr<enterprise_reporting::ReportScheduler> report_scheduler_;

  DISALLOW_COPY_AND_ASSIGN(ChromeBrowserCloudManagementController);
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_CHROME_BROWSER_CLOUD_MANAGEMENT_CONTROLLER_H_
