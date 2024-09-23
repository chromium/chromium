// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_TRACING_ARC_TRACING_BRIDGE_H_
#define CHROME_BROWSER_ASH_ARC_TRACING_ARC_TRACING_BRIDGE_H_

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "ash/components/arc/mojom/tracing.mojom-forward.h"
#include "ash/components/arc/session/connection_observer.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/synchronization/lock.h"
#include "base/thread_annotations.h"
#include "components/keyed_service/core/keyed_service.h"
#include "services/tracing/public/cpp/base_agent.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace arc {

class ArcBridgeService;

// This class provides the interface to trigger tracing in the container.
class ArcTracingBridge : public KeyedService,
                         public ConnectionObserver<mojom::TracingInstance> {
 public:
  // Returns singleton instance for the given BrowserContext,
  // or nullptr if the browser |context| is not allowed to use ARC.
  static ArcTracingBridge* GetForBrowserContext(
      content::BrowserContext* context);
  static ArcTracingBridge* GetForBrowserContextForTesting(
      content::BrowserContext* context);

  ArcTracingBridge(content::BrowserContext* context,
                   ArcBridgeService* bridge_service);

  ArcTracingBridge(const ArcTracingBridge&) = delete;
  ArcTracingBridge& operator=(const ArcTracingBridge&) = delete;

  ~ArcTracingBridge() override;

  void GetCategories(std::set<std::string>* category_set);

  // ConnectionObserver<mojom::TracingInstance> overrides:
  void OnConnectionReady() override;

  // State of the tracing activity of the bridge.
  enum class State { kDisabled, kStarting, kEnabled, kStopping };
  State state() const { return state_; }

  using StartCallback = base::OnceCallback<void(bool)>;
  using StopCallback = base::OnceCallback<void()>;

  // Starts tracing and calls |callback| when started indicating whether tracing
  // was started successfully via its parameter.
  void StartTracing(const std::string& config, StartCallback callback);

  // Stops tracing and calls |callback| when stopped.
  void StopTracing(StopCallback callback);

  static void EnsureFactoryBuilt();

 private:
  // TODO(crbug.com/41386726): Remove once we have replaced the legacy tracing
  // service with perfetto.
  class ArcTracingAgent : public ::tracing::BaseAgent {
   public:
    explicit ArcTracingAgent(ArcTracingBridge* bridge);

    ArcTracingAgent(const ArcTracingAgent&) = delete;
    ArcTracingAgent& operator=(const ArcTracingAgent&) = delete;

    ~ArcTracingAgent() override;

   private:
    // tracing::BaseAgent.
    void GetCategories(std::set<std::string>* category_set) override;

    const raw_ptr<ArcTracingBridge> bridge_;
  };

  struct Category;

  // Callback for QueryAvailableCategories.
  void OnCategoriesReady(const std::vector<std::string>& categories);

  void OnArcTracingStarted(StartCallback callback, bool success);
  void OnArcTracingStopped(StopCallback callback, bool success);

  const raw_ptr<ArcBridgeService>
      arc_bridge_service_;  // Owned by ArcServiceManager.

  // List of available categories.
  base::Lock categories_lock_;
  std::vector<Category> categories_ GUARDED_BY(categories_lock_);

  ArcTracingAgent agent_;

  State state_ = State::kDisabled;

  // NOTE: Weak pointers must be invalidated before all other member variables
  // so it must be the last member.
  base::WeakPtrFactory<ArcTracingBridge> weak_ptr_factory_{this};
};

}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_TRACING_ARC_TRACING_BRIDGE_H_
