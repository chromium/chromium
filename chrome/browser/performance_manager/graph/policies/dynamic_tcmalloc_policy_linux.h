// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MANAGER_GRAPH_POLICIES_DYNAMIC_TCMALLOC_POLICY_LINUX_H_
#define CHROME_BROWSER_PERFORMANCE_MANAGER_GRAPH_POLICIES_DYNAMIC_TCMALLOC_POLICY_LINUX_H_

#include "base/macros.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/common/performance_manager/mojom/tcmalloc.mojom.h"
#include "components/performance_manager/public/graph/graph.h"
#include "components/performance_manager/public/graph/node_attached_data.h"
#include "components/performance_manager/public/graph/process_node.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace performance_manager {
namespace policies {

// DynamicTcmallocPolicy is a policy which will periodically update renderers
// Tcmalloc tunables in an effort to improve performance and memory efficiency.
class DynamicTcmallocPolicy : public GraphOwned {
 public:
  DynamicTcmallocPolicy();
  ~DynamicTcmallocPolicy() override;

  // GraphOwned implementation:
  void OnPassedToGraph(Graph* graph) override;
  void OnTakenFromGraph(Graph* graph) override;

 protected:
  // Virtual for testing.
  virtual void CheckAndUpdateTunables();
  virtual float CalculateFreeMemoryRatio();
  virtual mojo::Remote<tcmalloc::mojom::TcmallocTunables>*
  EnsureTcmallocTunablesForProcess(const ProcessNode* process_node);

  base::RepeatingTimer timer_;

 private:
  Graph* graph_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(DynamicTcmallocPolicy);
};

}  // namespace policies
}  // namespace performance_manager

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_GRAPH_POLICIES_DYNAMIC_TCMALLOC_POLICY_LINUX_H_
