// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AI_AI_CLASSIFIER_H_
#define CHROME_BROWSER_AI_AI_CLASSIFIER_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ai/ai_context_bound_object.h"
#include "chrome/browser/ai/ai_on_device_session.h"
#include "components/optimization_guide/proto/features/classify_api.pb.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "third_party/blink/public/mojom/ai/ai_classifier.mojom.h"
#include "third_party/blink/public/mojom/ai/model_streaming_responder.mojom-forward.h"

class AIClassifier : public AIContextBoundObject,
                     public blink::mojom::AIClassifier {
 public:
  AIClassifier(
      AIContextBoundObjectSet& context_bound_object_set,
      std::unique_ptr<optimization_guide::OnDeviceSession> classifier_session,
      blink::mojom::AIClassifierCreateOptionsPtr options,
      mojo::PendingReceiver<blink::mojom::AIClassifier> receiver);

  // `blink::mojom::AIClassifier` implementation.
  void Classify(const std::string& input,
                const std::string& context,
                mojo::PendingRemote<blink::mojom::ModelStreamingResponder>
                    pending_responder) override;

  // AIContextBoundObject:
  void SetPriority(on_device_model::mojom::Priority priority) override;

  ~AIClassifier() override;

 private:
  void ModelExecutionCallback(
      mojo::RemoteSetElementId responder_id,
      optimization_guide::OptimizationGuideModelStreamingExecutionResult
          result);

  void DidGetExecutionInputSizeForClassify(
      mojo::RemoteSetElementId responder_id,
      const optimization_guide::proto::ClassifyApiRequest& request,
      std::optional<uint32_t> result);

  optimization_guide::proto::ClassifyApiRequest BuildRequest(
      const std::string& input);

  AIOnDeviceSession session_wrapper_;

  // The `RemoteSet` storing all the responders, each of them corresponds to one
  // `Classify()` call.
  mojo::RemoteSet<blink::mojom::ModelStreamingResponder> responder_set_;

  mojo::Receiver<blink::mojom::AIClassifier> receiver_;

  const blink::mojom::AIClassifierCreateOptionsPtr options_;

  base::WeakPtrFactory<AIClassifier> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_AI_AI_CLASSIFIER_H_
