// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/renderer/browser_exposed_renderer_interfaces.h"

#include "android_webview/renderer/aw_content_renderer_client.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/visitedlink/renderer/visitedlink_slave.h"
#include "mojo/public/cpp/bindings/binder_map.h"

namespace android_webview {

void ExposeRendererInterfacesToBrowser(AwContentRendererClient* client,
                                       mojo::BinderMap* binders) {
  binders->Add(client->visited_link_slave()->GetBindCallback(),
               base::ThreadTaskRunnerHandle::Get());
}

}  // namespace android_webview
