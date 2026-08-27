// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SERVICE_WORKER_SERVICE_WORKER_SYNTHETIC_RESPONSE_H_
#define CHROME_BROWSER_SERVICE_WORKER_SERVICE_WORKER_SYNTHETIC_RESPONSE_H_

class GURL;

namespace content {
class BrowserContext;
}  // namespace content

namespace chrome_service_worker {

bool IsServiceWorkerSyntheticResponseAllowed(
    content::BrowserContext* browser_context,
    const GURL& url);

}  // namespace chrome_service_worker

#endif  // CHROME_BROWSER_SERVICE_WORKER_SERVICE_WORKER_SYNTHETIC_RESPONSE_H_
