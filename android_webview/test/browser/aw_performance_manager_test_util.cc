// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "components/performance_manager/public/graph/frame_node.h"
#include "components/performance_manager/public/graph/graph.h"
#include "components/performance_manager/public/graph/page_node.h"
#include "components/performance_manager/public/graph/process_node.h"
#include "components/performance_manager/public/graph/worker_node.h"
#include "components/performance_manager/public/performance_manager.h"
#include "content/public/browser/web_contents.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "android_webview/test/webview_instrumentation_test_native_jni/AwPerformanceManagerTestUtil_jni.h"

namespace android_webview {

static bool JNI_AwPerformanceManagerTestUtil_VerifyGraphNodesExist(
    JNIEnv* env,
    content::WebContents* web_contents,
    const std::string& frame_url,
    const std::string& worker_url) {
  if (!performance_manager::PerformanceManager::IsAvailable() ||
      !web_contents) {
    return false;
  }

  base::WeakPtr<performance_manager::PageNode> page_node =
      performance_manager::PerformanceManager::GetPrimaryPageNodeForWebContents(
          web_contents);
  if (!page_node) {
    return false;
  }

  const performance_manager::FrameNode* frame_node =
      page_node->GetPrimaryMainFrameNode();
  if (!frame_node) {
    return false;
  }

  const performance_manager::ProcessNode* process_node =
      frame_node->GetProcessNode();
  if (!process_node) {
    return false;
  }

  if (frame_node->GetURL().spec() != frame_url) {
    return false;
  }

  performance_manager::Graph* graph =
      performance_manager::PerformanceManager::GetGraph();

  bool found_worker = false;
  for (const performance_manager::WorkerNode* worker_node :
       graph->GetAllWorkerNodes()) {
    if (worker_node->GetURL().spec() == worker_url) {
      found_worker = true;
      break;
    }
  }
  if (!found_worker) {
    return false;
  }

  return true;
}

}  // namespace android_webview

DEFINE_JNI(AwPerformanceManagerTestUtil)
