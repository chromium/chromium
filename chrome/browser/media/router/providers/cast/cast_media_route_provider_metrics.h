// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ROUTER_PROVIDERS_CAST_CAST_MEDIA_ROUTE_PROVIDER_METRICS_H_
#define CHROME_BROWSER_MEDIA_ROUTER_PROVIDERS_CAST_CAST_MEDIA_ROUTE_PROVIDER_METRICS_H_

#include "base/time/time.h"
#include "base/values.h"
#include "components/media_router/common/providers/cast/channel/cast_message_util.h"
#include "components/media_router/common/providers/cast/channel/enum_table.h"

namespace media {
enum class AudioCodec;
enum class VideoCodec;
}  // namespace media

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
// Histogram name for whether the sink's model is known to support remoting.
static constexpr char kHistogramSinkModelSupportsRemoting[] =
    "MediaRouter.RemotePlayback.SinkModelCompatibility";
// Histogram name for AudioCodec of the RemotePlayback MediaSource that the
// sink's audio capability supports/does not support rendering.
static constexpr char kHistogramSinkCapabilitySupportedAudioCodec[] =
    "MediaRouter.RemotePlayback.SinkCapability.SupportedAudioCodec";
static constexpr char kHistogramSinkCapabilityUnsupportedAudioCodec[] =
    "MediaRouter.RemotePlayback.SinkCapability.UnsupportedAudioCodec";
// Histogram name for VideoCodec of the RemotePlayback MediaSource that the
// sink's video capability supports/does not support rendering.
static constexpr char kHistogramSinkCapabilitySupportedVideoCodec[] =
    "MediaRouter.RemotePlayback.SinkCapability.SupportedVideoCodec";
static constexpr char kHistogramSinkCapabilityUnsupportedVideoCodec[] =
    "MediaRouter.RemotePlayback.SinkCapability.UnsupportedVideoCodec";

// For the puprose of collecting data for
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

// Records whether the sink supports Media Remoting.
void RecordSinkRemotingCompatibility(
    bool is_supported_model,
    bool is_supported_audio_codec,
    absl::optional<media::AudioCodec> audio_codec,
    bool is_supported_video_codec,
    media::VideoCodec video_codec);

}  // namespace media_router

#endif  // CHROME_BROWSER_MEDIA_ROUTER_PROVIDERS_CAST_CAST_MEDIA_ROUTE_PROVIDER_METRICS_H_
