// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LENS_LENS_OVERLAY_LENS_OVERLAY_QUERY_CONTROLLER_H_
#define CHROME_BROWSER_LENS_LENS_OVERLAY_LENS_OVERLAY_QUERY_CONTROLLER_H_

#include "base/functional/callback.h"
#include "chrome/browser/resources/lens/server/proto/lens_overlay_request.pb.h"
#include "chrome/browser/resources/lens/server/proto/lens_overlay_response.pb.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "url/gurl.h"

namespace lens {

// Manages queries on behalf of a Lens overlay.
class LensOverlayQueryController {
 public:
  LensOverlayQueryController();
  ~LensOverlayQueryController();

  // Starts a query flow by sending a request to Lens using the screenshot,
  // returning the response to the callback.
  void StartQueryFlow(
      const SkBitmap& screenshot,
      base::OnceCallback<void(lens::proto::LensOverlayResponse)> callback);

  // Clears the state and resets stored values.
  void EndQuery();

  // Sends a request to Lens representing an interaction following the initial
  // query, returning a URL to load and suggest signals to the callback.
  void SendInteraction(
      lens::proto::LensOverlayRequest request,
      base::OnceCallback<void(const GURL&, std::string)> callback);

 private:
};

}  // namespace lens

#endif  // CHROME_BROWSER_LENS_LENS_OVERLAY_LENS_OVERLAY_QUERY_CONTROLLER_H_
