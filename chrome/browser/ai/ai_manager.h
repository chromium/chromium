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
#include "chrome/browser/ai/ai_model_download_progress_manager.h"
#include "chrome/browser/ai/ai_summarizer.h"
#include "chrome/browser/ai/ai_utils.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_observer.h"
#include "content/public/browser/weak_document_ptr.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "third_party/blink/public/mojom/ai/ai_common.mojom-forward.h"
#include "third_party/blink/public/mojom/ai/ai_common.mojom.h"
#include "third_party/blink/public/mojom/ai/ai_language_model.mojom-forward.h"
#include "third_party/blink/public/mojom/ai/ai_manager.mojom.h"
#include "third_party/blink/public/mojom/ai/model_download_progress_observer.mojom-forward.h"
#include "third_party/blink/public/mojom/devtools/console_message.mojom-data-view.h"

namespace base {
class SupportsUserData;
}  // namespace base

namespace content {
class RenderFrameHost;
}  // namespace content

using blink::mojom::AILanguageCodePtr;

// Owned by the host of the document / service worker via `SupportUserData`.
// The browser-side implementation of `blink::mojom::AIManager`.
class AIManager : public base::SupportsUserData::Data,
                  public blink::mojom::AIManager,
                  public content::RenderWidgetHostObserver {
 public:
  using AILanguageModelOrCreationError =
      base::expected<std::unique_ptr<AILanguageModel>,
                     blink::mojom::AIManagerCreateClientError>;
  AIManager(content::BrowserContext* browser_context,
            component_updater::ComponentUpdateService* component_update_service,
            content::RenderFrameHost* rfh);
  AIManager(const AIManager&) = delete;
  AIManager& operator=(const AIManager&) = delete;

  ~AIManager() override;

  void AddReceiver(mojo::PendingReceiver<blink::mojom::AIManager> receiver);

  size_t GetContextBoundObjectSetSizeForTesting() {
    return context_bound_object_set_.GetSizeForTesting();
  }

  size_t GetDownloadProgressObserversSizeForTesting() {
    return model_download_progress_manager_.GetNumberOfReporters();
  }

  // Return the max top k value for the LanguageModel API. Note that this value
  // won't exceed the max top k defined by the underlying on-device model.
  uint32_t GetLanguageModelMaxTopK();
  // Return the max temperature for the LanguageModel API.
  float GetLanguageModelMaxTemperature();

  // Returns if all of the language codes in `languages` are supported.
  static bool IsLanguagesSupported(
      const std::vector<AILanguageCodePtr>& languages);

  // Returns if `output` and all of the language codes in `input` and `context`
  // are supported.
  static bool IsLanguagesSupported(
      const std::vector<AILanguageCodePtr>& input,
      const std::vector<AILanguageCodePtr>& context,
      const AILanguageCodePtr& output);

  // Return the default and max sampling params for the LanguageModel API.
  blink::mojom::AILanguageModelParamsPtr GetLanguageModelParams();

  // `blink::mojom::AIManager` implementation.
  void CanCreateLanguageModel(
      blink::mojom::AILanguageModelCreateOptionsPtr options,
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

  // Check whether optimization guide supports the feature matching `capability`
  // and modalities specified by `capabilities`; yields a result to `callback`.
  void CanCreateSession(optimization_guide::ModelBasedCapabilityKey capability,
                        on_device_model::Capabilities capabilities,
                        CanCreateLanguageModelCallback callback);

  bool IsBuiltInAIAPIsEnabledByPolicy();

 private:
  void OnModelPathValidationComplete(const std::string& model_path,
                                     bool is_valid_path);

  // Creates an `AILanguageModel`, as a new session. Clones are created
  // internally within the `AILanguageModel` object.
  void CreateLanguageModelInternal(
      mojo::PendingRemote<blink::mojom::AIManagerCreateLanguageModelClient>
          client,
      blink::mojom::AILanguageModelCreateOptionsPtr options,
      base::WeakPtr<optimization_guide::ModelClient> model_client);

  // content::RenderWidgetHostObserver:
  void RenderWidgetHostVisibilityChanged(content::RenderWidgetHost* widget_host,
                                         bool became_visible) override;
  void RenderWidgetHostDestroyed(
      content::RenderWidgetHost* widget_host) override;

  void FinishCanCreateSession(
      optimization_guide::ModelBasedCapabilityKey capability,
      on_device_model::Capabilities capabilities,
      CanCreateLanguageModelCallback callback,
      optimization_guide::OnDeviceModelEligibilityReason eligibility);

  void AddMessageToConsoleForUnexpectedLanguage(
      blink::mojom::ConsoleMessageLevel level,
      std::string message);

  mojo::ReceiverSet<blink::mojom::AIManager> receivers_;

  on_device_ai::AIModelDownloadProgressManager model_download_progress_manager_;

  raw_ref<component_updater::ComponentUpdateService> component_update_service_;
  AIContextBoundObjectSet context_bound_object_set_;
  raw_ptr<content::BrowserContext> browser_context_;

  base::ScopedObservation<content::RenderWidgetHost,
                          content::RenderWidgetHostObserver>
      widget_observer_{this};

  std::unique_ptr<optimization_guide::ModelBrokerClient> model_broker_client_;

  content::WeakDocumentPtr rfh_;

  bool did_add_warning_console_message_for_unexpected_language_ = false;
  bool did_add_error_console_message_for_unexpected_language_ = false;

  base::WeakPtrFactory<AIManager> weak_factory_{this};
};

#endif  // CHROME_BROWSER_AI_AI_MANAGER_H_
