// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/projector_app/projector_message_handler.h"

#include <memory>
#include <string>

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/projector/projector_controller.h"
#include "ash/public/cpp/projector/projector_new_screencast_precondition.h"
#include "ash/webui/projector_app/projector_app_client.h"
#include "ash/webui/projector_app/projector_screencast.h"
#include "base/check.h"
#include "base/functional/bind.h"
#include "base/json/values_util.h"
#include "base/time/time.h"
#include "content/public/browser/web_ui.h"
#include "url/gurl.h"

namespace ash {

ProjectorMessageHandler::ProjectorMessageHandler() = default;
ProjectorMessageHandler::~ProjectorMessageHandler() = default;

base::WeakPtr<ProjectorMessageHandler> ProjectorMessageHandler::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void ProjectorMessageHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "getVideo", base::BindRepeating(&ProjectorMessageHandler::GetVideo,
                                      base::Unretained(this)));
}

void ProjectorMessageHandler::GetVideo(const base::Value::List& args) {
  // Two arguments. The first is callback id, and the second is the list
  // containing the item id and resource key.
  CHECK_EQ(args.size(), 2u);
  const std::string& js_callback_id = args[0].GetString();
  const auto& func_args = args[1].GetList();

  CHECK_EQ(func_args.size(), 2u);

  const std::string& video_file_id = func_args[0].GetString();
  std::string resource_key;
  if (func_args[1].is_string())
    resource_key = func_args[1].GetString();

  ProjectorAppClient::Get()->GetVideo(
      video_file_id, resource_key,
      base::BindOnce(&ProjectorMessageHandler::OnVideoLocated, GetWeakPtr(),
                     js_callback_id));
}

void ProjectorMessageHandler::OnVideoLocated(
    const std::string& js_callback_id,
    std::unique_ptr<ProjectorScreencastVideo> video,
    const std::string& error_message) {
  AllowJavascript();

  if (!error_message.empty()) {
    RejectJavascriptCallback(base::Value(js_callback_id),
                             base::Value(error_message));
    return;
  }
  DCHECK(video)
      << "If there is no error message, then video should not be nullptr";
  ResolveJavascriptCallback(base::Value(js_callback_id), video->ToValue());
}
}  // namespace ash
