// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEW_TAB_PAGE_MODULES_TEST_SUPPORT_H_
#define CHROME_BROWSER_NEW_TAB_PAGE_MODULES_TEST_SUPPORT_H_

#include <string>
#include <vector>

#include "base/test/scoped_feature_list.h"
#include "components/history/core/browser/history_service.h"
#include "components/search/ntp_features.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace ntp {

class MockHistoryService : public history::HistoryService {
 public:
  MockHistoryService();

  MockHistoryService(const MockHistoryService&) = delete;

  MockHistoryService& operator=(const MockHistoryService&) = delete;

  ~MockHistoryService() override;

  MOCK_METHOD1(ClearCachedDataForContextID,
               void(history::ContextID context_id));

  MOCK_CONST_METHOD5(
      GetAnnotatedVisits,
      base::CancelableTaskTracker::TaskId(
          const history::QueryOptions& options,
          bool compute_redirect_chain_start_properties,
          bool get_unclustered_visits_only,
          history::HistoryService::GetAnnotatedVisitsCallback callback,
          base::CancelableTaskTracker* tracker));

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

  MOCK_METHOD3(
      GetMostRecentVisitForEachURL,
      base::CancelableTaskTracker::TaskId(
          const std::vector<GURL>& urls,
          base::OnceCallback<void(std::map<GURL, history::VisitRow>)> callback,
          base::CancelableTaskTracker* tracker));

  MOCK_CONST_METHOD4(ToAnnotatedVisits,
                     base::CancelableTaskTracker::TaskId(
                         const history::VisitVector& visit_rows,
                         bool compute_redirect_chain_start_properties,
                         ToAnnotatedVisitsCallback callback,
                         base::CancelableTaskTracker* tracker));
};

static constexpr size_t kNumModuleFeatures = 5;
extern const std::vector<base::test::FeatureRef>& kAllModuleFeatures;

std::vector<base::test::FeatureRef> ComputeDisabledFeaturesList(
    const std::vector<base::test::FeatureRef>& features,
    const std::vector<base::test::FeatureRef>& enabled_features);

}  // namespace ntp

#endif  // CHROME_BROWSER_NEW_TAB_PAGE_MODULES_TEST_SUPPORT_H_
