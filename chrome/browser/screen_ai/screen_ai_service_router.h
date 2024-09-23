// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SCREEN_AI_SCREEN_AI_SERVICE_ROUTER_H_
#define CHROME_BROWSER_SCREEN_AI_SCREEN_AI_SERVICE_ROUTER_H_

#include <optional>
#include <set>

#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/screen_ai/screen_ai_install_state.h"
#include "components/keyed_service/core/keyed_service.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/screen_ai/public/mojom/screen_ai_factory.mojom.h"
#include "services/screen_ai/public/mojom/screen_ai_service.mojom.h"

namespace {
class ComponentFiles;
}

namespace screen_ai {

using ServiceStateCallback = base::OnceCallback<void(bool)>;

class ScreenAIServiceRouter : public KeyedService,
                              ScreenAIInstallState::Observer {
 public:
  enum class Service {
    kMainContentExtraction,
    kOCR,
  };

  ScreenAIServiceRouter(const ScreenAIServiceRouter&) = delete;
  ScreenAIServiceRouter& operator=(const ScreenAIServiceRouter&) = delete;
  ~ScreenAIServiceRouter() override;

  void BindScreenAIAnnotator(
      mojo::PendingReceiver<mojom::ScreenAIAnnotator> receiver);

  void BindMainContentExtractor(
      mojo::PendingReceiver<mojom::Screen2xMainContentExtractor> receiver);

  // Schedules library download and initializaes the service if needed, and
  // calls `callback` with initialization result when service is ready or
  // failed to initialize.
  void GetServiceStateAsync(Service service, ServiceStateCallback callback);

  // ScreenAIInstallState::Observer:
  void StateChanged(ScreenAIInstallState::State state) override;

  // Returns true if the connection for `service` is bound.
  bool IsConnectionBoundForTesting(Service service);

  // Returns true if sandboxed process is running.
  bool IsProcessRunningForTesting();

  void ShutDownIfNoClientsForTesting() {
    if (screen_ai_service_factory_.is_bound()) {
      screen_ai_service_factory_->ShutDownIfNoClients();
    }
  }

 private:
  friend class ScreenAIServiceRouterFactory;

  ScreenAIServiceRouter();
  // Initialzies the `service` if it's not already done.
  void InitializeServiceIfNeeded(Service service);

  void InitializeOCR(base::TimeTicks request_start_time,
                     mojo::PendingReceiver<mojom::OCRService> receiver,
                     std::unique_ptr<ComponentFiles> model_files);

  void InitializeMainContentExtraction(
      base::TimeTicks request_start_time,
      mojo::PendingReceiver<mojom::MainContentExtractionService> receiver,
      std::unique_ptr<ComponentFiles> model_files);

  // Launches the service if it's not already launched.
  void LaunchIfNotRunning();

  // True if service is already initialized, false if it is disabled, and
  // nullopt if not known.
  std::optional<bool> GetServiceState(Service service);

  // Callback from Screen AI service with library load result.
  void SetLibraryLoadState(Service service,
                           base::TimeTicks request_start_time,
                           bool successful);

  // Calls back all pendnding service state requests.
  void CallPendingStatusRequests(Service service, bool successful);

  // Called when ScreenAI service factory is disconnected.
  void OnScreenAIServiceDisconnected();

  // Returns the list of services that have a pending status request.
  std::set<Service> GetAllPendingStatusServices();

  // Pending requests to receive service state for each service type.
  std::map<Service, std::vector<ServiceStateCallback>> pending_state_requests_;

  // Observes changes in Screen AI component download state.
  base::ScopedObservation<ScreenAIInstallState, ScreenAIInstallState::Observer>
      component_ready_observer_{this};

  mojo::Remote<mojom::ScreenAIServiceFactory> screen_ai_service_factory_;
  mojo::Remote<mojom::OCRService> ocr_service_;
  mojo::Remote<mojom::MainContentExtractionService>
      main_content_extraction_service_;

  base::WeakPtrFactory<ScreenAIServiceRouter> weak_ptr_factory_{this};
};

}  // namespace screen_ai

#endif  // CHROME_BROWSER_SCREEN_AI_SCREEN_AI_SERVICE_ROUTER_H_
