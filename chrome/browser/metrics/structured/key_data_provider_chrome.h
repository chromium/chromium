// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_STRUCTURED_KEY_DATA_PROVIDER_CHROME_H_
#define CHROME_BROWSER_METRICS_STRUCTURED_KEY_DATA_PROVIDER_CHROME_H_

#include <string>

#include "components/metrics/structured/key_data_provider_prefs.h"
#include "components/metrics/structured/lib/key_data_provider.h"
#include "components/metrics/structured/lib/proto/key.pb.h"

class PrefRegistrySimple;
class PrefService;

namespace metrics::structured {

// Implementation of KeyDataProvider for Chrome platforms.
//
// Currently, only device keys are supported due to the current use case only
// needing device id's.
class KeyDataProviderChrome : public KeyDataProvider {
 public:
  explicit KeyDataProviderChrome(PrefService* local_state);

  KeyDataProviderChrome(const KeyDataProviderChrome&) = delete;
  KeyDataProviderChrome& operator=(const KeyDataProviderChrome&) = delete;

  ~KeyDataProviderChrome() override;

  // KeyDataProvider:
  bool IsReady() override;
  std::optional<uint64_t> GetId(const std::string& project_name) override;
  KeyData* GetKeyData(const std::string& project_name) override;
  void Purge() override;

  static void RegisterLocalState(PrefRegistrySimple* registry);

 private:
  KeyDataProviderPrefs device_key_;
};

}  // namespace metrics::structured

#endif  // CHROME_BROWSER_METRICS_STRUCTURED_KEY_DATA_PROVIDER_CHROME_H_
