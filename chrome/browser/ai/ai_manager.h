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
#include "chrome/browser/ai/ai_utils.h"
#include "content/public/browser/browser_context.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "third_party/blink/public/mojom/ai/ai_common.mojom-forward.h"
#include "third_party/blink/public/mojom/ai/ai_common.mojom.h"
#include "third_party/blink/public/mojom/ai/ai_language_model.mojom-forward.h"
#include "third_party/blink/public/mojom/ai/ai_manager.mojom.h"
#include "third_party/blink/public/mojom/ai/model_download_progress_observer.mojom-forward.h"

namespace base {
class SupportsUserData;
}  // namespace base

using blink::mojom::AILanguageCodePtr;

// Owned by the host of the document / service worker via `SupportUserData`.
// The browser-side implementation of `blink::mojom::AIManager`.
class AIManager : public base::SupportsUserData::Data,
                  public blink::mojom::AIManager {
 public:
  using AILanguageModelOrCreationError =
      base::expected<std::unique_ptr<AILanguageModel>,
                     blink::mojom::AIManagerCreateClientError>;
  explicit AIManager(content::BrowserContext* browser_context);
  AIManager(const AIManager&) = delete;
  AIManager& operator=(const AIManager&) = delete;

  ~AIManager() override;

  void AddReceiver(mojo::PendingReceiver<blink::mojom::AIManager> receiver);
  void CreateLanguageModelForCloning(
      base::PassKey<AILanguageModel> pass_key,
      blink::mojom::AILanguageModelSamplingParamsPtr sampling_params,
      AIContextBoundObjectSet& context_bound_object_set,
      AIUtils::LanguageCodes expected_input_languages,
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

  // Return the max top k value for the LanguageModel API. Note that this value
  // won't exceed the max top k defined by the underlying on-device model.
  uint32_t GetLanguageModelMaxTopK();
  // Return the max temperature for the LanguageModel API.
  float GetLanguageModelMaxTemperature();

  // Return the default and max sampling params for the LanguageModel API.
  blink::mojom::AILanguageModelParamsPtr GetLanguageModelParams();

 private:
  FRIEND_TEST_ALL_PREFIXES(AIManagerTest, CanCreate);
  FRIEND_TEST_ALL_PREFIXES(AIManagerTest, NoUAFWithInvalidOnDeviceModelPath);
  FRIEND_TEST_ALL_PREFIXES(AISummarizerUnitTest,
                           CreateSummarizerWithoutService);
  FRIEND_TEST_ALL_PREFIXES(AIManagerIsLanguagesSupportedTest, OneVector);
  FRIEND_TEST_ALL_PREFIXES(AIManagerIsLanguagesSupportedTest,
                           TwoVectorsAndOneCode);

  // Returns if all of the language codes in `languages` are supported.
  static bool IsLanguagesSupported(
      const std::vector<AILanguageCodePtr>& languages);

  // Returns if `output` and all of the language codes in `input` and `context`
  // are supported.
  static bool IsLanguagesSupported(
      const std::vector<AILanguageCodePtr>& input,
      const std::vector<AILanguageCodePtr>& context,
      const AILanguageCodePtr& output);

  // `blink::mojom::AIManager` implementation.
  void CanCreateLanguageModel(
      std::optional<std::vector<blink::mojom::AILanguageCodePtr>>
          expected_input_languages,
      CanCreateLanguageModelCallback callback) override;
  void CreateLanguageModel(
      mojo::PendingRemote<blink::mojom::AIManagerCreateLanguageModelClient>
          client,
      blink::mojom::AILanguageModelCreateOptionsPtr options) override;
  void GetLanguageModelParams(GetLanguageModelParamsCallback callback) override;
  void CanCreateWriter(blink::mojom::AIWriterCreateOptionsPtr options,
                       CanCreateWriterCallback callback) override;
  void CreateWriter(
      mojo::PendingRemote<blink::mojom::AIManagerCreateWriterClient> client,
      blink::mojom::AIWriterCreateOptionsPtr options) override;
  void CanCreateSummarizer(blink::mojom::AISummarizerCreateOptionsPtr options,
                           CanCreateSummarizerCallback callback) override;
  void CreateSummarizer(
      mojo::PendingRemote<blink::mojom::AIManagerCreateSummarizerClient> client,
      blink::mojom::AISummarizerCreateOptionsPtr options) override;
  void CanCreateRewriter(blink::mojom::AIRewriterCreateOptionsPtr options,
                         CanCreateRewriterCallback callback) override;
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
      AIUtils::LanguageCodes expected_input_languages,
      base::OnceCallback<void(AILanguageModelOrCreationError)> callback,
      const std::optional<const AILanguageModel::Context>& context =
          std::nullopt);

  void SendDownloadProgressUpdate(uint64_t downloaded_bytes,
                                  uint64_t total_bytes);

  mojo::ReceiverSet<blink::mojom::AIManager> receivers_;
  mojo::RemoteSet<blink::mojom::ModelDownloadProgressObserver>
      download_progress_observers_;
  std::unique_ptr<AIOnDeviceModelComponentObserver> component_observer_;

  AIContextBoundObjectSet context_bound_object_set_;
  raw_ptr<content::BrowserContext> browser_context_;

  base::WeakPtrFactory<AIManager> weak_factory_{this};
};

#endif  // CHROME_BROWSER_AI_AI_MANAGER_H_
