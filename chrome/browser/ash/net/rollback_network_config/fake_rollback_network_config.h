// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_NET_ROLLBACK_NETWORK_CONFIG_FAKE_ROLLBACK_NETWORK_CONFIG_H_
#define CHROME_BROWSER_ASH_NET_ROLLBACK_NETWORK_CONFIG_FAKE_ROLLBACK_NETWORK_CONFIG_H_

#include <optional>

#include "base/functional/callback.h"
#include "base/values.h"
#include "chrome/browser/ash/net/rollback_network_config/rollback_network_config.h"

namespace ash {

class FakeRollbackNetworkConfig : public RollbackNetworkConfig {
 public:
  FakeRollbackNetworkConfig();
  FakeRollbackNetworkConfig(const FakeRollbackNetworkConfig&) = delete;
  FakeRollbackNetworkConfig& operator=(const FakeRollbackNetworkConfig&) =
      delete;
  ~FakeRollbackNetworkConfig() override;

  void RollbackConfigImport(const std::string& config,
                            ImportCallback callback) override;
  void RollbackConfigExport(ExportCallback callback) override;

  base::Value* imported_config() {
    if (imported_config_.has_value()) {
      return &imported_config_.value();
    }
    return nullptr;
  }

  void RegisterImportClosure(base::OnceClosure config_imported_callback) {
    config_imported_callback_ = std::move(config_imported_callback);
  }

 private:
  std::optional<base::Value> imported_config_ = std::nullopt;
  base::OnceClosure config_imported_callback_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_NET_ROLLBACK_NETWORK_CONFIG_FAKE_ROLLBACK_NETWORK_CONFIG_H_
