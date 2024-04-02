// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/history_embeddings/history_embeddings_tab_helper.h"

#include "base/check.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/weak_ptr.h"
#include "base/notreached.h"
#include "base/time/time.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/history_embeddings/history_embeddings_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/history_types.h"
#include "components/history/core/browser/url_row.h"
#include "components/history_embeddings/history_embeddings_features.h"
#include "components/history_embeddings/history_embeddings_service.h"
#include "components/keyed_service/core/service_access_type.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "url/gurl.h"

HistoryEmbeddingsTabHelper::HistoryEmbeddingsTabHelper(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      content::WebContentsUserData<HistoryEmbeddingsTabHelper>(*web_contents) {}

HistoryEmbeddingsTabHelper::~HistoryEmbeddingsTabHelper() = default;

void HistoryEmbeddingsTabHelper::OnUpdatedHistoryForNavigation(
    content::NavigationHandle* navigation_handle,
    base::Time visit_time,
    const GURL& url) {
  if (!base::FeatureList::IsEnabled(history_embeddings::kHistoryEmbeddings)) {
    return;
  }

  history::HistoryService* history_service = GetHistoryService();
  if (navigation_handle->IsInPrimaryMainFrame() && history_service) {
    history_service->GetMostRecentVisitsForGurl(
        url, 1,
        base::BindOnce(
            [](base::WeakPtr<history_embeddings::HistoryEmbeddingsService>
                   history_embeddings_service,
               content::RenderFrameHost* render_frame_host,
               base::Time visit_time, history::QueryURLResult result) {
              // `visits` can be empty for navigations that don't result in a
              // visit being added to the DB, e.g. navigations to
              // "chrome://" URLs.
              if (!history_embeddings_service || !result.success ||
                  result.visits.empty()) {
                return;
              }
              const history::URLRow& url_row = result.row;
              const history::VisitRow& latest_visit = result.visits[0];
              CHECK(url_row.id());
              CHECK(latest_visit.visit_id);
              CHECK_EQ(url_row.id(), latest_visit.url_id);
              // Make sure the visit we got actually corresponds to the
              // navigation by comparing the visit_times.
              if (latest_visit.visit_time != visit_time) {
                return;
              }
              // Make sure the latest visit (the first one in the array) is
              // a local one. That should almost always be the case, since
              // this gets called just after a local visit happened, but in
              // some rare cases it might not be, e.g. if another device
              // sent us a visit "from the future". If this turns out to be
              // a problem, consider implementing a
              // GetMostRecent*Local*VisitsForURL().
              if (!latest_visit.originator_cache_guid.empty()) {
                return;
              }
              history_embeddings_service->RetrievePassages(latest_visit,
                                                           *render_frame_host);
            },
            GetHistoryEmbeddingsService()->AsWeakPtr(),
            navigation_handle->GetRenderFrameHost(), visit_time),
        &task_tracker_);
  }
}

history_embeddings::HistoryEmbeddingsService*
HistoryEmbeddingsTabHelper::GetHistoryEmbeddingsService() {
  CHECK(web_contents());
  return HistoryEmbeddingsServiceFactory::GetForProfile(
      Profile::FromBrowserContext(web_contents()->GetBrowserContext()));
}

history::HistoryService* HistoryEmbeddingsTabHelper::GetHistoryService() {
  CHECK(web_contents());
  Profile* profile =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext());
  return HistoryServiceFactory::GetForProfileIfExists(
      profile, ServiceAccessType::IMPLICIT_ACCESS);
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(HistoryEmbeddingsTabHelper);
