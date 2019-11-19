// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/providers/cast/activity_record.h"

#include "base/logging.h"
#include "chrome/browser/media/router/providers/cast/cast_internal_message_util.h"
#include "chrome/common/media_router/discovery/media_sink_internal.h"

namespace media_router {

ActivityRecord::ActivityRecord(
    const MediaRoute& route,
    const std::string& app_id,
    cast_channel::CastMessageHandler* message_handler,
    CastSessionTracker* session_tracker)
    : route_(route),
      app_id_(app_id),
      message_handler_(message_handler),
      session_tracker_(session_tracker) {}

ActivityRecord::~ActivityRecord() = default;

CastSession* ActivityRecord::GetSession() const {
  DCHECK(session_id_);
  CastSession* session = session_tracker_->GetSessionById(*session_id_);
  if (!session) {
    // TODO(crbug.com/905002): Add UMA metrics for this and other error
    // conditions.
    LOG(ERROR) << "Session not found: " << session_id_.value_or("<missing>");
  }
  return session;
}

void ActivityRecord::SetOrUpdateSession(const CastSession& session,
                                        const MediaSinkInternal& sink,
                                        const std::string& hash_token) {
  DVLOG(2) << "SetOrUpdateSession old session_id = "
           << session_id_.value_or("<missing>")
           << ", new session_id = " << session.session_id();
  route_.set_description(session.GetRouteDescription());
  sink_ = sink;
  if (session_id_) {
    DCHECK_EQ(*session_id_, session.session_id());
  } else {
    session_id_ = session.session_id();
    if (on_session_set_)
      std::move(on_session_set_).Run();
  }
}

}  // namespace media_router
