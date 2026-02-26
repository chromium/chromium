// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_OPTIMIZATION_GUIDE_PRIVATE_AI_MODEL_EXECUTION_FETCHER_H_
#define CHROME_BROWSER_OPTIMIZATION_GUIDE_PRIVATE_AI_MODEL_EXECUTION_FETCHER_H_

#include <optional>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "components/optimization_guide/core/model_execution/feature_keys.h"
#include "components/optimization_guide/core/model_execution/model_execution_fetcher.h"

namespace private_ai {
class Client;
}  // namespace private_ai

namespace optimization_guide {

class PrivateAiModelExecutionFetcher : public ModelExecutionFetcher {
 public:
  explicit PrivateAiModelExecutionFetcher(
      private_ai::Client* private_ai_client);
  ~PrivateAiModelExecutionFetcher() override;

  PrivateAiModelExecutionFetcher(const PrivateAiModelExecutionFetcher&) =
      delete;
  PrivateAiModelExecutionFetcher& operator=(
      const PrivateAiModelExecutionFetcher&) = delete;

  // ModelExecutionFetcher:
  void ExecuteModel(ModelBasedCapabilityKey feature,
                    signin::IdentityManager* identity_manager,
                    const google::protobuf::MessageLite& request_metadata,
                    std::optional<base::TimeDelta> timeout,
                    ModelExecuteResponseCallback callback) override;

 private:
  // Unowned pointer to the PrivateAI client. The owner of this object needs
  // to ensure that all fetchers are destroyed before the client.
  raw_ptr<private_ai::Client> private_ai_client_;

  base::WeakPtrFactory<PrivateAiModelExecutionFetcher> weak_ptr_factory_{this};
};

}  // namespace optimization_guide

#endif  // CHROME_BROWSER_OPTIMIZATION_GUIDE_PRIVATE_AI_MODEL_EXECUTION_FETCHER_H_
