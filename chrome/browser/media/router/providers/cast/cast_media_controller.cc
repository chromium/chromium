// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/providers/cast/cast_media_controller.h"

#include "base/callback_helpers.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "chrome/browser/media/router/providers/cast/app_activity.h"
#include "chrome/browser/media/router/providers/cast/cast_internal_message_util.h"
#include "components/cast_channel/cast_message_util.h"
#include "components/cast_channel/enum_table.h"

using cast_channel::V2MessageType;

namespace media_router {

namespace {

constexpr int kQueuePrevJumpValue = -1;
constexpr int kQueueNextJumpValue = 1;

void SetIfValid(std::string* out, const base::Value* value) {
  if (value && value->is_string())
    *out = value->GetString();
}
void SetIfValid(float* out, const base::Value* value) {
  if (!value)
    return;
  if (value->is_double()) {
    *out = value->GetDouble();
  } else if (value->is_int()) {
    *out = value->GetInt();
  }
}
void SetIfValid(int* out, const base::Value* value) {
  if (value && value->is_int())
    *out = value->GetInt();
}
void SetIfValid(bool* out, const base::Value* value) {
  if (value && value->is_bool())
    *out = value->GetBool();
}
void SetIfValid(base::TimeDelta* out, const base::Value* value) {
  if (!value)
    return;
  if (value->is_double()) {
    *out = base::TimeDelta::FromSecondsD(value->GetDouble());
  } else if (value->is_int()) {
    *out = base::TimeDelta::FromSeconds(value->GetInt());
  }
}

// If |value| has "width" and "height" fields with positive values, it gets
// converted into gfx::Size. Otherwise base::nullopt is returned.
base::Optional<gfx::Size> GetValidSize(const base::Value* value) {
  if (!value || !value->is_dict())
    return base::nullopt;
  int width = 0;
  int height = 0;
  SetIfValid(&width, value->FindPath("width"));
  SetIfValid(&height, value->FindPath("height"));
  if (width <= 0 || height <= 0)
    return base::nullopt;
  return base::make_optional<gfx::Size>(width, height);
}

}  // namespace

CastMediaController::CastMediaController(
    AppActivity* activity,
    mojo::PendingReceiver<mojom::MediaController> receiver,
    mojo::PendingRemote<mojom::MediaStatusObserver> observer)
    : sender_id_("sender-" + base::NumberToString(base::RandUint64())),
      activity_(activity),
      receiver_(this, std::move(receiver)),
      observer_(std::move(observer)) {}

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
  base::Value request = CreateVolumeRequest();
  request.SetBoolPath("message.volume.muted", mute);
  request.SetStringKey("type", "v2_message");
  request.SetStringKey("clientId", sender_id_);
  auto message = CastInternalMessage::From(std::move(request));
  activity_->SendSetVolumeRequestToReceiver(*message, base::DoNothing());
}

void CastMediaController::SetVolume(float volume) {
  if (session_id_.empty())
    return;
  base::Value request = CreateVolumeRequest();
  request.SetDoublePath("message.volume.level", volume);
  request.SetStringKey("type", "v2_message");
  request.SetStringKey("clientId", sender_id_);
  activity_->SendSetVolumeRequestToReceiver(
      *CastInternalMessage::From(std::move(request)), base::DoNothing());
}

void CastMediaController::Seek(base::TimeDelta time) {
  if (session_id_.empty())
    return;
  base::Value request = CreateMediaRequest(V2MessageType::kSeek);
  request.SetDoublePath("message.currentTime", time.InSecondsF());
  activity_->SendMediaRequestToReceiver(
      *CastInternalMessage::From(std::move(request)));
}

void CastMediaController::NextTrack() {
  if (session_id_.empty())
    return;
  // We do not use |kQueueNext| because not all receiver apps support it.
  // See crbug.com/1078601.
  base::Value request = CreateMediaRequest(V2MessageType::kQueueUpdate);
  request.SetIntPath("message.jump", kQueueNextJumpValue);
  activity_->SendMediaRequestToReceiver(
      *CastInternalMessage::From(std::move(request)));
}

void CastMediaController::PreviousTrack() {
  if (session_id_.empty())
    return;
  // We do not use |kQueuePrev| because not all receiver apps support it.
  // See crbug.com/1078601.
  base::Value request = CreateMediaRequest(V2MessageType::kQueueUpdate);
  request.SetIntPath("message.jump", kQueuePrevJumpValue);
  activity_->SendMediaRequestToReceiver(
      *CastInternalMessage::From(std::move(request)));
}

