// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_URL_LOADER_FACTORY_INTERCEPTOR_H_
#define CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_URL_LOADER_FACTORY_INTERCEPTOR_H_

#include "content/public/browser/render_frame_host.h"
#include "services/network/public/cpp/url_loader_factory_builder.h"

namespace contextual_tasks {

// Intercepts URLLoaderFactory creation for Contextual Tasks guest frames
// to inject the Authorization header. This is done using a URLLoaderFactory
// instead of the webview onBeforeRequest callback because the webview does not
// support stalling the request until a token is fetched. This approach allows
// for the token to wait until a fresh token can be fetched. This prevents
// issues where the token becomes invalid/stale.
void MaybeInterceptURLLoaderFactory(
    content::RenderFrameHost* frame,
    network::URLLoaderFactoryBuilder& factory_builder);

}  // namespace contextual_tasks

#endif  // CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_URL_LOADER_FACTORY_INTERCEPTOR_H_
