// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_STRUCTURED_KEY_DATA_PROVIDER_ASH_H_
#define CHROME_BROWSER_METRICS_STRUCTURED_KEY_DATA_PROVIDER_ASH_H_

#include "components/metrics/structured/key_data_provider.h"

namespace metrics::structured {

// Implementation for Ash Chrome to provide keys to StructuredMetricsRecorder.
//
// Device keys are stored in a static path while profile keys are stored in the
// user's cryptohome partition.
//
// InitializeProfileKey should only be called once for the primary user. All
// subsequent calls will no-op.
class KeyDataProviderAsh : public KeyDataProvider {
 public:
  KeyDataProviderAsh(const base::FilePath& device_key_path, int write_delay_ms);
  KeyDataProviderAsh();
  ~KeyDataProviderAsh() override;

  // KeyDataProvider:
  void InitializeDeviceKey(base::OnceClosure callback) override;
  void InitializeProfileKey(const base::FilePath& profile_path,
                            base::OnceClosure callback) override;
  KeyData* GetDeviceKeyData() override;
  KeyData* GetProfileKeyData() override;
  void Purge() override;
  bool HasProfileKey() override;
  bool HasDeviceKey() override;

 private:
  const base::FilePath device_key_path_;
  int write_delay_ms_;

  std::unique_ptr<KeyData> device_key_;
  std::unique_ptr<KeyData> profile_key_;
};
}  // namespace metrics::structured

#endif  // CHROME_BROWSER_METRICS_STRUCTURED_KEY_DATA_PROVIDER_ASH_H_
