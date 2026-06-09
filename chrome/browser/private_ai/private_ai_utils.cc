// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/private_ai/private_ai_utils.h"

#include <set>
#include <vector>

#include "base/feature_list.h"
#include "components/optimization_guide/core/model_execution/model_execution_features.h"
#include "components/variations/variations_ids_provider.h"

namespace private_ai {

void PopulatePaicMessage(
    proto::FeatureName feature_name,
    const optimization_guide::proto::ExecuteRequest& execute_request,
    proto::PaicMessage* paic_message) {
  paic_message->set_feature_name(feature_name);
  if (base::FeatureList::IsEnabled(
          optimization_guide::features::internal::kPrivateExecuteRequest)) {
    auto* private_execute_request =
        paic_message->mutable_private_execute_request_ext();
    *private_execute_request->mutable_request() = execute_request;

    const std::set<variations::IDCollectionKey> web_properties_keys{
        variations::GOOGLE_WEB_PROPERTIES_ANY_CONTEXT,
        variations::GOOGLE_WEB_PROPERTIES_FIRST_PARTY,
        variations::GOOGLE_WEB_PROPERTIES_TRIGGER_ANY_CONTEXT,
        variations::GOOGLE_WEB_PROPERTIES_TRIGGER_FIRST_PARTY,
    };
    std::vector<variations::VariationID> variations =
        variations::VariationsIdsProvider::GetInstance()->GetVariationsVector(
            web_properties_keys);
    for (auto variation : variations) {
      private_execute_request->add_variations(variation);
    }
  } else {
    *paic_message->mutable_execute_request_ext() = execute_request;
  }
}

}  // namespace private_ai
