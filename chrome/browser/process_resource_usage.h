// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROCESS_RESOURCE_USAGE_H_
#define CHROME_BROWSER_PROCESS_RESOURCE_USAGE_H_

#include <stddef.h>

#include "base/containers/circular_deque.h"
#include "base/functional/callback.h"
#include "base/threading/thread_checker.h"
#include "content/public/common/resource_usage_reporter.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/common/web_cache/web_cache_resource_type_stats.h"

// Provides resource usage information about a child process.
//
// This is a wrapper around the content::mojom::ResourceUsageReporter Mojo
// service that exposes
// information about resources used by a child process. Currently, this is only
// V8 memory and Blink resource cache usage, but could be expanded to include
// other resources.  This is intended for status viewers such as the task
// manager.
//
// To create:
// 1. Create a mojo::PendingRemote<content::mojom::ResourceUsageReporter> and
//    obtain a mojo::PendingReceiver<> using InitWithNewPipeAndPassReceiver().
// 2. Use the child process's service registry to connect to the service using
//    the mojo::PendingReceiver<>. Note, ServiceRegistry is thread hostile and
//    must always be accessed from the same thread. However, PendingReceiver<>
//    can be passed safely between threads, and therefore a task can be posted
//    to the ServiceRegistry thread to connect to the remote service.
// 3. Pass the mojo::PendingRemote<content::mojom::ResourceUsageReporter> to the
//    constructor.
//
// Example:
//   void Foo::ConnectToService(
//       mojo::PendingReceiver<content::mojom::ResourceUsageReporter>
//           receiver) {
//     content::ServiceRegistry* registry = host_->GetServiceRegistry();
//     registry->ConnectToRemoteService(std::move(req));
//   }
//
//   ...
//     mojo::PendingRemote<content::mojom::ResourceUsageReporter> service;
//     mojo::PendingReceiver<content::mojom::ResourceUsageReporter> receiver =
//         service.InitWithNewPipeAndPassReceiver();
//     content::GetIOThreadTaskRunner({})->PostTask(
//         FROM_HERE,
//         base::BindOnce(&Foo::ConnectToService, this, std::move(receiver)));
//     resource_usage_.reset(new ProcessResourceUsage(std::move(service)));
//   ...
//
// Note: ProcessResourceUsage is thread-hostile and must live on a single
// thread.
class ProcessResourceUsage {
 public:
  // Must be called from the same thread that created |service|.
  explicit ProcessResourceUsage(
      mojo::PendingRemote<content::mojom::ResourceUsageReporter> service);

  ProcessResourceUsage(const ProcessResourceUsage&) = delete;
  ProcessResourceUsage& operator=(const ProcessResourceUsage&) = delete;

  ~ProcessResourceUsage();

  // Refresh the resource usage information. |callback| is invoked when the
  // usage data is updated, or when the IPC connection is lost.
  void Refresh(base::OnceClosure callback);

  // Get V8 memory usage information.
  bool ReportsV8MemoryStats() const;
  size_t GetV8MemoryAllocated() const;
  size_t GetV8MemoryUsed() const;

  // Get Blink resource cache information.
  blink::WebCacheResourceTypeStats GetBlinkMemoryCacheStats() const;

 private:
  // Mojo IPC callback.
  void OnRefreshDone(content::mojom::ResourceUsageDataPtr data);

  void RunPendingRefreshCallbacks();

  mojo::Remote<content::mojom::ResourceUsageReporter> service_;
  bool update_in_progress_;
  base::circular_deque<base::OnceClosure> refresh_callbacks_;

  content::mojom::ResourceUsageDataPtr stats_;

  base::ThreadChecker thread_checker_;
};

#endif  // CHROME_BROWSER_PROCESS_RESOURCE_USAGE_H_
