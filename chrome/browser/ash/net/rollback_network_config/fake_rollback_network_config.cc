// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/net/rollback_network_config/fake_rollback_network_config.h"

#include <optional>
#include <utility>

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/values.h"

namespace {
constexpr char kEmptyConfig[] = "{\"NetworkConfigurations\":[]}";
}

namespace ash {

FakeRollbackNetworkConfig::FakeRollbackNetworkConfig() = default;
FakeRollbackNetworkConfig::~FakeRollbackNetworkConfig() = default;

void FakeRollbackNetworkConfig::RollbackConfigImport(const std::string& config,
                                                     ImportCallback callback) {
  imported_config_ = base::JSONReader::Read(config);
  if (config_imported_callback_) {
    std::move(config_imported_callback_).Run();
  }
  std::move(callback).Run(/*success=*/imported_config_.has_value());
}

void FakeRollbackNetworkConfig::RollbackConfigExport(ExportCallback callback) {
  if (imported_config_.has_value()) {
    std::string serialized_config;
    base::JSONWriter::Write(*imported_config_, &serialized_config);
    std::move(callback).Run(serialized_config);
  } else {
    std::move(callback).Run(kEmptyConfig);
  }
}

}  // namespace ash
