// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search_engines/chrome_template_url_service_client.h"

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

ChromeTemplateURLServiceClient::~ChromeTemplateURLServiceClient() {
}

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
        /*did_replace_entry=*/false);
}

void ChromeTemplateURLServiceClient::OnURLVisited(
    history::HistoryService* history_service,
    const history::URLRow& url_row,
    const history::VisitRow& new_visit) {
  DCHECK_EQ(history_service_, history_service);
  if (!owner_)
    return;

  TemplateURLService::URLVisitedDetails visited_details;
  visited_details.url = url_row.url();
  visited_details.is_keyword_transition = ui::PageTransitionCoreTypeIs(
      new_visit.transition, ui::PAGE_TRANSITION_KEYWORD);
  owner_->OnHistoryURLVisited(visited_details);
}
