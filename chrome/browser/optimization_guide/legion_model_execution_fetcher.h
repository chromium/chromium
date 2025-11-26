// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_OPTIMIZATION_GUIDE_LEGION_MODEL_EXECUTION_FETCHER_H_
#define CHROME_BROWSER_OPTIMIZATION_GUIDE_LEGION_MODEL_EXECUTION_FETCHER_H_

#include <optional>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "components/optimization_guide/core/model_execution/feature_keys.h"
#include "components/optimization_guide/core/model_execution/model_execution_fetcher.h"

namespace legion {
class Client;
}  // namespace legion

namespace optimization_guide {

class LegionModelExecutionFetcher : public ModelExecutionFetcher {
 public:
  explicit LegionModelExecutionFetcher(legion::Client* legion_client);
  ~LegionModelExecutionFetcher() override;

  LegionModelExecutionFetcher(const LegionModelExecutionFetcher&) = delete;
  LegionModelExecutionFetcher& operator=(const LegionModelExecutionFetcher&) =
      delete;

  // ModelExecutionFetcher:
  void ExecuteModel(ModelBasedCapabilityKey feature,
                    signin::IdentityManager* identity_manager,
                    const google::protobuf::MessageLite& request_metadata,
                    std::optional<base::TimeDelta> timeout,
                    ModelExecuteResponseCallback callback) override;

 private:
  // Unowned pointer to the legion client. The owner of this object needs
  // to ensure that all fetchers are destroyed before the client.
  raw_ptr<legion::Client> legion_client_;

  base::WeakPtrFactory<LegionModelExecutionFetcher> weak_ptr_factory_{this};
};

}  // namespace optimization_guide

#endif  // CHROME_BROWSER_OPTIMIZATION_GUIDE_LEGION_MODEL_EXECUTION_FETCHER_H_
