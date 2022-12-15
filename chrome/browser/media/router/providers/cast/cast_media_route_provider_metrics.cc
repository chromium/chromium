// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/providers/cast/cast_media_route_provider_metrics.h"

#include "base/containers/contains.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/time/time.h"
#include "media/base/audio_codecs.h"
#include "media/base/video_codecs.h"

using cast_channel::ReceiverAppType;

namespace media_router {

void RecordAppAvailabilityResult(cast_channel::GetAppAvailabilityResult result,
                                 base::TimeDelta duration) {
  if (result == cast_channel::GetAppAvailabilityResult::kUnknown)
    UMA_HISTOGRAM_TIMES(kHistogramAppAvailabilityFailure, duration);
  else
    UMA_HISTOGRAM_TIMES(kHistogramAppAvailabilitySuccess, duration);
}

void RecordLaunchSessionRequestSupportedAppTypes(
    std::vector<ReceiverAppType> types) {
  DCHECK(base::Contains(types, ReceiverAppType::kWeb));
  bool has_atv = false;
  for (ReceiverAppType type : types) {
    switch (type) {
      case ReceiverAppType::kAndroidTv:
        has_atv = true;
        break;
      case ReceiverAppType::kWeb:
      case ReceiverAppType::kOther:
        break;
    }
  }
  base::UmaHistogramEnumeration(kHistogramCastSupportedAppTypes,
                                has_atv ? ReceiverAppTypeSet::kAndroidTvAndWeb
                                        : ReceiverAppTypeSet::kWeb);
}

void RecordLaunchSessionResponseAppType(const base::Value* app_type) {
  if (!app_type) {
    return;
  }
  absl::optional<ReceiverAppType> type =
      cast_util::StringToEnum<ReceiverAppType>(app_type->GetString());
  if (type) {
    base::UmaHistogramEnumeration(kHistogramCastAppType, *type);
  } else {
    base::UmaHistogramEnumeration(kHistogramCastAppType,
                                  cast_channel::ReceiverAppType::kOther);
  }
}

void RecordSinkRemotingCompatibility(bool is_supported_model,
                                     bool is_supported_audio_codec,
                                     media::AudioCodec audio_codec,
                                     bool is_supported_video_codec,
                                     media::VideoCodec video_codec) {
  base::UmaHistogramBoolean(kHistogramSinkModelSupportsRemoting,
                            is_supported_model);
  if (!is_supported_model) {
    return;
  }

  if (is_supported_audio_codec) {
    base::UmaHistogramEnumeration(kHistogramSinkCapabilitySupportedAudioCodec,
                                  audio_codec);
  } else {
    base::UmaHistogramEnumeration(kHistogramSinkCapabilityUnsupportedAudioCodec,
                                  audio_codec);
  }

  if (is_supported_video_codec) {
    base::UmaHistogramEnumeration(kHistogramSinkCapabilitySupportedVideoCodec,
                                  video_codec);
  } else {
    base::UmaHistogramEnumeration(kHistogramSinkCapabilityUnsupportedVideoCodec,
                                  video_codec);
  }
}

}  // namespace media_router
