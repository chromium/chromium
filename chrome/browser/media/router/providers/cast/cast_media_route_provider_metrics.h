// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ROUTER_PROVIDERS_CAST_CAST_MEDIA_ROUTE_PROVIDER_METRICS_H_
#define CHROME_BROWSER_MEDIA_ROUTER_PROVIDERS_CAST_CAST_MEDIA_ROUTE_PROVIDER_METRICS_H_

#include "base/time/time.h"
#include "base/values.h"
#include "components/media_router/common/providers/cast/channel/cast_message_util.h"
#include "components/media_router/common/providers/cast/channel/enum_table.h"

namespace media_router {

// Histogram names for app availability.
static constexpr char kHistogramAppAvailabilitySuccess[] =
    "MediaRouter.Cast.App.Availability.Success";
static constexpr char kHistogramAppAvailabilityFailure[] =
    "MediaRouter.Cast.App.Availability.Failure";
static constexpr char kHistogramAudioSender[] =
    "MediaRouter.CastStreaming.Audio.PlaybackOnReceiver";
// Histogram name for appType set by the receiver device.
static constexpr char kHistogramCastAppType[] =
    "MediaRouter.Cast.LaunchSessionResponse.AppType";

// For the purpose of collecting data for
// MediaRouter.Cast.LaunchSessionRequest.SupportedAppType histogram, this enum
// contains all possible combinations of ReceiverAppType.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused. Please keep it in sync with
// ReceiverAppTypeSet in tools/metrics/histograms/enums.xml.
enum class ReceiverAppTypeSet {
  // Web-based Cast receiver apps. This is supported by all Cast media source
  // by default.
  kWeb = 0,

  // A media source may support launching an Android TV app in addition to a
  // Cast web app.
  kAndroidTvAndWeb = 1,

  // Do not reorder existing entries. Add new types above |kMaxValue|.
  kMaxValue = kAndroidTvAndWeb,
};

// Records the result of an app availability request and how long it took.
// If |result| is kUnknown, then a failure is recorded. Otherwise, a success
// is recorded.
void RecordAppAvailabilityResult(cast_channel::GetAppAvailabilityResult result,
                                 base::TimeDelta duration);


// Records the type of app (web app, native Android app etc.) launched on the
// receiver side in an Enumeration histogram.
// If |app_type| is "WEB", a kWeb will be recorded. If |app_type| is
// "ANDROID_TV", a kAndroidTv will be recorded. Otherwise, a kOther will be
// recorded.
void RecordLaunchSessionResponseAppType(const base::Value* app_type);

}  // namespace media_router

#endif  // CHROME_BROWSER_MEDIA_ROUTER_PROVIDERS_CAST_CAST_MEDIA_ROUTE_PROVIDER_METRICS_H_
