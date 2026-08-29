// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_URL_LOADER_THROTTLE_H_
#define CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_URL_LOADER_THROTTLE_H_

#include <memory>

#include "base/functional/callback.h"
#include "third_party/blink/public/common/loader/url_loader_throttle.h"

class GURL;
class Profile;

namespace content {
class WebContents;
}  // namespace content

namespace contextual_tasks {

// Throttle that injects the Chrome-Search-Capabilities-Version custom HTTP
// header on requests to Google-associated domains when the Contextual Tasks
// rearchitecture is enabled.
class ContextualTasksURLLoaderThrottle : public blink::URLLoaderThrottle {
 public:
  static std::unique_ptr<ContextualTasksURLLoaderThrottle> MaybeCreate(
      Profile* profile,
      const base::RepeatingCallback<content::WebContents*()>& wc_getter = {});

  ContextualTasksURLLoaderThrottle();
  ~ContextualTasksURLLoaderThrottle() override;

  ContextualTasksURLLoaderThrottle(const ContextualTasksURLLoaderThrottle&) =
      delete;
  ContextualTasksURLLoaderThrottle& operator=(
      const ContextualTasksURLLoaderThrottle&) = delete;

  // blink::URLLoaderThrottle:
  void WillStartRequest(network::ResourceRequest* request,
                        bool* defer) override;
  void WillRedirectRequest(
      net::RedirectInfo* redirect_info,
      const network::mojom::URLResponseHead& response_head,
      bool* defer,
      network::HttpRequestHeadersUpdateParams* headers_update_params) override;

 private:
  static bool ShouldAppendHeader(const GURL& url);
};

}  // namespace contextual_tasks

#endif  // CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_URL_LOADER_THROTTLE_H_
