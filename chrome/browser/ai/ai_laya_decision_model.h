// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AI_AI_LAYA_DECISION_MODEL_H_
#define CHROME_BROWSER_AI_AI_LAYA_DECISION_MODEL_H_

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ai/ai_context_bound_object.h"
#include "chrome/browser/ai/decision_model_schema_compiler.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/on_device_model/public/mojom/on_device_model.mojom.h"
#include "third_party/blink/public/mojom/ai/ai_decision_model.mojom.h"

class AIContextBoundObjectSet;

namespace optimization_guide {
class ModelClient;
}  // namespace optimization_guide

// Browser-side implementation of `blink::mojom::AIDecisionModel` backed by a
// Laya decision model in the on-device model service.
class AILayaDecisionModel : public AIContextBoundObject,
                            public blink::mojom::AIDecisionModel {
 public:
  // Maximum candidate options per question supported by the Laya model.
  static constexpr size_t kMaxOptions = 16;

  AILayaDecisionModel(
      AIContextBoundObjectSet& context_bound_object_set,
      DecisionModelSchemaCompiler::CompiledSchema schema,
      base::WeakPtr<optimization_guide::ModelClient> model_client,
      mojo::PendingReceiver<blink::mojom::AIDecisionModel> receiver);
  AILayaDecisionModel(const AILayaDecisionModel&) = delete;
  AILayaDecisionModel& operator=(const AILayaDecisionModel&) = delete;
  ~AILayaDecisionModel() override;

  // blink::mojom::AIDecisionModel:
  void Decide(const std::string& input, DecideCallback callback) override;

 private:
  void EnsureSessionConnected();
  void OnDecideComplete(
      DecideCallback callback,
      std::optional<
          std::vector<on_device_model::mojom::DecisionQuestionResultPtr>>
          results);

  const DecisionModelSchemaCompiler::CompiledSchema schema_;
  base::WeakPtr<optimization_guide::ModelClient> model_client_;

  mojo::Remote<on_device_model::mojom::DecisionSession> session_;
  mojo::Receiver<blink::mojom::AIDecisionModel> receiver_;

  base::WeakPtrFactory<AILayaDecisionModel> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_AI_AI_LAYA_DECISION_MODEL_H_
