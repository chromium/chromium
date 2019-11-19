// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ARC_TRACING_ARC_TRACING_BRIDGE_H_
#define CHROME_BROWSER_CHROMEOS_ARC_TRACING_ARC_TRACING_BRIDGE_H_

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/containers/ring_buffer.h"
#include "base/files/file_descriptor_watcher_posix.h"
#include "base/files/scoped_file.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "components/arc/mojom/tracing.mojom.h"
#include "components/arc/session/connection_observer.h"
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

  ArcTracingBridge(content::BrowserContext* context,
                   ArcBridgeService* bridge_service);
  ~ArcTracingBridge() override;

  void GetCategories(std::set<std::string>* category_set);

  // ConnectionObserver<mojom::TracingInstance> overrides:
  void OnConnectionReady() override;

  // State of the tracing activity of the bridge.
  enum class State { kDisabled, kStarting, kEnabled, kStopping };
  State state() const { return state_; }

  using SuccessCallback = base::OnceCallback<void(bool)>;
  using TraceDataCallback = base::OnceCallback<void(const std::string& data)>;

  // Starts tracing and calls |callback| when started indicating whether tracing
  // was started successfully via its parameter.
  void StartTracing(const std::string& config, SuccessCallback callback);

  // Stops tracing and calls |callback| with the recorded trace data once
  // stopped. If unsuccessful, calls |callback| with an empty data string.
  void StopAndFlush(TraceDataCallback callback);

 private:
  // TODO(crbug.com/839086): Remove once we have replaced the legacy tracing
  // service with perfetto.
  class ArcTracingAgent : public tracing::BaseAgent {
   public:
    explicit ArcTracingAgent(ArcTracingBridge* bridge);
    ~ArcTracingAgent() override;

   private:
    // tracing::BaseAgent.
    void GetCategories(std::set<std::string>* category_set) override;

    ArcTracingBridge* const bridge_;

    DISALLOW_COPY_AND_ASSIGN(ArcTracingAgent);
  };

  // A helper class for reading trace data from the client side. We separate
  // this from |ArcTracingAgentImpl| to isolate the logic that runs on browser's
  // IO thread. All the functions in this class except for constructor are
  // expected to be run on browser's IO thread.
  class ArcTracingReader {
   public:
    ArcTracingReader();
    ~ArcTracingReader();

    // Starts reading trace data from the given file descriptor.
    void StartTracing(base::ScopedFD read_fd);
    // Stops reading and returns the collected trace data.
    std::string StopTracing();

   private:
    void OnTraceDataAvailable();

    // Number of events for the ring buffer.
    static constexpr size_t kTraceEventBufferSize = 64000;

    base::ScopedFD read_fd_;
    std::unique_ptr<base::FileDescriptorWatcher::Controller> fd_watcher_;
    base::RingBuffer<std::string, kTraceEventBufferSize> ring_buffer_;

    DISALLOW_COPY_AND_ASSIGN(ArcTracingReader);
  };

  struct Category;

  // Callback for QueryAvailableCategories.
  void OnCategoriesReady(const std::vector<std::string>& categories);

  void OnArcTracingStarted(SuccessCallback callback, bool success);
  void OnArcTracingStopped(TraceDataCallback tracing_stopped_callback,
                           bool success);
  void OnTracingReaderStopped(TraceDataCallback tracing_stopped_callback,
                              const std::string& data);

  ArcBridgeService* const arc_bridge_service_;  // Owned by ArcServiceManager.

  // List of available categories.
  std::vector<Category> categories_;

  ArcTracingAgent agent_;

  std::unique_ptr<ArcTracingReader> reader_;

  State state_ = State::kDisabled;

  // NOTE: Weak pointers must be invalidated before all other member variables
  // so it must be the last member.
  base::WeakPtrFactory<ArcTracingBridge> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ArcTracingBridge);
};

}  // namespace arc

#endif  // CHROME_BROWSER_CHROMEOS_ARC_TRACING_ARC_TRACING_BRIDGE_H_
