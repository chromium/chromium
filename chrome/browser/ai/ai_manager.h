// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AI_AI_MANAGER_H_
#define CHROME_BROWSER_AI_AI_MANAGER_H_

#include <optional>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/supports_user_data.h"
#include "base/types/pass_key.h"
#include "chrome/browser/ai/ai_context_bound_object_set.h"
#include "chrome/browser/ai/ai_language_model.h"
#include "chrome/browser/ai/ai_model_download_progress_manager.h"
#include "chrome/browser/ai/ai_proofreader.h"
#include "chrome/browser/ai/ai_summarizer.h"
#include "chrome/browser/ai/ai_utils.h"
#include "components/component_updater/component_updater_service.h"
#include "components/optimization_guide/public/mojom/model_broker.mojom-data-view.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_observer.h"
#include "content/public/browser/weak_document_ptr.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote_set.h"
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
    return context_bound_object_set_.GetSize();
  }

  size_t GetDownloadProgressObserversSizeForTesting() {
    return model_download_progress_manager_.GetNumberOfReporters();
  }

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
  void CanCreateProofreader(blink::mojom::AIProofreaderCreateOptionsPtr options,
                            CanCreateProofreaderCallback callback) override;
  void CreateProofreader(
      mojo::PendingRemote<blink::mojom::AIManagerCreateProofreaderClient>
          client,
      blink::mojom::AIProofreaderCreateOptionsPtr options) override;
  void AddModelDownloadProgressObserver(
      mojo::PendingRemote<blink::mojom::ModelDownloadProgressObserver>
          observer_remote) override;

  // Check whether optimization guide supports the feature matching `capability`
  // and modalities specified by `capabilities`; yields a result to `callback`.
  void CanCreateSession(optimization_guide::mojom::OnDeviceFeature capability,
                        on_device_model::Capabilities capabilities,
                        CanCreateLanguageModelCallback callback);

  bool IsBuiltInAIAPIsEnabledByPolicy();

  // Returns true if `options` uses only `supported` languages, false otherwise.
  // Logs errors and warnings and initializes empty output languages as needed.
  template <typename OptionsPtrType>
  bool CheckAndFixLanguages(OptionsPtrType& options,
                            std::string_view api_name,
                            const base::flat_set<std::string_view>& supported);

 private:
  void OnModelPathValidationComplete(const base::FilePath& model_path,
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
      optimization_guide::mojom::OnDeviceFeature capability,
      on_device_model::Capabilities capabilities,
      CanCreateLanguageModelCallback callback,
      optimization_guide::OnDeviceModelEligibilityReason eligibility);

  template <typename ContextBoundObjectType,
            typename ContextBoundObjectReceiverInterface,
            typename ClientRemoteInterface,
            typename CreateOptionsPtrType>
  void OnSessionCreated(
      AIContextBoundObjectSet& context_bound_object_set,
      CreateOptionsPtrType options,
      std::optional<optimization_guide::MultimodalMessage> initial_request,
      mojo::PendingRemote<ClientRemoteInterface> client,
      std::unique_ptr<optimization_guide::OnDeviceSession> session);

  // Eagerly initializes a broad set of features.
  void MaybeTryEagerInit();

  void MaybeLogMissingOutputLanguageWarning(
      const std::string_view api_name,
      const base::flat_set<std::string_view>& supported_languages);
  void MaybeLogUnsupportedLanguageError(
      const std::string_view api_name,
      const base::flat_set<std::string_view>& supported_languages);

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

  bool did_log_missing_output_language_warning_ = false;
  bool did_log_unsupported_language_error_ = false;

  // Features that have attempted initialization in this session.
  base::flat_set<optimization_guide::mojom::OnDeviceFeature> tried_init_;

  base::WeakPtrFactory<AIManager> weak_factory_{this};
};

#endif  // CHROME_BROWSER_AI_AI_MANAGER_H_
