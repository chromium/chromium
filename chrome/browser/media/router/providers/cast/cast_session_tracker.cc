// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/providers/cast/cast_session_tracker.h"

#include "base/functional/bind.h"
#include "base/observer_list.h"
#include "base/ranges/algorithm.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/media/router/providers/cast/chrome_cast_message_handler.h"
#include "chrome/browser/media/router/providers/cast/dual_media_sink_service.h"
#include "components/media_router/common/providers/cast/channel/cast_socket_service.h"

namespace media_router {

CastSessionTracker::Observer::~Observer() = default;

CastSessionTracker::~CastSessionTracker() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  media_sink_service_->RemoveObserver(this);
  message_handler_->RemoveObserver(this);
}

// static
CastSessionTracker* CastSessionTracker::GetInstance() {
  if (instance_for_test_)
    return instance_for_test_;

  static CastSessionTracker* instance = new CastSessionTracker(
      DualMediaSinkService::GetInstance()->GetCastMediaSinkServiceBase(),
      GetCastMessageHandler(),
      cast_channel::CastSocketService::GetInstance()->task_runner());
  return instance;
}

void CastSessionTracker::AddObserver(CastSessionTracker::Observer* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observers_.AddObserver(observer);
}

void CastSessionTracker::RemoveObserver(
    CastSessionTracker::Observer* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observers_.RemoveObserver(observer);
}

const CastSessionTracker::SessionMap& CastSessionTracker::GetSessions() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return sessions_by_sink_id_;
}

CastSession* CastSessionTracker::GetSessionById(
    const std::string& session_id) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto it = base::ranges::find(
      sessions_by_sink_id_, session_id,
      [](const auto& entry) { return entry.second->session_id(); });
  return it != sessions_by_sink_id_.end() ? it->second.get() : nullptr;
}

CastSessionTracker::CastSessionTracker(
    MediaSinkServiceBase* media_sink_service,
    cast_channel::CastMessageHandler* message_handler,
    const scoped_refptr<base::SequencedTaskRunner>& task_runner)
    : media_sink_service_(media_sink_service),
      message_handler_(message_handler) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
  // This is safe because |this| will never be destroyed (except in unit tests).
  task_runner->PostTask(FROM_HERE,
                        base::BindOnce(&CastSessionTracker::InitOnIoThread,
                                       base::Unretained(this)));
}

// This method needs to be separate from the constructor because the constructor
// needs to be called from the UI thread, but observers can only be added in an
// IO thread.
void CastSessionTracker::InitOnIoThread() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  media_sink_service_->AddObserver(this);
  message_handler_->AddObserver(this);
}

void CastSessionTracker::HandleReceiverStatusMessage(
    const MediaSinkInternal& sink,
    const base::Value::Dict& message) {
  const base::Value::Dict* status = message.FindDict("status");
  auto session = status ? CastSession::From(sink, *status) : nullptr;
  const MediaSink::Id& sink_id = sink.sink().id();
  if (!session) {
    if (sessions_by_sink_id_.erase(sink_id)) {
      for (auto& observer : observers_)
        observer.OnSessionRemoved(sink);
    }
    return;
  }

  auto it = sessions_by_sink_id_.find(sink_id);
  if (it == sessions_by_sink_id_.end()) {
    it = sessions_by_sink_id_.emplace(sink_id, std::move(session)).first;
  } else {
    it->second->UpdateSession(std::move(session));
  }

  for (auto& observer : observers_)
    observer.OnSessionAddedOrUpdated(sink, *it->second);
}

