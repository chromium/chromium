// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/query_tiles/tile_background_task.h"

#include "base/feature_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_key.h"
#include "chrome/browser/query_tiles/query_tile_utils.h"
#include "chrome/browser/query_tiles/tile_service_factory.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "components/query_tiles/switches.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_service.h"

namespace query_tiles {
namespace {
bool IsSearchEngineSupported(TemplateURLService* template_url_service) {
  // Could be first start case, return true by default.
  if (!template_url_service)
    return true;
  const TemplateURL* default_search_provider =
      template_url_service->GetDefaultSearchProvider();
  return default_search_provider &&
         default_search_provider->url_ref().HasGoogleBaseURLs(
             template_url_service->search_terms_data());
}
}  //  namespace

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
  StartFetchTask(browser_context, std::move(callback));
}

void TileBackgroundTask::OnFullBrowserLoaded(
    content::BrowserContext* browser_context) {
  StartFetchTask(browser_context, std::move(callback_));
}

bool TileBackgroundTask::OnStopTask(const TaskParameters& task_params) {
  // Don't reschedule.
  return false;
}

void TileBackgroundTask::StartFetchTask(
    content::BrowserContext* browser_context,
    TaskFinishedCallback callback) {
  auto* profile_key =
      Profile::FromBrowserContext(browser_context)->GetProfileKey();
  TemplateURLService* template_url_service =
      TemplateURLServiceFactory::GetForProfile(
          Profile::FromBrowserContext(browser_context));
  auto* tile_service =
      TileServiceFactory::GetInstance()->GetForKey(profile_key);
  DCHECK(tile_service);
  if (IsQueryTilesEnabled() && IsSearchEngineSupported(template_url_service)) {
    tile_service->StartFetchForTiles(false, std::move(callback));
  } else {
    tile_service->CancelTask();
  }
}

}  // namespace query_tiles
