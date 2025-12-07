// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search_engines/chrome_template_url_service_client.h"

#include "base/feature_list.h"
#include "components/history/core/browser/features.h"
#include "components/history/core/browser/history_types.h"
#include "components/search_engines/template_url_service.h"

ChromeTemplateURLServiceClient::ChromeTemplateURLServiceClient(
    history::HistoryService* history_service)
    : owner_(nullptr), history_service_(history_service) {
  // TODO(sky): bug 1166191. The keywords should be moved into the history
  // db, which will mean we no longer need this notification and the history
  // backend can handle automatically adding the search terms as the user
  // navigates.
  if (history_service_)
    history_service_observation_.Observe(history_service_.get());
}

ChromeTemplateURLServiceClient::~ChromeTemplateURLServiceClient() = default;

void ChromeTemplateURLServiceClient::Shutdown() {
  // ChromeTemplateURLServiceClient is owned by TemplateURLService which is a
  // KeyedService with a dependency on HistoryService, thus |history_service_|
  // outlives the ChromeTemplateURLServiceClient.
  //
  // Remove self from |history_service_| observers in the shutdown phase of the
  // two-phases since KeyedService are not supposed to use a dependend service
  // after the Shutdown call.
  history_service_observation_.Reset();
}

void ChromeTemplateURLServiceClient::SetOwner(TemplateURLService* owner) {
  DCHECK(!owner_);
  owner_ = owner;
}

void ChromeTemplateURLServiceClient::DeleteAllSearchTermsForKeyword(
    TemplateURLID id) {
  if (history_service_)
    history_service_->DeleteAllSearchTermsForKeyword(id);
}

void ChromeTemplateURLServiceClient::SetKeywordSearchTermsForURL(
    const GURL& url,
    TemplateURLID id,
    const std::u16string& term) {
  if (history_service_)
    history_service_->SetKeywordSearchTermsForURL(url, id, term);
}

void ChromeTemplateURLServiceClient::AddKeywordGeneratedVisit(const GURL& url) {
  if (history_service_)
    history_service_->AddPage(
        url, base::Time::Now(), /*context_id=*/0, /*nav_entry_id=*/0,
        /*referrer=*/GURL(), history::RedirectList(),
        ui::PAGE_TRANSITION_KEYWORD_GENERATED, history::SOURCE_BROWSED,
        history::VisitResponseCodeCategory::kNot404,
        /*did_replace_entry=*/false);
}

void ChromeTemplateURLServiceClient::OnURLVisited(
    history::HistoryService* history_service,
    const history::VisitedURLInfo& visited_url_info) {
  DCHECK_EQ(history_service_, history_service);
  if (!owner_)
    return;
  // Filter out 404 visits to prevent them from informing search
  // recommendations and impacting user journeys.
  if (visited_url_info.response_code_category ==
      history::VisitResponseCodeCategory::k404) {
    return;
  }

#if !BUILDFLAG(IS_ANDROID)
  // Filter out `SOURCE_ACTOR` visits to prevent them from informing search
  // recommendations and impacting user journeys.
  // TODO(crbug.com/464331451): Add tests to check that `SOURCE ACTOR` visits
  // are dropped.
  if (base::FeatureList::IsEnabled(
          history::kBrowsingHistoryActorIntegrationM2) &&
      visited_url_info.visit_row.source == history::SOURCE_ACTOR) {
    return;
  }
#endif  // !BUILDFLAG(IS_ANDROID)

  TemplateURLService::URLVisitedDetails visited_details;
  visited_details.url = visited_url_info.url_row.url();
  visited_details.is_keyword_transition = ui::PageTransitionCoreTypeIs(
      visited_url_info.visit_row.transition, ui::PAGE_TRANSITION_KEYWORD);
  owner_->OnHistoryURLVisited(visited_details);
}
