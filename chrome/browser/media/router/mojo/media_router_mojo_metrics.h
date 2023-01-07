// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ROUTER_MOJO_MEDIA_ROUTER_MOJO_METRICS_H_
#define CHROME_BROWSER_MEDIA_ROUTER_MOJO_MEDIA_ROUTER_MOJO_METRICS_H_

#include "base/gtest_prod_util.h"
#include "components/media_router/common/media_route_provider_helper.h"
#include "components/media_router/common/media_source.h"
#include "components/media_router/common/route_request_result.h"
#include "content/public/browser/web_contents.h"

namespace media_router {

// Whether audio has been played since the last navigation. Do not modify
// existing values, since they are used for metrics reporting. Add new values
// only at the bottom, and also update tools/metrics/histograms/enums.xml.
enum class WebContentsAudioState {
  kWasNeverAudible = 0,
  kIsCurrentlyAudible = 1,
  kWasPreviouslyAudible = 2,  // Was playing audio, but not currently.
};

class MediaRouterMojoMetrics {
 public:
  // Records whether the Media Route Provider succeeded or failed to create a
  // controller for a media route.
  static void RecordMediaRouteControllerCreationResult(bool success);

  // Records the audio playback state of a WebContents that is being
  // tab-mirrored.
  static void RecordTabMirroringMetrics(content::WebContents* web_contents);

  // Records the audio capture setting of a site-initiated mirroring session.
  static void RecordSiteInitiatedMirroringStarted(
      content::WebContents* web_contents,
      const MediaSource& media_source);
};

}  // namespace media_router

#endif  // CHROME_BROWSER_MEDIA_ROUTER_MOJO_MEDIA_ROUTER_MOJO_METRICS_H_
