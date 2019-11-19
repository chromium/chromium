// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DATA_REDUCTION_PROXY_DATA_REDUCTION_PROXY_CHROME_SETTINGS_H_
#define CHROME_BROWSER_DATA_REDUCTION_PROXY_DATA_REDUCTION_PROXY_CHROME_SETTINGS_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_request_options.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_settings.h"
#include "components/keyed_service/core/keyed_service.h"

class PrefService;
class Profile;

namespace base {
class SequencedTaskRunner;
}  // namespace base

namespace content {
class NavigationHandle;
}

namespace data_reduction_proxy {
class DataReductionProxyData;
class DataStore;
}  // namespace data_reduction_proxy

class PrefService;

// Data reduction proxy settings class suitable for use with a Chrome browser.
// It is keyed to a browser context.
class DataReductionProxyChromeSettings
    : public data_reduction_proxy::DataReductionProxySettings,
      public KeyedService {
 public:
  // Enum values that can be reported for the
  // DataReductionProxy.ProxyPrefMigrationResult histogram. These values must be
  // kept in sync with their counterparts in histograms.xml. Visible here for
  // testing purposes.
  enum ProxyPrefMigrationResult {
    PROXY_PREF_NOT_CLEARED = 0,
    PROXY_PREF_CLEARED_EMPTY,
    PROXY_PREF_CLEARED_MODE_SYSTEM,
    PROXY_PREF_CLEARED_DRP,
    PROXY_PREF_CLEARED_GOOGLEZIP,
    PROXY_PREF_CLEARED_PAC_GOOGLEZIP,
    PROXY_PREF_MAX
  };

  // Constructs a settings object. Construction and destruction must happen on
  // the UI thread.
  explicit DataReductionProxyChromeSettings(bool is_off_the_record_profile);

  // Destructs the settings object.
  ~DataReductionProxyChromeSettings() override;

  // Overrides KeyedService::Shutdown:
  void Shutdown() override;

  // Initialize the settings object with the given profile, data store, and db
  // task runner.
  void InitDataReductionProxySettings(
      Profile* profile,
      std::unique_ptr<data_reduction_proxy::DataStore> store,
      const scoped_refptr<base::SequencedTaskRunner>& db_task_runner);

  // Gets the client type for the data reduction proxy.
  static data_reduction_proxy::Client GetClient();

  // Public for testing.
  void MigrateDataReductionProxyOffProxyPrefs(PrefService* prefs);

  void SetIgnoreLongTermBlackListRules(
      bool ignore_long_term_black_list_rules) override;

  // Builds an instance of DataReductionProxyData from the given |handle| and
  // |headers|.
  std::unique_ptr<data_reduction_proxy::DataReductionProxyData>
  CreateDataFromNavigationHandle(content::NavigationHandle* handle,
                                 const net::HttpResponseHeaders* headers);

  // This data will be used on the next commit if it's HTTP/HTTPS and the page
  // is not an error page..
  void SetDataForNextCommitForTesting(
      std::unique_ptr<data_reduction_proxy::DataReductionProxyData> data);

 private:
  // Helper method for migrating the Data Reduction Proxy away from using the
  // proxy pref. Returns the ProxyPrefMigrationResult value indicating the
  // migration action taken.
  ProxyPrefMigrationResult MigrateDataReductionProxyOffProxyPrefsHelper(
      PrefService* prefs);

  // Null before InitDataReductionProxySettings is called.
  Profile* profile_;

  std::unique_ptr<data_reduction_proxy::DataReductionProxyData> test_data_;

  DISALLOW_COPY_AND_ASSIGN(DataReductionProxyChromeSettings);
};

#endif  // CHROME_BROWSER_DATA_REDUCTION_PROXY_DATA_REDUCTION_PROXY_CHROME_SETTINGS_H_
