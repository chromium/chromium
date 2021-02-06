// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SEARCH_ENGINES_CHROME_TEMPLATE_URL_SERVICE_CLIENT_H_
#define CHROME_BROWSER_SEARCH_ENGINES_CHROME_TEMPLATE_URL_SERVICE_CLIENT_H_

#include "base/macros.h"
#include "base/scoped_observer.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/history_service_observer.h"
#include "components/search_engines/template_url_service_client.h"

// ChromeTemplateURLServiceClient provides keyword related history
// functionality for TemplateURLService.
class ChromeTemplateURLServiceClient : public TemplateURLServiceClient,
                                       public history::HistoryServiceObserver {
 public:
  explicit ChromeTemplateURLServiceClient(
      history::HistoryService* history_service);
  ~ChromeTemplateURLServiceClient() override;

  // TemplateURLServiceClient:
  void Shutdown() override;
  void SetOwner(TemplateURLService* owner) override;
  void DeleteAllSearchTermsForKeyword(history::KeywordID keyword_Id) override;
  void SetKeywordSearchTermsForURL(const GURL& url,
                                   TemplateURLID id,
                                   const base::string16& term) override;
  void AddKeywordGeneratedVisit(const GURL& url) override;

  // history::HistoryServiceObserver:
  void OnURLVisited(history::HistoryService* history_service,
                    ui::PageTransition transition,
                    const history::URLRow& row,
                    const history::RedirectList& redirects,
                    base::Time visit_time) override;

 private:
  TemplateURLService* owner_;
  ScopedObserver<history::HistoryService, history::HistoryServiceObserver>
      history_service_observer_{this};
  history::HistoryService* history_service_;

  DISALLOW_COPY_AND_ASSIGN(ChromeTemplateURLServiceClient);
};

#endif  // CHROME_BROWSER_SEARCH_ENGINES_CHROME_TEMPLATE_URL_SERVICE_CLIENT_H_
