// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AI_AI_MANAGER_KEYED_SERVICE_H_
#define CHROME_BROWSER_AI_AI_MANAGER_KEYED_SERVICE_H_

#include <optional>

#include "base/memory/weak_ptr.h"
#include "base/types/pass_key.h"
#include "chrome/browser/ai/ai_assistant.h"
#include "chrome/browser/ai/ai_context_bound_object_set.h"
#include "chrome/browser/ai/ai_create_on_device_session_task.h"
#include "chrome/browser/ai/ai_on_device_model_component_observer.h"
#include "chrome/browser/ai/ai_summarizer.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/browser_context.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "third_party/blink/public/mojom/ai/ai_assistant.mojom-forward.h"
#include "third_party/blink/public/mojom/ai/ai_manager.mojom.h"
#include "third_party/blink/public/mojom/ai/model_download_progress_observer.mojom-forward.h"

// The browser-side implementation of `blink::mojom::AIManager`. There should
// be one shared AIManagerKeyedService per BrowserContext.
class AIManagerKeyedService : public KeyedService,
                              public blink::mojom::AIManager {
 public:
  explicit AIManagerKeyedService(content::BrowserContext* browser_context);
  AIManagerKeyedService(const AIManagerKeyedService&) = delete;
  AIManagerKeyedService& operator=(const AIManagerKeyedService&) = delete;

  ~AIManagerKeyedService() override;

  void AddReceiver(mojo::PendingReceiver<blink::mojom::AIManager> receiver,
                   AIContextBoundObjectSet::ReceiverContext host);
  void CreateAssistantForCloning(
      base::PassKey<AIAssistant> pass_key,
      blink::mojom::AIAssistantSamplingParamsPtr sampling_params,
      AIContextBoundObjectSet& context_bound_object_set,
      const AIAssistant::Context& context,
      mojo::Remote<blink::mojom::AIManagerCreateAssistantClient> client_remote);

  size_t GetReceiversSizeForTesting() { return receivers_.size(); }
  size_t GetDownloadProgressObserversSizeForTesting() {
    return download_progress_observers_.size();
  }
  void SendDownloadProgressUpdateForTesting(uint64_t downloaded_bytes,
                                            uint64_t total_bytes);

  void OnTextModelDownloadProgressChange(
      base::PassKey<AIOnDeviceModelComponentObserver> observer_key,
      uint64_t downloaded_bytes,
      uint64_t total_bytes);

  // Return the max top k value for the Assistant API. Note that this value
  // won't exceed the max top k defined by the underlying on-device model.
  static int GetAssistantModelMaxTopK();

  // Return the default temperature for the Assistant API.
  static double GetAssistantModelDefaultTemperature();

 private:
  FRIEND_TEST_ALL_PREFIXES(AIManagerKeyedServiceTest,
                           NoUAFWithInvalidOnDeviceModelPath);
  FRIEND_TEST_ALL_PREFIXES(AISummarizerUnitTest,
                           CreateSummarizerWithoutService);

  // `blink::mojom::AIManager` implementation.
  void CanCreateAssistant(CanCreateAssistantCallback callback) override;
  void CreateAssistant(
      mojo::PendingRemote<blink::mojom::AIManagerCreateAssistantClient> client,
      blink::mojom::AIAssistantCreateOptionsPtr options) override;
  void GetModelInfo(GetModelInfoCallback callback) override;
  void CreateWriter(
      mojo::PendingRemote<blink::mojom::AIManagerCreateWriterClient> client,
      blink::mojom::AIWriterCreateOptionsPtr options) override;
  void CanCreateSummarizer(CanCreateSummarizerCallback callback) override;
  void CreateSummarizer(
      mojo::PendingRemote<blink::mojom::AIManagerCreateSummarizerClient> client,
      blink::mojom::AISummarizerCreateOptionsPtr options) override;
  void CreateRewriter(
      mojo::PendingRemote<blink::mojom::AIManagerCreateRewriterClient> client,
      blink::mojom::AIRewriterCreateOptionsPtr options) override;
  void AddModelDownloadProgressObserver(
      mojo::PendingRemote<blink::mojom::ModelDownloadProgressObserver>
          observer_remote) override;

  void OnModelPathValidationComplete(const std::string& model_path,
                                     bool is_valid_path);

  void CanCreateSession(optimization_guide::ModelBasedCapabilityKey capability,
                        CanCreateAssistantCallback callback);

  void RemoveReceiver(mojo::ReceiverId receiver_id);

  // Creates an `AIAssistant`, either as a new session, or as a clone of
  // an existing session with its context copied.
  // - When this method is called during the session cloning, the optional
  // `context` variable should be set to the existing `AIAssistant`'s session.
  // - When this method is called during new session creation, the optional
  // `receiver_context` should be set to the corresponding `ReceiverContext`
  // since the `CreateAssistantOnDeviceSessionTask` might be waiting for the
  // on-device model availability changes, and it needs to be kept-alive in the
  // `ReceiverContext`.
  void CreateAssistantInternal(
      const blink::mojom::AIAssistantSamplingParamsPtr& sampling_params,
      AIContextBoundObjectSet& context_bound_object_set,
      base::OnceCallback<void(std::unique_ptr<AIAssistant>)> callback,
      const std::optional<const AIAssistant::Context>& context = std::nullopt,
      const std::optional<AIContextBoundObjectSet::ReceiverContext>
          receiver_context = std::nullopt);

  void SendDownloadProgressUpdate(uint64_t downloaded_bytes,
                                  uint64_t total_bytes);

  // A `KeyedService` should never outlive the `BrowserContext`.
  raw_ptr<content::BrowserContext> browser_context_;

  mojo::ReceiverSet<blink::mojom::AIManager,
                    AIContextBoundObjectSet::ReceiverContext>
      receivers_;
  mojo::RemoteSet<blink::mojom::ModelDownloadProgressObserver>
      download_progress_observers_;
  std::unique_ptr<AIOnDeviceModelComponentObserver> component_observer_;

  base::WeakPtrFactory<AIManagerKeyedService> weak_factory_{this};
};

#endif  // CHROME_BROWSER_AI_AI_MANAGER_KEYED_SERVICE_H_
