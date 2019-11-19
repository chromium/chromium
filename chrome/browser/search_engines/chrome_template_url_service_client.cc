// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search_engines/chrome_template_url_service_client.h"

#include "components/search_engines/template_url_service.h"

ChromeTemplateURLServiceClient::ChromeTemplateURLServiceClient(
    history::HistoryService* history_service)
    : owner_(NULL),
      history_service_(history_service) {
  // TODO(sky): bug 1166191. The keywords should be moved into the history
  // db, which will mean we no longer need this notification and the history
  // backend can handle automatically adding the search terms as the user
  // navigates.
  if (history_service_)
    history_service_observer_.Add(history_service_);
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
  history_service_observer_.RemoveAll();
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
    const base::string16& term) {
  if (history_service_)
    history_service_->SetKeywordSearchTermsForURL(url, id, term);
}

void ChromeTemplateURLServiceClient::AddKeywordGeneratedVisit(const GURL& url) {
  if (history_service_)
    history_service_->AddPage(url, base::Time::Now(), NULL, 0, GURL(),
                              history::RedirectList(),
                              ui::PAGE_TRANSITION_KEYWORD_GENERATED,
                              history::SOURCE_BROWSED, false);
}

void ChromeTemplateURLServiceClient::OnURLVisited(
    history::HistoryService* history_service,
    ui::PageTransition transition,
    const history::URLRow& row,
    const history::RedirectList& redirects,
    base::Time visit_time) {
  DCHECK_EQ(history_service_, history_service);
  if (!owner_)
    return;

  TemplateURLService::URLVisitedDetails visited_details;
  visited_details.url = row.url();
  visited_details.is_keyword_transition =
      ui::PageTransitionCoreTypeIs(transition, ui::PAGE_TRANSITION_KEYWORD);
  owner_->OnHistoryURLVisited(visited_details);
}
