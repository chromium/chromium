// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/providers/cast/chrome_cast_message_handler.h"

#include <string>

#include "base/functional/bind.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/media/router/data_decoder_util.h"
#include "components/embedder_support/user_agent_utils.h"
#include "components/media_router/common/providers/cast/channel/cast_message_handler.h"
#include "components/media_router/common/providers/cast/channel/cast_socket_service.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "services/data_decoder/public/cpp/data_decoder.h"

namespace media_router {

namespace {

void ParseJsonFromIoThread(
    const std::string& json,
    data_decoder::DataDecoder::ValueParseCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  GetDataDecoder().ParseJson(json, std::move(callback));
}

}  // namespace

cast_channel::CastMessageHandler* GetCastMessageHandler() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  static cast_channel::CastMessageHandler* instance =
      new cast_channel::CastMessageHandler(
          cast_channel::CastSocketService::GetInstance(),
          base::BindRepeating(&ParseJsonFromIoThread),
          embedder_support::GetUserAgent(),
          std::string(version_info::GetVersionNumber()),
          g_browser_process->GetApplicationLocale());
  return instance;
}

}  // namespace media_router
