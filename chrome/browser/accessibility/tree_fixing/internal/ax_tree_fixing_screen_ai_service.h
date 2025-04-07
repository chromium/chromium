// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACCESSIBILITY_TREE_FIXING_INTERNAL_AX_TREE_FIXING_SCREEN_AI_SERVICE_H_
#define CHROME_BROWSER_ACCESSIBILITY_TREE_FIXING_INTERNAL_AX_TREE_FIXING_SCREEN_AI_SERVICE_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/elapsed_timer.h"
#include "chrome/browser/profiles/profile.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/screen_ai/public/mojom/screen_ai_service.mojom.h"

namespace tree_fixing {

// This class provides a connection to the ScreenAI service for requests such as
// main node identification or main content extraction. Upon construction, this
// class will ensure that the required models are downloaded/loaded. This
// service should only be constructed by clients when the client requires the
// ScreenAI service (and not earlier). The client that constructs this service
// owns the object and needs to handle tear-down.
class AXTreeFixingScreenAIService final {
 public:
  // The possible initialization states of the ScreenAI service.
  enum class InitializationState {
    kUninitialized,
    kInitializing,
    kInitialized,
    kDisconnected,
    kInitializationFailed
  };

  // Delegate for clients that want to perform main node identification.
  class MainNodeIdentificationDelegate {
   protected:
    MainNodeIdentificationDelegate() = default;

   public:
    MainNodeIdentificationDelegate(const MainNodeIdentificationDelegate&) =
        delete;
    MainNodeIdentificationDelegate& operator=(
        const MainNodeIdentificationDelegate&) = delete;
    virtual ~MainNodeIdentificationDelegate() = default;

    // This method is used to communicate to the delegate (owner) of this
    // instance, the main node that was identified via the IdentifyMainNode
    // method. When calling IdentifyMainNode, the client must provide a
    // request_id, and this ID is passed back to the client along with the
    // tree_id and node_id. The request_id allows clients to make multiple
    // requests in parallel and uniquely identify each response. It is the
    // responsibility of the client to handle the logic behind a request_id,
    // this service simply passes the id through.
    virtual void OnMainNodeIdentified(const ui::AXTreeID& tree_id,
                                      ui::AXNodeID node_id,
                                      int request_id) = 0;

    // This method is used to communicate to the delegate (owner) of this
    // instance, the current status of the ScreenAI service. This initialization
    // is done asynchronously, and requires both downloading and loading the
    // ScreenAI model. |This| service will inform the client once it is
    // initialized, beforehand requests will fail.
    virtual void OnServiceStateChanged(bool service_ready) = 0;
  };

  AXTreeFixingScreenAIService(MainNodeIdentificationDelegate& delegate,
                              Profile* profile);
  AXTreeFixingScreenAIService(const AXTreeFixingScreenAIService&) = delete;
  AXTreeFixingScreenAIService& operator=(const AXTreeFixingScreenAIService&) =
      delete;
  ~AXTreeFixingScreenAIService();

  // --- Public APIs for upstream clients (e.g. AXTreeFixingServicesRouter) ---

  // Identifies the main node of an AXTreeUpdate. The client should provide a
  // request_id, which is returned to the client along with a tree_id and
  // node_id via a call to OnMainNodeIdentified.
  void IdentifyMainNode(const ui::AXTreeUpdate& ax_tree_update, int request_id);

 private:
  // Internal methods related to managing ScreenAI service connection.
  void Initialize();
  void ServiceInitializationCallback(bool success);
  void HandleServiceDisconnect();
  uint32_t initialization_attempt_count_ = 0;
  InitializationState initialization_state_ =
      InitializationState::kUninitialized;
  bool previously_attempted_reconnect_ = false;

  // Internal method that processes results from the ScreenAI service before
  // returning the results to the owner of this instance via the provided
  // delegate.
  void ProcessScreenAIMainNodeIdentificationResult(
      int request_id,
      base::ElapsedTimer timer,
      const ui::AXTreeUpdate& ax_tree_update,
      const ui::AXTreeID& tree_id,
      int node_id);

  // Delegate provided by client to receive main node identification results.
  // Use a raw_ref since we do not own the delegate or control its lifecycle.
  const raw_ref<MainNodeIdentificationDelegate>
      main_node_identification_delegate_;

  // Profile for the KeyedService that owns us.
  const raw_ptr<Profile> profile_;

  // The remote of the ScreenAI service, the receiver is in a utility process.
  mojo::Remote<screen_ai::mojom::Screen2xMainContentExtractor>
      screen_ai_service_;

  base::WeakPtrFactory<AXTreeFixingScreenAIService> weak_ptr_factory_{this};
};

}  // namespace tree_fixing

#endif  // CHROME_BROWSER_ACCESSIBILITY_TREE_FIXING_INTERNAL_AX_TREE_FIXING_SCREEN_AI_SERVICE_H_
