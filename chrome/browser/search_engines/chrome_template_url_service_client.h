// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SEARCH_ENGINES_CHROME_TEMPLATE_URL_SERVICE_CLIENT_H_
#define CHROME_BROWSER_SEARCH_ENGINES_CHROME_TEMPLATE_URL_SERVICE_CLIENT_H_

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
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

  ChromeTemplateURLServiceClient(const ChromeTemplateURLServiceClient&) =
      delete;
  ChromeTemplateURLServiceClient& operator=(
      const ChromeTemplateURLServiceClient&) = delete;

  ~ChromeTemplateURLServiceClient() override;

  // TemplateURLServiceClient:
  void Shutdown() override;
  void SetOwner(TemplateURLService* owner) override;
  void DeleteAllSearchTermsForKeyword(history::KeywordID keyword_Id) override;
  void SetKeywordSearchTermsForURL(const GURL& url,
                                   TemplateURLID id,
                                   const std::u16string& term) override;
  void AddKeywordGeneratedVisit(const GURL& url) override;

  // history::HistoryServiceObserver:
  void OnURLVisited(history::HistoryService* history_service,
                    const history::URLRow& url_row,
                    const history::VisitRow& new_visit) override;

 private:
  raw_ptr<TemplateURLService> owner_;
  base::ScopedObservation<history::HistoryService,
                          history::HistoryServiceObserver>
      history_service_observation_{this};
  raw_ptr<history::HistoryService> history_service_;
};

#endif  // CHROME_BROWSER_SEARCH_ENGINES_CHROME_TEMPLATE_URL_SERVICE_CLIENT_H_
