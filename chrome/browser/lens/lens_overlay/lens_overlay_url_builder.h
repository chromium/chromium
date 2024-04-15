// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LENS_LENS_OVERLAY_LENS_OVERLAY_URL_BUILDER_H_
#define CHROME_BROWSER_LENS_LENS_OVERLAY_LENS_OVERLAY_URL_BUILDER_H_

#include <string>

#include "third_party/lens_server_proto/lens_overlay_cluster_info.pb.h"
#include "third_party/lens_server_proto/lens_overlay_request_id.pb.h"
#include "url/gurl.h"

namespace lens {
GURL AppendCommonSearchParametersToURL(const GURL& url_to_modify);

GURL BuildTextOnlySearchURL(const std::string& text_query);

GURL BuildLensSearchURL(std::optional<std::string> text_query,
                        std::unique_ptr<lens::LensOverlayRequestId> request_id,
                        lens::LensOverlayClusterInfo cluster_info);

}  // namespace lens

#endif  // CHROME_BROWSER_LENS_LENS_OVERLAY_LENS_OVERLAY_URL_BUILDER_H_
