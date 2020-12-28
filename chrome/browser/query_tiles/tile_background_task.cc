// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/query_tiles/tile_background_task.h"

#include "base/feature_list.h"
#include "base/macros.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_key.h"
#include "chrome/browser/query_tiles/tile_service_factory.h"
#include "components/query_tiles/switches.h"

namespace query_tiles {

TileBackgroundTask::TileBackgroundTask() = default;

TileBackgroundTask::~TileBackgroundTask() = default;

void TileBackgroundTask::OnStartTaskInReducedMode(
    const TaskParameters& task_params,
    TaskFinishedCallback callback,
    SimpleFactoryKey* key) {
  callback_ = std::move(callback);
}

void TileBackgroundTask::OnStartTaskWithFullBrowser(
    const TaskParameters& task_params,
    TaskFinishedCallback callback,
    content::BrowserContext* browser_context) {
  auto* profile_key =
      Profile::FromBrowserContext(browser_context)->GetProfileKey();
  StartFetchTask(profile_key, false, std::move(callback));
}

void TileBackgroundTask::OnFullBrowserLoaded(
    content::BrowserContext* browser_context) {
  auto* profile_key =
      Profile::FromBrowserContext(browser_context)->GetProfileKey();
  StartFetchTask(profile_key, false, std::move(callback_));
}

bool TileBackgroundTask::OnStopTask(const TaskParameters& task_params) {
  // Don't reschedule.
  return false;
}

void TileBackgroundTask::StartFetchTask(SimpleFactoryKey* key,
                                        bool is_from_reduced_mode,
                                        TaskFinishedCallback callback) {
  if (is_from_reduced_mode)
    return;
  auto* tile_service = TileServiceFactory::GetInstance()->GetForKey(key);
  DCHECK(tile_service);
  if (!base::FeatureList::IsEnabled(query_tiles::features::kQueryTiles) ||
      (!base::FeatureList::IsEnabled(query_tiles::features::kQueryTilesInNTP) &&
       !base::FeatureList::IsEnabled(
           query_tiles::features::kQueryTilesInOmnibox))) {
    tile_service->CancelTask();
  } else {
    tile_service->StartFetchForTiles(is_from_reduced_mode, std::move(callback));
  }
}

}  // namespace query_tiles
