// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ROUTER_MOJO_MEDIA_SINK_SERVICE_STATUS_H_
#define CHROME_BROWSER_MEDIA_ROUTER_MOJO_MEDIA_SINK_SERVICE_STATUS_H_

#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/containers/mru_cache.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "components/media_router/common/discovery/media_sink_internal.h"
#include "components/media_router/common/media_route_provider_helper.h"

namespace media_router {

// Keeps track of media sinks reported by media sink service. This class
// provides some debug info about in-browser discovery.
class MediaSinkServiceStatus {
 public:
  MediaSinkServiceStatus();
  ~MediaSinkServiceStatus();

  // Called when a media sink service reports discovered sinks to MR.
  void UpdateDiscoveredSinks(
      const std::string& provider_name,
      const std::vector<MediaSinkInternal>& discovered_sinks);

  // Called when a media sink service reports available sinks for an app to MR.
  void UpdateAvailableSinks(
      MediaRouteProviderId provider_id,
      const std::string& media_source,
      const std::vector<MediaSinkInternal>& available_sinks);

  // Returns current status as a JSON string represented by base::Value.
  base::Value GetStatusAsValue() const;

  // Returns current status as a JSON string.
  std::string GetStatusAsJSONString() const;

  base::WeakPtr<MediaSinkServiceStatus> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  // Map of discovered sinks, keyed by provider name.
  base::flat_map<std::string, std::vector<MediaSinkInternal>> discovered_sinks_;
  // Map of available sinks, keyed by media source.
  base::MRUCache<std::string, std::vector<MediaSinkInternal>> available_sinks_;

  base::WeakPtrFactory<MediaSinkServiceStatus> weak_ptr_factory_{this};
  DISALLOW_COPY_AND_ASSIGN(MediaSinkServiceStatus);
};

}  // namespace media_router

#endif  // CHROME_BROWSER_MEDIA_ROUTER_MOJO_MEDIA_SINK_SERVICE_STATUS_H_
