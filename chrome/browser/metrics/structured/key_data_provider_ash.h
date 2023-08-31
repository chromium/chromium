// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_STRUCTURED_KEY_DATA_PROVIDER_ASH_H_
#define CHROME_BROWSER_METRICS_STRUCTURED_KEY_DATA_PROVIDER_ASH_H_

#include <memory>

#include "base/files/file_path.h"
#include "base/time/time.h"
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
  KeyDataProviderAsh();
  KeyDataProviderAsh(const base::FilePath& device_key_path,
                     base::TimeDelta write_delay);
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
  const base::TimeDelta write_delay_;

  std::unique_ptr<KeyData> device_key_;
  std::unique_ptr<KeyData> profile_key_;
};
}  // namespace metrics::structured

#endif  // CHROME_BROWSER_METRICS_STRUCTURED_KEY_DATA_PROVIDER_ASH_H_