void CastMediaController::SetSession(const CastSession& session) {
  session_id_ = session.session_id();
  if (!session.value().is_dict())
    return;
  const base::Value* volume = session.value().FindPath("receiver.volume");
  if (!volume || !volume->is_dict())
    return;
  SetIfValid(&media_status_.volume, volume->FindKey("level"));
  SetIfValid(&media_status_.is_muted, volume->FindKey("muted"));
  const base::Value* volume_type = volume->FindKey("controlType");
  if (volume_type && volume_type->is_string()) {
    media_status_.can_set_volume = volume_type->GetString() != "fixed";
    media_status_.can_mute = media_status_.can_set_volume;
  }
  observer_->OnMediaStatusUpdated(media_status_.Clone());
}

void CastMediaController::SetMediaStatus(const base::Value& status_value) {
  UpdateMediaStatus(status_value);
  observer_->OnMediaStatusUpdated(media_status_.Clone());
}

base::Value CastMediaController::CreateMediaRequest(V2MessageType type) {
  base::Value message(base::Value::Type::DICTIONARY);
  message.SetIntKey("mediaSessionId", media_session_id_);
  message.SetStringKey("sessionId", session_id_);
  message.SetStringKey("type", cast_util::EnumToString(type).value().data());
  base::Value request(base::Value::Type::DICTIONARY);
  request.SetKey("message", std::move(message));
  request.SetStringKey("type", "v2_message");
  request.SetStringKey("clientId", sender_id_);
  return request;
}

base::Value CastMediaController::CreateVolumeRequest() {
  base::Value message(base::Value::Type::DICTIONARY);
  message.SetStringKey("sessionId", session_id_);
  // Muting also uses the |kSetVolume| message type.
  message.SetStringKey(
      "type",
      cast_util::EnumToString(V2MessageType::kSetVolume).value().data());
  message.SetKey("volume", base::Value(base::Value::Type::DICTIONARY));
  base::Value request(base::Value::Type::DICTIONARY);
  request.SetKey("message", std::move(message));
  return request;
}

void CastMediaController::UpdateMediaStatus(const base::Value& message_value) {
  const base::Value* status_list_value = message_value.FindKey("status");
  if (!status_list_value || !status_list_value->is_list())
    return;
  base::Value::ConstListView status_list = status_list_value->GetList();
  if (status_list.empty())
    return;
  const base::Value& status_value = status_list[0];
  if (!status_value.is_dict())
    return;
  SetIfValid(&media_session_id_, status_value.FindKey("mediaSessionId"));
  SetIfValid(&media_status_.title,
             status_value.FindPath("media.metadata.title"));
  SetIfValid(&media_status_.secondary_title,
             status_value.FindPath("media.metadata.subtitle"));
  SetIfValid(&media_status_.current_time, status_value.FindKey("currentTime"));
  SetIfValid(&media_status_.duration, status_value.FindPath("media.duration"));

  const base::Value* images = status_value.FindPath("media.metadata.images");
  if (images && images->is_list()) {
    media_status_.images.clear();
    for (const base::Value& image_value : images->GetList()) {
      if (!image_value.is_dict())
        continue;
      const std::string* url_string = image_value.FindStringKey("url");
      if (!url_string)
        continue;
      media_status_.images.emplace_back(base::in_place, GURL(*url_string),
                                        GetValidSize(&image_value));
    }
  }

  const base::Value* commands_value =
      status_value.FindListKey("supportedMediaCommands");
  if (commands_value) {
    const base::ListValue& commands_list =
        base::Value::AsListValue(*commands_value);
    // |can_set_volume| and |can_mute| are not used, because the receiver volume
    // info obtained in SetSession() is used instead.
    media_status_.can_play_pause =
        base::Contains(commands_list, base::Value(kMediaCommandPause));
    media_status_.can_seek =
        base::Contains(commands_list, base::Value(kMediaCommandSeek));
    media_status_.can_skip_to_next_track =
        base::Contains(commands_list, base::Value(kMediaCommandQueueNext));
    media_status_.can_skip_to_previous_track =
        base::Contains(commands_list, base::Value(kMediaCommandQueuePrev));
  }

  const base::Value* player_state = status_value.FindKey("playerState");
  if (player_state && player_state->is_string()) {
    const std::string& state = player_state->GetString();
    if (state == "PLAYING") {
      media_status_.play_state = mojom::MediaStatus::PlayState::PLAYING;
    } else if (state == "PAUSED") {
      media_status_.play_state = mojom::MediaStatus::PlayState::PAUSED;
    } else if (state == "BUFFERING") {
      media_status_.play_state = mojom::MediaStatus::PlayState::BUFFERING;
    }
  }
}

}  // namespace media_router
