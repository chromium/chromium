// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ROUTER_DISCOVERY_ACCESS_CODE_ACCESS_CODE_MEDIA_SINK_UTIL_H_
#define CHROME_BROWSER_MEDIA_ROUTER_DISCOVERY_ACCESS_CODE_ACCESS_CODE_MEDIA_SINK_UTIL_H_

#include "base/values.h"
#include "chrome/browser/media/router/discovery/access_code/discovery_resources.pb.h"
#include "chrome/browser/media/router/discovery/mdns/media_sink_util.h"
#include "chrome/browser/ui/webui/access_code_cast/access_code_cast.mojom.h"
#include "components/access_code_cast/common/access_code_cast_metrics.h"
#include "components/media_router/common/discovery/media_sink_internal.h"

namespace media_router {

using AddSinkResultCode = access_code_cast::mojom::AddSinkResultCode;
using DiscoveryDevice = chrome_browser_media::proto::DiscoveryDevice;
using NetworkInfo = chrome_browser_media::proto::NetworkInfo;

// Creates a MediaSinkInternal from |discovery_device|. |cast_sink| is only
// valid if the returned result is |kOk|.
std::pair<std::optional<MediaSinkInternal>, CreateCastMediaSinkResult>
CreateAccessCodeMediaSink(const DiscoveryDevice& discovery_device);

base::Value::Dict CreateValueDictFromMediaSinkInternal(
    const MediaSinkInternal& sink);
std::optional<MediaSinkInternal> ParseValueDictIntoMediaSinkInternal(
    const base::Value::Dict& value_dict);

AccessCodeCastAddSinkResult AddSinkResultMetricsHelper(
    AddSinkResultCode result);

std::optional<net::IPEndPoint> GetIPEndPointFromValueDict(
    const base::Value::Dict& value_dict);

}  // namespace media_router

#endif  // CHROME_BROWSER_MEDIA_ROUTER_DISCOVERY_ACCESS_CODE_ACCESS_CODE_MEDIA_SINK_UTIL_H_
