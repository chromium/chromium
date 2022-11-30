// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_OFFLINE_PAGES_RESOURCE_LOADING_OBSERVER_H_
#define CHROME_BROWSER_OFFLINE_PAGES_RESOURCE_LOADING_OBSERVER_H_

#include <stdint.h>

namespace offline_pages {

// This interface is used by clients who want to be notified when a resource is
// requested or completes loading, and to report the size of the resource.
class ResourceLoadingObserver {
 public:
  enum ResourceDataType {
    IMAGE,
    TEXT_CSS,
    XHR,
    OTHER,
    RESOURCE_DATA_TYPE_COUNT,
  };

  // Report when a resource starts or completes loading.
  virtual void ObserveResourceLoading(ResourceDataType type, bool started) = 0;

  // Report how many bytes were received for a resource.
  virtual void OnNetworkBytesChanged(int64_t received_bytes) = 0;
};

}  // namespace offline_pages

#endif  // CHROME_BROWSER_OFFLINE_PAGES_RESOURCE_LOADING_OBSERVER_H_
