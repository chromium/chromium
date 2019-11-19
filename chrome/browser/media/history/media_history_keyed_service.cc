// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/history/media_history_keyed_service.h"

#include "base/task/post_task.h"
#include "chrome/browser/media/history/media_history_keyed_service_factory.h"
#include "content/public/browser/browser_context.h"

namespace media_history {

MediaHistoryKeyedService::MediaHistoryKeyedService(
    content::BrowserContext* browser_context) {
  DCHECK(!browser_context->IsOffTheRecord());

  auto db_task_runner = base::CreateUpdateableSequencedTaskRunner(
      {base::ThreadPool(), base::MayBlock(), base::TaskPriority::USER_VISIBLE,
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN});

  media_history_store_ = std::make_unique<MediaHistoryStore>(
      Profile::FromBrowserContext(browser_context), std::move(db_task_runner));
}

// static
MediaHistoryKeyedService* MediaHistoryKeyedService::Get(Profile* profile) {
  return MediaHistoryKeyedServiceFactory::GetForProfile(profile);
}

MediaHistoryKeyedService::~MediaHistoryKeyedService() = default;

}  // namespace media_history
