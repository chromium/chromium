// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_OFFLINE_PAGES_PREFETCH_GCM_TOKEN_H_
#define CHROME_BROWSER_OFFLINE_PAGES_PREFETCH_GCM_TOKEN_H_

#include <string>

#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "components/gcm_driver/instance_id/instance_id.h"
#include "components/offline_pages/core/prefetch/prefetch_gcm_app_handler.h"

namespace content {
class BrowserContext;
}

namespace offline_pages {

// Returns a GCM token to be used for prefetching.
void GetGCMToken(content::BrowserContext* context,
                 const std::string& app_id,
                 instance_id::InstanceID::GetTokenCallback callback);

}  // namespace offline_pages

#endif  // CHROME_BROWSER_OFFLINE_PAGES_PREFETCH_GCM_TOKEN_H_
