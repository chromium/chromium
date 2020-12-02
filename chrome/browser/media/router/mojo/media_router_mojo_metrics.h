// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ROUTER_MOJO_MEDIA_ROUTER_MOJO_METRICS_H_
#define CHROME_BROWSER_MEDIA_ROUTER_MOJO_MEDIA_ROUTER_MOJO_METRICS_H_

#include "base/gtest_prod_util.h"
#include "base/time/time.h"
#include "components/media_router/common/media_route_provider_helper.h"
#include "components/media_router/common/media_source.h"
#include "components/media_router/common/route_request_result.h"
#include "content/public/browser/web_contents.h"

namespace base {
class Version;
}  // namespace base

namespace extensions {
class Extension;
}  // namespace extensions

namespace media_router {

// NOTE: Do not renumber enums as that would confuse interpretation of
// previously logged data. When making changes, also update the enum lists
// in tools/metrics/histograms/enums.xml to keep it in sync.

// Why the Media Route Provider process was woken up.
enum class MediaRouteProviderWakeReason {
  CREATE_ROUTE = 0,
  JOIN_ROUTE = 1,
  TERMINATE_ROUTE = 2,
  SEND_SESSION_MESSAGE = 3,
  SEND_SESSION_BINARY_MESSAGE = 4,
  DETACH_ROUTE = 5,
  START_OBSERVING_MEDIA_SINKS = 6,
  STOP_OBSERVING_MEDIA_SINKS = 7,
  START_OBSERVING_MEDIA_ROUTES = 8,
  STOP_OBSERVING_MEDIA_ROUTES = 9,
  START_LISTENING_FOR_ROUTE_MESSAGES = 10,
  STOP_LISTENING_FOR_ROUTE_MESSAGES = 11,
  CONNECTION_ERROR = 12,
  REGISTER_MEDIA_ROUTE_PROVIDER = 13,
  CONNECT_ROUTE_BY_ROUTE_ID = 14,
  ENABLE_MDNS_DISCOVERY = 15,
  UPDATE_MEDIA_SINKS = 16,
  SEARCH_SINKS = 17,  // Obsolete
  PROVIDE_SINKS = 18,
  CREATE_MEDIA_ROUTE_CONTROLLER = 19,
  ROUTE_CONTROLLER_COMMAND = 20,

  // NOTE: Add entries only immediately above this line.
  TOTAL_COUNT = 21
};

// The install status of the Media Router component extension.
enum class MediaRouteProviderVersion {
  // Installed but version is invalid or cannot be determined.
  UNKNOWN = 0,
  // Installed and the extension version matches the browser version.
  SAME_VERSION_AS_CHROME = 1,
  // Installed and the extension version is one version behind the browser
  // version.
  ONE_VERSION_BEHIND_CHROME = 2,
  // Installed and the extension version is more than one version behind the
  // browser version.
  MULTIPLE_VERSIONS_BEHIND_CHROME = 3,
  // Note: Add entries only immediately above this line.
  TOTAL_COUNT = 4
};

// The outcome of an attempt to wake the Media Router component event page.
enum class MediaRouteProviderWakeup {
  SUCCESS = 0,
  ERROR_UNKNOWN = 1,
  ERROR_TOO_MANY_RETRIES = 2,
  // Note: Add entries only immediately above this line.
  TOTAL_COUNT = 3
};

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
  // Records the installed version of the Media Router component extension.
  static void RecordMediaRouteProviderVersion(
      const extensions::Extension& extension);

  // Records why the media route provider extension was woken up.
  static void RecordMediaRouteProviderWakeReason(
      MediaRouteProviderWakeReason reason);

  // Records the outcome of an attempt to wake the Media Router component event
  // page.
  static void RecordMediaRouteProviderWakeup(MediaRouteProviderWakeup wakeup);

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

 private:
  FRIEND_TEST_ALL_PREFIXES(MediaRouterMojoMetricsTest,
                           TestGetMediaRouteProviderVersion);

  // Returns the version status of the Media Router component extension.
  static MediaRouteProviderVersion GetMediaRouteProviderVersion(
      const base::Version& extension_version,
      const base::Version& browser_version);
};

}  // namespace media_router

#endif  // CHROME_BROWSER_MEDIA_ROUTER_MOJO_MEDIA_ROUTER_MOJO_METRICS_H_
