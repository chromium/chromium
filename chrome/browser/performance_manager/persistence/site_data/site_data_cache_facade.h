// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MANAGER_PERSISTENCE_SITE_DATA_SITE_DATA_CACHE_FACADE_H_
#define CHROME_BROWSER_PERFORMANCE_MANAGER_PERSISTENCE_SITE_DATA_SITE_DATA_CACHE_FACADE_H_

#include "base/callback_forward.h"
#include "base/macros.h"
#include "components/keyed_service/core/keyed_service.h"

namespace content {
class BrowserContext;
}

namespace performance_manager {

// This class serves as an interface between a SiteDataCache living on the PM
// sequence and the UI thread. This is meant to be used by a
// BrowserContextKeyedServiceFactory to manage the lifetime of this cache.
//
// Instances of this class are expected to live on the UI thread.
class SiteDataCacheFacade : public KeyedService {
 public:
  explicit SiteDataCacheFacade(content::BrowserContext* browser_context);
  ~SiteDataCacheFacade() override;

  void IsDataCacheRecordingForTesting(base::OnceCallback<void(bool)> cb);

  void WaitUntilCacheInitializedForTesting();

 private:
  // The browser context associated with this cache.
  content::BrowserContext* browser_context_;

  DISALLOW_COPY_AND_ASSIGN(SiteDataCacheFacade);
};

}  // namespace performance_manager

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_PERSISTENCE_SITE_DATA_SITE_DATA_CACHE_FACADE_H_
