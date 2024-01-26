// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/providers/cast/cast_media_controller.h"

#include "base/containers/contains.h"
#include "base/functional/callback_helpers.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "chrome/browser/media/router/providers/cast/app_activity.h"
#include "chrome/browser/media/router/providers/cast/cast_internal_message_util.h"
#include "components/media_router/common/providers/cast/channel/cast_message_util.h"
#include "components/media_router/common/providers/cast/channel/enum_table.h"
#include "third_party/abseil-cpp/absl/utility/utility.h"

using cast_channel::V2MessageType;

namespace media_router {

namespace {

constexpr int kQueuePrevJumpValue = -1;
constexpr int kQueueNextJumpValue = 1;

void SetIfValid(std::string* out, const base::Value* value) {
  if (value && value->is_string())
    *out = value->GetString();
}
void SetIfValid(bool* out, const base::Value* value) {
  if (value && value->is_bool())
    *out = value->GetBool();
}

void SetIfNonNegative(float* out, const base::Value* value) {
  if (!value)
    return;
  if (value->is_double() && value->GetDouble() >= 0) {
    *out = value->GetDouble();
  } else if (value->is_int() && value->GetInt() >= 0) {
    *out = value->GetInt();
  }
}
void SetIfNonNegative(int* out, const base::Value* value) {
  if (value && value->is_int() && value->GetInt() >= 0)
    *out = value->GetInt();
}
void SetIfNonNegative(base::TimeDelta* out, const base::Value* value) {
  if (!value)
    return;
  if (value->is_double() && value->GetDouble() >= 0) {
    *out = base::Seconds(value->GetDouble());
  } else if (value->is_int() && value->GetInt() >= 0) {
    *out = base::Seconds(value->GetInt());
  }
}

// If |value| has "width" and "height" fields with positive values, it gets
// converted into gfx::Size. Otherwise std::nullopt is returned.
std::optional<gfx::Size> GetValidSize(const base::Value::Dict& dict) {
  int width = 0;
  int height = 0;
  SetIfNonNegative(&width, dict.Find("width"));
  SetIfNonNegative(&height, dict.Find("height"));
  if (width <= 0 || height <= 0)
    return std::nullopt;
  return std::make_optional<gfx::Size>(width, height);
}

}  // namespace

CastMediaController::CastMediaController(AppActivity* activity)
    : sender_id_("sender-" + base::NumberToString(base::RandUint64())),
      activity_(activity) {}

CastMediaController::~CastMediaController() {}

void CastMediaController::Play() {
  if (session_id_.empty())
    return;
  activity_->SendMediaRequestToReceiver(
      *CastInternalMessage::From(CreateMediaRequest(V2MessageType::kPlay)));
}

void CastMediaController::Pause() {
  if (session_id_.empty())
    return;
  activity_->SendMediaRequestToReceiver(
      *CastInternalMessage::From(CreateMediaRequest(V2MessageType::kPause)));
}

void CastMediaController::SetMute(bool mute) {
  if (session_id_.empty())
    return;
  auto message = CastInternalMessage::From(
      CreateVolumeRequest()
          .SetByDottedPath("message.volume.muted", mute)
          .Set("type", "v2_message")
          .Set("clientId", sender_id_));
  activity_->SendSetVolumeRequestToReceiver(*message, base::DoNothing());
}

void CastMediaController::SetVolume(float volume) {
  if (session_id_.empty())
    return;
  activity_->SendSetVolumeRequestToReceiver(
      *CastInternalMessage::From(
          CreateVolumeRequest()
              .SetByDottedPath("message.volume.level", volume)
              .Set("type", "v2_message")
              .Set("clientId", sender_id_)),
      base::DoNothing());
}

void CastMediaController::Seek(base::TimeDelta time) {
  if (session_id_.empty())
    return;
  activity_->SendMediaRequestToReceiver(*CastInternalMessage::From(
      CreateMediaRequest(V2MessageType::kSeek)
          .SetByDottedPath("message.currentTime", time.InSecondsF())));
}

void CastMediaController::NextTrack() {
  if (session_id_.empty())
    return;
  // We do not use |kQueueNext| because not all receiver apps support it.
  // See crbug.com/1078601.
  activity_->SendMediaRequestToReceiver(*CastInternalMessage::From(
      CreateMediaRequest(V2MessageType::kQueueUpdate)
          .SetByDottedPath("message.jump", kQueueNextJumpValue)));
}

void CastMediaController::PreviousTrack() {
  if (session_id_.empty())
    return;
  // We do not use |kQueuePrev| because not all receiver apps support it.
  // See crbug.com/1078601.
  activity_->SendMediaRequestToReceiver(*CastInternalMessage::From(
      CreateMediaRequest(V2MessageType::kQueueUpdate)
          .SetByDottedPath("message.jump", kQueuePrevJumpValue)));
}

void CastMediaController::AddMediaController(
    mojo::PendingReceiver<mojom::MediaController> receiver,
    mojo::PendingRemote<mojom::MediaStatusObserver> observer) {
  receivers_.Add(this, std::move(receiver));
  observers_.Add(std::move(observer));
}

void CastMediaController::SetSession(const CastSession& session) {
  session_id_ = session.session_id();
  const base::Value::Dict* volume =
      session.value().FindDictByDottedPath("receiver.volume");
  if (!volume)
    return;
  SetIfNonNegative(&media_status_.volume, volume->Find("level"));
  SetIfValid(&media_status_.is_muted, volume->Find("muted"));
  const std::string* volume_type = volume->FindString("controlType");
  if (volume_type) {
    media_status_.can_set_volume = *volume_type != "fixed";
    media_status_.can_mute = media_status_.can_set_volume;
  }
  for (const auto& observer : observers_) {
    observer->OnMediaStatusUpdated(media_status_.Clone());
  }
}

void CastMediaController::SetMediaStatus(
    const base::Value::Dict& status_value) {
  UpdateMediaStatus(status_value);
  for (const auto& observer : observers_) {
    observer->OnMediaStatusUpdated(media_status_.Clone());
  }
}

base::Value::Dict CastMediaController::CreateMediaRequest(V2MessageType type) {
  return base::Value::Dict()
      .Set("message",
           base::Value::Dict()
               .Set("mediaSessionId", media_session_id_)
               .Set("sessionId", session_id_)
               .Set("type", cast_util::EnumToString(type).value().data()))
      .Set("type", "v2_message")
      .Set("clientId", sender_id_);
}

base::Value::Dict CastMediaController::CreateVolumeRequest() {
  return base::Value::Dict().Set(
      "message",
      base::Value::Dict()
          .Set("sessionId", session_id_)
          // Muting also uses the |kSetVolume| message type.
          .Set(
              "type",
              cast_util::EnumToString(V2MessageType::kSetVolume).value().data())
          .Set("volume", base::Value::Dict()));
}

void CastMediaController::UpdateMediaStatus(
    const base::Value::Dict& message_value) {
  const base::Value::List* status_list = message_value.FindList("status");
  if (!status_list)
    return;
  if (status_list->empty())
    return;
  const base::Value& status_value = (*status_list)[0];
  if (!status_value.is_dict())
    return;
  SetIfNonNegative(&media_session_id_,
                   status_value.GetDict().Find("mediaSessionId"));
  SetIfValid(&media_status_.title,
             status_value.GetDict().FindByDottedPath("media.metadata.title"));
  SetIfValid(
      &media_status_.secondary_title,
      status_value.GetDict().FindByDottedPath("media.metadata.subtitle"));
  SetIfNonNegative(&media_status_.current_time,
                   status_value.GetDict().Find("currentTime"));
  SetIfNonNegative(&media_status_.duration,
                   status_value.GetDict().FindByDottedPath("media.duration"));

  const base::Value::List* images =
      status_value.GetDict().FindListByDottedPath("media.metadata.images");
  if (images) {
    media_status_.images.clear();
    for (const base::Value& image_value : *images) {
      if (!image_value.is_dict())
        continue;
      const base::Value::Dict& image_dict = image_value.GetDict();
      const std::string* url_string = image_dict.FindString("url");
      if (!url_string)
        continue;
      media_status_.images.emplace_back(std::in_place, GURL(*url_string),
                                        GetValidSize(image_dict));
    }
  }

  const base::Value::List* commands_list =
      status_value.GetDict().FindList("supportedMediaCommands");
  if (commands_list) {
    // |can_set_volume| and |can_mute| are not used, because the receiver volume
    // info obtained in SetSession() is used instead.
    media_status_.can_play_pause =
        base::Contains(*commands_list, base::Value(kMediaCommandPause));
    media_status_.can_seek =
        base::Contains(*commands_list, base::Value(kMediaCommandSeek));
    media_status_.can_skip_to_next_track =
        base::Contains(*commands_list, base::Value(kMediaCommandQueueNext));
    media_status_.can_skip_to_previous_track =
        base::Contains(*commands_list, base::Value(kMediaCommandQueuePrev));
  }

  const std::string* player_state =
      status_value.GetDict().FindString("playerState");
  if (player_state) {
    if (*player_state == "PLAYING") {
      media_status_.play_state = mojom::MediaStatus::PlayState::PLAYING;
    } else if (*player_state == "PAUSED") {
      media_status_.play_state = mojom::MediaStatus::PlayState::PAUSED;
    } else if (*player_state == "BUFFERING") {
      media_status_.play_state = mojom::MediaStatus::PlayState::BUFFERING;
    }
  }
}

}  // namespace media_router
