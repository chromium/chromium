// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/optimization_guide/legion_model_execution_fetcher.h"

#include <utility>

#include "components/legion/client.h"
#include "components/optimization_guide/core/model_execution/model_execution_fetcher.h"
#include "components/optimization_guide/proto/model_execution.pb.h"

namespace optimization_guide {

LegionModelExecutionFetcher::LegionModelExecutionFetcher(
    legion::Client* legion_client)
    : legion_client_(legion_client) {
  CHECK(legion_client);
}

LegionModelExecutionFetcher::~LegionModelExecutionFetcher() = default;

void LegionModelExecutionFetcher::ExecuteModel(
    ModelBasedCapabilityKey feature,
    signin::IdentityManager* identity_manager,
    const google::protobuf::MessageLite& request_metadata,
    std::optional<base::TimeDelta> timeout,
    ModelExecuteResponseCallback callback) {
  // TODO(crbug.com/460052805): Actually use legion client.
  std::move(callback).Run(base::ok(proto::ExecuteResponse{}));
}

}  // namespace optimization_guide