void CastSessionTracker::HandleMediaStatusMessage(
    const MediaSinkInternal& sink,
    const base::Value::Dict& message) {
  DVLOG(2) << "Initial MEDIA_STATUS: " << message;
  auto session_it = sessions_by_sink_id_.find(sink.sink().id());
  if (session_it == sessions_by_sink_id_.end()) {
    DVLOG(2) << "Got media status message, but no session for: "
             << sink.sink().id();
    return;
  }

  // NOTE(mfoltz): There might be some way to map the mediaSessionId in
  // |message| to the cast application sessionId... but we might be getting a
  // media status for a playback that was initiated from another sender, in
  // which case we won't have any association to look up the sessionId.
  //
  // So I guess we have to assume that a media status received from a sink is
  // for the session we happen to currently know is on that sink.
  CastSession* session = session_it->second.get();
  const std::string& session_id = session->session_id();
  base::Value::Dict updated_message = message.Clone();
  updated_message.Set("sessionId", session_id);

  base::Value::List* updated_status = updated_message.FindList("status");
  if (!updated_status) {
    DVLOG(2) << "No status list in media status message.";
    return;
  }

  // Ensure every item in |updated_status| is a dictionary.
  updated_status->EraseIf([](auto const& media) { return !media.is_dict(); });

  // Backfill messages from receivers to make them compatible with Cast SDK.
  for (auto& media : *updated_status) {
    base::Value::Dict& media_dict = media.GetDict();
    media_dict.Set("sessionId", session_id);
    std::optional<int> supported_media_commands =
        media_dict.FindInt("supportedMediaCommands");
    if (!supported_media_commands.has_value())
      continue;

    media_dict.Set(
        "supportedMediaCommands",
        SupportedMediaCommandsToListValue(supported_media_commands.value()));
  }

  CopySavedMediaFieldsToMediaList(session, *updated_status);

  DVLOG(2) << "Final updated MEDIA_STATUS: " << *updated_status;
  session->UpdateMedia(*updated_status);

  std::optional<int> request_id =
      cast_channel::GetRequestIdFromResponse(updated_message);

  // Notify observers of media update.
  for (auto& observer : observers_)
    observer.OnMediaStatusUpdated(sink, updated_message, request_id);
}

void CastSessionTracker::CopySavedMediaFieldsToMediaList(
    CastSession* session,
    base::Value::List& media_list) {
  // When |session| has saved media objects with a mediaSessionId corresponding
  // to a value in |media_list|, copy the 'media' field from the saved objects
  // to the corresponding objects in |media_list|.
  const base::Value::List* session_media_value_list =
      session->value().FindList("media");
  if (!session_media_value_list)
    return;

  for (auto& media : media_list) {
    base::Value::Dict& media_dict = media.GetDict();
    std::optional<int> media_session_id = media_dict.FindInt("mediaSessionId");
    if (!media_session_id.has_value() || media_dict.Find("media"))
      continue;

    auto session_media_it = base::ranges::find(
        *session_media_value_list, media_session_id,
        [](const base::Value& session_media) {
          return session_media.GetDict().FindInt("mediaSessionId");
        });
    if (session_media_it == session_media_value_list->end())
      continue;
    const base::Value* session_media =
        session_media_it->GetDict().Find("media");
    if (session_media)
      media_dict.Set("media", session_media->Clone());
  }
}

const MediaSinkInternal* CastSessionTracker::GetSinkByChannelId(
    int channel_id) const {
  for (const auto& sink : media_sink_service_->GetSinks()) {
    if (sink.second.cast_data().cast_channel_id == channel_id)
      return &sink.second;
  }
  return nullptr;
}

void CastSessionTracker::OnSinkAddedOrUpdated(const MediaSinkInternal& sink) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  message_handler_->RequestReceiverStatus(sink.cast_data().cast_channel_id);
}

void CastSessionTracker::OnSinkRemoved(const MediaSinkInternal& sink) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (sessions_by_sink_id_.erase(sink.sink().id())) {
    for (auto& observer : observers_)
      observer.OnSessionRemoved(sink);
  }
}

void CastSessionTracker::OnAppMessage(int channel_id,
                                      const CastMessage& message) {}

void CastSessionTracker::OnInternalMessage(
    int channel_id,
    const cast_channel::InternalMessage& message) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // It's possible for a session to have been started/discovered on a different
  // channel_id than the one we got the latest RECEIVER_STATUS on, but this
  // should be okay, since we are mapping everything back to the sink_id, which
  // should be constant.
  const MediaSinkInternal* sink = GetSinkByChannelId(channel_id);
  if (!sink) {
    DVLOG(2) << "Received message from channel without sink: " << channel_id;
    return;
  }

  if (message.type == cast_channel::CastMessageType::kReceiverStatus) {
    DVLOG(2) << "Got receiver status: " << message.message;
    HandleReceiverStatusMessage(*sink, message.message);
  } else if (message.type == cast_channel::CastMessageType::kMediaStatus) {
    DVLOG(2) << "Got media status: " << message.message;
    HandleMediaStatusMessage(*sink, message.message);
  }
}

// static
void CastSessionTracker::SetInstanceForTest(
    CastSessionTracker* session_tracker) {
  instance_for_test_ = session_tracker;
}

void CastSessionTracker::SetSessionForTest(
    const MediaSink::Id& sink_id,
    std::unique_ptr<CastSession> session) {
  DCHECK(session);
  sessions_by_sink_id_[sink_id] = std::move(session);
}

// static
CastSessionTracker* CastSessionTracker::instance_for_test_ = nullptr;

}  // namespace media_router
