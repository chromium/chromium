// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_STRUCTURED_KEY_DATA_PROVIDER_ASH_H_
#define CHROME_BROWSER_METRICS_STRUCTURED_KEY_DATA_PROVIDER_ASH_H_

#include <memory>
#include <optional>

#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/metrics/structured/profile_observer.h"
#include "components/metrics/structured/lib/key_data_provider.h"

namespace metrics::structured {

// Implementation for Ash Chrome to provide keys to StructuredMetricsRecorder.
//
// Device keys are stored in a static path while profile keys are stored in the
// user's cryptohome partition.
//
// InitializeProfileKey should only be called once for the primary user. All
// subsequent calls will no-op.
class KeyDataProviderAsh : public KeyDataProvider,
                           public KeyDataProvider::Observer,
                           public ProfileObserver {
 public:
  KeyDataProviderAsh();
  KeyDataProviderAsh(const base::FilePath& device_key_path,
                     base::TimeDelta write_delay);
  ~KeyDataProviderAsh() override;

  // KeyDataProvider:
  bool IsReady() override;
  std::optional<uint64_t> GetId(const std::string& project_name) override;
  std::optional<uint64_t> GetSecondaryId(
      const std::string& project_name) override;
  KeyData* GetKeyData(const std::string& project_name) override;
  void Purge() override;

  // KeyDataProvider::Observer:
  void OnKeyReady() override;

  // ProfileObserver:
  void ProfileAdded(const Profile& profile) override;

 private:
  KeyDataProvider* GetKeyDataProvider(const std::string& project_name);

  const base::FilePath device_key_path_;
  const base::TimeDelta write_delay_;

  std::unique_ptr<KeyDataProvider> device_key_;
  std::unique_ptr<KeyDataProvider> profile_key_;

  base::WeakPtrFactory<KeyDataProviderAsh> weak_ptr_factory_{this};
};
}  // namespace metrics::structured

#endif  // CHROME_BROWSER_METRICS_STRUCTURED_KEY_DATA_PROVIDER_ASH_H_
