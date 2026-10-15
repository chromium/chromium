// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_EXTENSION_SERVICE_WORKER_HANDLER_H_
#define CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_EXTENSION_SERVICE_WORKER_HANDLER_H_

#include <optional>
#include <string>
#include <vector>

#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/uuid.h"
#include "chrome/browser/contextual_tasks/contextual_tasks.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

class Profile;

namespace content {
class RenderFrameHost;
}  // namespace content

namespace contextual_tasks {

// Handles Mojo connection requests from the Contextual Tasks component
// extension Service Worker, routing document-scoped search messages to and from
// `ContextualTasksWebContentsUserData` keyed by `ExtensionApiFrameIdMap`
// document ID. Owned by `ContextualTasksExtensionBridge` and scoped to the
// Profile lifecycle.
class ContextualTasksExtensionServiceWorkerHandler
    : public mojom::ExtensionPageHandlerFactory,
      public mojom::ExtensionPageHandler {
 public:
  explicit ContextualTasksExtensionServiceWorkerHandler(Profile& profile);
  ContextualTasksExtensionServiceWorkerHandler(
      const ContextualTasksExtensionServiceWorkerHandler&) = delete;
  ContextualTasksExtensionServiceWorkerHandler& operator=(
      const ContextualTasksExtensionServiceWorkerHandler&) = delete;
  ~ContextualTasksExtensionServiceWorkerHandler() override;

  void BindFactory(
      mojo::PendingReceiver<mojom::ExtensionPageHandlerFactory> receiver);

  // Resets active receiver and remote pipes during profile shutdown.
  void Clear();

  // mojom::ExtensionPageHandlerFactory:
  void CreateExtensionPageHandler(
      mojo::PendingRemote<mojom::ExtensionPage> page,
      mojo::PendingReceiver<mojom::ExtensionPageHandler> receiver) override;

  // mojom::ExtensionPageHandler:
  void GetHandshakeMessage(const std::optional<std::string>& document_id,
                           GetHandshakeMessageCallback callback) override;
  void OnWebviewMessage(const std::vector<uint8_t>& message,
                        const std::optional<std::string>& document_id) override;
  void SetTaskId(const base::Uuid& uuid) override;
  void GetLensCropPreview(GetLensCropPreviewCallback callback) override;
  void RemoveLensCrop() override;

 private:
  content::RenderFrameHost* GetValidatedMainFrameForDocumentId(
      const std::string& document_id) const;
  void DispatchSearchMessage(const std::string& document_id,
                             const std::vector<uint8_t>& message);

  const raw_ref<Profile> profile_;
  mojo::Receiver<mojom::ExtensionPageHandlerFactory> factory_receiver_{this};
  mojo::Receiver<mojom::ExtensionPageHandler> handler_receiver_{this};
  mojo::Remote<mojom::ExtensionPage> page_;
  base::WeakPtrFactory<ContextualTasksExtensionServiceWorkerHandler>
      weak_ptr_factory_{this};
};

}  // namespace contextual_tasks

#endif  // CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_EXTENSION_SERVICE_WORKER_HANDLER_H_
