// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_EXTENSIONS_METRICS_PROVIDER_H_
#define CHROME_BROWSER_METRICS_EXTENSIONS_METRICS_PROVIDER_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/time/time.h"
#include "chrome/browser/metrics/cached_metrics_profile.h"
#include "components/metrics/metrics_provider.h"
#include "third_party/metrics_proto/extension_install.pb.h"

class Profile;

namespace extensions {
class Extension;
class ExtensionPrefs;
class ExtensionSet;
}

namespace metrics {
class MetricsStateManager;
class SystemProfileProto;
}

// ExtensionsMetricsProvider groups various constants and functions used for
// reporting extension IDs with UMA reports (after hashing the extension IDs
// for privacy).
class ExtensionsMetricsProvider : public metrics::MetricsProvider {
 public:
  // Holds on to |metrics_state_manager|, which must outlive this object, as a
  // weak pointer.
  explicit ExtensionsMetricsProvider(
      metrics::MetricsStateManager* metrics_state_manager);
  ~ExtensionsMetricsProvider() override;

  // metrics::MetricsProvider:
  void ProvideSystemProfileMetrics(
      metrics::SystemProfileProto* system_profile) override;

  static metrics::ExtensionInstallProto ConstructInstallProtoForTesting(
      const extensions::Extension& extension,
      extensions::ExtensionPrefs* prefs,
      base::Time last_sample_time);
  static std::vector<metrics::ExtensionInstallProto>
  GetInstallsForProfileForTesting(Profile* profile,
                                  base::Time last_sample_time);

 protected:
  // Exposed for the sake of mocking in test code.

  // Retrieves the set of extensions installed in the given |profile|.
  virtual std::unique_ptr<extensions::ExtensionSet> GetInstalledExtensions(
      Profile* profile);

  // Retrieves the client ID.
  virtual uint64_t GetClientID();

  // Hashes the extension extension ID using the provided client key (which
  // must be less than kExtensionListClientKeys) and to produce an output value
  // between 0 and kExtensionListBuckets-1.
  static int HashExtension(const std::string& extension_id,
                           uint32_t client_key);

 private:
  // Writes whether any loaded profiles have extensions not from the webstore.
  void ProvideOffStoreMetric(metrics::SystemProfileProto* system_profile);

  // Writes the hashed list of installed extensions into the specified
  // SystemProfileProto object.
  void ProvideOccupiedBucketMetric(metrics::SystemProfileProto* system_profile);

  // Writes information about the installed extensions for all profiles into
  // the proto.
  void ProvideExtensionInstallsMetrics(
      metrics::SystemProfileProto* system_profile);

  // The MetricsStateManager from which the client ID is obtained.
  metrics::MetricsStateManager* metrics_state_manager_;

  metrics::CachedMetricsProfile cached_profile_;

  // The time of our last recorded sample.
  base::Time last_sample_time_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionsMetricsProvider);
};

#endif  // CHROME_BROWSER_METRICS_EXTENSIONS_METRICS_PROVIDER_H_
