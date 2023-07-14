// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEW_TAB_PAGE_MODULES_HISTORY_CLUSTERS_HISTORY_CLUSTERS_TEST_SUPPORT_H_
#define CHROME_BROWSER_NEW_TAB_PAGE_MODULES_HISTORY_CLUSTERS_HISTORY_CLUSTERS_TEST_SUPPORT_H_

#include <string>
#include <vector>

#include "base/supports_user_data.h"
#include "chrome/browser/new_tab_page/modules/history_clusters/history_clusters_module_service.h"
#include "chrome/browser/ui/side_panel/history_clusters/history_clusters_tab_helper.h"
#include "components/history/core/browser/history_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

constexpr char kSampleNonSearchUrl[] = "https://www.foo.com/";
constexpr char kSampleSearchUrl[] = "https://www.google.com/search?q=foo";

namespace content {
class WebContents;
}  // namespace content

namespace history {
struct Cluster;
struct ClusterVisit;
}  // namespace history

namespace history_clusters {
struct QueryClustersFilterParams;
}  // namespace history_clusters

class GURL;

class HistoryClustersModuleRankingSignals;

class MockHistoryClustersTabHelper
    : public side_panel::HistoryClustersTabHelper {
 public:
  MockHistoryClustersTabHelper(const MockHistoryClustersTabHelper&) = delete;
  MockHistoryClustersTabHelper& operator=(const MockHistoryClustersTabHelper&) =
      delete;
  ~MockHistoryClustersTabHelper() override;

  static MockHistoryClustersTabHelper* CreateForWebContents(
      content::WebContents* contents) {
    DCHECK(contents);
    DCHECK(!contents->GetUserData(UserDataKey()));
    contents->SetUserData(
        UserDataKey(),
        base::WrapUnique(new MockHistoryClustersTabHelper(contents)));
    return static_cast<MockHistoryClustersTabHelper*>(
        contents->GetUserData(UserDataKey()));
  }

  MOCK_METHOD(void, ShowJourneysSidePanel, (const std::string&), (override));

 private:
  explicit MockHistoryClustersTabHelper(content::WebContents* web_contents);
};

class MockHistoryClustersModuleService : public HistoryClustersModuleService {
 public:
  MockHistoryClustersModuleService();
  MockHistoryClustersModuleService(const MockHistoryClustersModuleService&) =
      delete;
  MockHistoryClustersModuleService& operator=(
      const MockHistoryClustersModuleService&) = delete;
  ~MockHistoryClustersModuleService() override;

  MOCK_METHOD3(
      GetClusters,
      void(const history_clusters::QueryClustersFilterParams filter_params,
           size_t min_required_related_searches,
           base::OnceCallback<void(
               std::vector<history::Cluster>,
               base::flat_map<int64_t, HistoryClustersModuleRankingSignals>)>
               callback));
};

class MockHistoryService : public history::HistoryService {
 public:
  MockHistoryService();
  MockHistoryService(const MockHistoryService&) = delete;
  MockHistoryService& operator=(const MockHistoryService&) = delete;
  ~MockHistoryService() override;

  MOCK_METHOD1(ClearCachedDataForContextID,
               void(history::ContextID context_id));
  MOCK_METHOD3(HideVisits,
               base::CancelableTaskTracker::TaskId(
                   const std::vector<history::VisitID>& visit_ids,
                   base::OnceClosure callback,
                   base::CancelableTaskTracker* tracker));
  MOCK_METHOD4(
      UpdateVisitsInteractionState,
      base::CancelableTaskTracker::TaskId(
          const std::vector<history::VisitID>& visit_ids,
          const history::ClusterVisit::InteractionState interaction_state,
          base::OnceClosure callback,
          base::CancelableTaskTracker* tracker));
};

history::ClusterVisit SampleVisitForURL(
    history::VisitID id,
    GURL url,
    bool has_url_keyed_image = true,
    const std::vector<std::string>& related_searches = {});

history::Cluster SampleCluster(int id,
                               int srp_visits,
                               int non_srp_visits,
                               const std::vector<std::string> related_searches =
                                   {"fruits", "red fruits", "healthy fruits"});

history::Cluster SampleCluster(int srp_visits,
                               int non_srp_visits,
                               const std::vector<std::string> related_searches =
                                   {"fruits", "red fruits", "healthy fruits"});

#endif  // CHROME_BROWSER_NEW_TAB_PAGE_MODULES_HISTORY_CLUSTERS_HISTORY_CLUSTERS_TEST_SUPPORT_H_
