// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_REPORTING_EXTENSION_REQUEST_EXTENSION_REQUEST_OBSERVER_FACTORY_H_
#define CHROME_BROWSER_ENTERPRISE_REPORTING_EXTENSION_REQUEST_EXTENSION_REQUEST_OBSERVER_FACTORY_H_

#include <map>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/enterprise/reporting/extension_request/extension_request_observer.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager_observer.h"
#include "chrome/browser/profiles/profile_observer.h"

namespace enterprise_reporting {

// Factory class for ExtensionRequestObserver. It creates
// ExtensionRequestObserver for each Profile or a specific profile.
class ExtensionRequestObserverFactory : public ProfileManagerObserver,
                                        public ProfileObserver {
 public:
  // If a specific |profile| is given, this factory class only create an
  // observer for it. If no |profile| is given, this factory class create
  // observers for all loaded profiles respectively.
  explicit ExtensionRequestObserverFactory(Profile* profile = nullptr);
  ExtensionRequestObserverFactory(const ExtensionRequestObserverFactory&) =
      delete;
  ExtensionRequestObserverFactory& operator=(
      const ExtensionRequestObserverFactory&) = delete;
  ~ExtensionRequestObserverFactory() override;

  bool IsReportEnabled();
  void EnableReport(ExtensionRequestObserver::ReportTrigger trigger);
  void DisableReport();

  // ProfileManagerObserver
  void OnProfileAdded(Profile* profile) override;
  void OnProfileMarkedForPermanentDeletion(Profile* profile) override;

  // ProfileObserver
  // According to the destructor of ProfileImpl, the ExtensionManager may be
  // disposed before this class is released in the CrOS environment. So we use
  // this OnProfileWillBeDestroyed method to remove all observers earlier.
  void OnProfileWillBeDestroyed(Profile* profile) override;

  ExtensionRequestObserver* GetObserverByProfileForTesting(Profile* profile);
  int GetNumberOfObserversForTesting();

 private:
  raw_ptr<Profile> profile_;
  std::map<Profile*, std::unique_ptr<ExtensionRequestObserver>, ProfileCompare>
      observers_;
  ExtensionRequestObserver::ReportTrigger report_trigger_;
};

}  // namespace enterprise_reporting

#endif  // CHROME_BROWSER_ENTERPRISE_REPORTING_EXTENSION_REQUEST_EXTENSION_REQUEST_OBSERVER_FACTORY_H_
