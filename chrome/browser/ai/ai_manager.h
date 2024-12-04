// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AI_AI_MANAGER_H_
#define CHROME_BROWSER_AI_AI_MANAGER_H_

#include <optional>

#include "base/memory/weak_ptr.h"
#include "base/supports_user_data.h"
#include "base/types/pass_key.h"
#include "chrome/browser/ai/ai_context_bound_object_set.h"
#include "chrome/browser/ai/ai_create_on_device_session_task.h"
#include "chrome/browser/ai/ai_language_model.h"
#include "chrome/browser/ai/ai_on_device_model_component_observer.h"
#include "chrome/browser/ai/ai_summarizer.h"
#include "content/public/browser/browser_context.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "third_party/blink/public/mojom/ai/ai_language_model.mojom-forward.h"
#include "third_party/blink/public/mojom/ai/ai_manager.mojom.h"
#include "third_party/blink/public/mojom/ai/model_download_progress_observer.mojom-forward.h"

namespace base {
class SupportsUserData;
}  // namespace base

// Owned by the host of the document / service worker via `SupportUserData`.
// The browser-side implementation of `blink::mojom::AIManager`.
class AIManager : public base::SupportsUserData::Data,
                  public blink::mojom::AIManager {
 public:
  using AILanguageModelOrCreationError =
      base::expected<std::unique_ptr<AILanguageModel>,
                     blink::mojom::AIManagerCreateLanguageModelError>;
  explicit AIManager(content::BrowserContext* browser_context);
  AIManager(const AIManager&) = delete;
  AIManager& operator=(const AIManager&) = delete;

  ~AIManager() override;

  void AddReceiver(mojo::PendingReceiver<blink::mojom::AIManager> receiver);
  void CreateLanguageModelForCloning(
      base::PassKey<AILanguageModel> pass_key,
      blink::mojom::AILanguageModelSamplingParamsPtr sampling_params,
      AIContextBoundObjectSet& context_bound_object_set,
      const AILanguageModel::Context& context,
      mojo::Remote<blink::mojom::AIManagerCreateLanguageModelClient>
          client_remote);

  size_t GetContextBoundObjectSetSizeForTesting() {
    return context_bound_object_set_.GetSizeForTesting();
  }

  size_t GetDownloadProgressObserversSizeForTesting() {
    return download_progress_observers_.size();
  }
  void SendDownloadProgressUpdateForTesting(uint64_t downloaded_bytes,
                                            uint64_t total_bytes);

  void OnTextModelDownloadProgressChange(
      base::PassKey<AIOnDeviceModelComponentObserver> observer_key,
      uint64_t downloaded_bytes,
      uint64_t total_bytes);

  // TODO(crbug.com/372349624): make the max sampling params configured from the
  // model execution config as well.
  // Return the max top k value for the LanguageModel API. Note that this value
  // won't exceed the max top k defined by the underlying on-device model.
  uint32_t GetLanguageModelMaxTopK();

  // Return the default sampling params for the LanguageModel API.
  optimization_guide::SamplingParams GetLanguageModelDefaultSamplingParams();

 private:
  FRIEND_TEST_ALL_PREFIXES(AIManagerTest, NoUAFWithInvalidOnDeviceModelPath);
  FRIEND_TEST_ALL_PREFIXES(AISummarizerUnitTest,
                           CreateSummarizerWithoutService);

  // `blink::mojom::AIManager` implementation.
  void CanCreateLanguageModel(CanCreateLanguageModelCallback callback) override;
  void CreateLanguageModel(
      mojo::PendingRemote<blink::mojom::AIManagerCreateLanguageModelClient>
          client,
      blink::mojom::AILanguageModelCreateOptionsPtr options) override;
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
                        CanCreateLanguageModelCallback callback);

  // Creates an `AILanguageModel`, either as a new session, or as a clone of
  // an existing session with its context copied. When this method is called
  // during the session cloning, the optional `context` variable should be set
  // to the existing `AILanguageModel`'s session.
  // The `CreateLanguageModelOnDeviceSessionTask` will be returned and the
  // caller is responsible for keeping it alive if the task is waiting for the
  // model to be available.
  std::unique_ptr<CreateLanguageModelOnDeviceSessionTask>
  CreateLanguageModelInternal(
      const blink::mojom::AILanguageModelSamplingParamsPtr& sampling_params,
      AIContextBoundObjectSet& context_bound_object_set,
      base::OnceCallback<void(AILanguageModelOrCreationError)> callback,
      const std::optional<const AILanguageModel::Context>& context =
          std::nullopt);

  void SendDownloadProgressUpdate(uint64_t downloaded_bytes,
                                  uint64_t total_bytes);

  mojo::ReceiverSet<blink::mojom::AIManager> receivers_;
  mojo::RemoteSet<blink::mojom::ModelDownloadProgressObserver>
      download_progress_observers_;
  std::unique_ptr<AIOnDeviceModelComponentObserver> component_observer_;

  // Since it requires creating a default session to fetch the default sampling
  // params, we keep a lazy-initialized instance here as a cache.
  std::optional<optimization_guide::SamplingParams>
      default_language_model_sampling_params_;

  AIContextBoundObjectSet context_bound_object_set_;
  raw_ptr<content::BrowserContext> browser_context_;

  base::WeakPtrFactory<AIManager> weak_factory_{this};
};

#endif  // CHROME_BROWSER_AI_AI_MANAGER_H_
