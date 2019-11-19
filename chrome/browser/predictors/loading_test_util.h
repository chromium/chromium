// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROME_BROWSER_PREDICTORS_LOADING_TEST_UTIL_H_
#define CHROME_BROWSER_PREDICTORS_LOADING_TEST_UTIL_H_

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "chrome/browser/predictors/loading_data_collector.h"
#include "chrome/browser/predictors/resource_prefetch_predictor.h"
#include "chrome/browser/predictors/resource_prefetch_predictor_tables.h"
#include "components/sessions/core/session_id.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace predictors {

// Does nothing, controls which URLs are prefetchable.
class MockResourcePrefetchPredictor : public ResourcePrefetchPredictor {
 public:
  MockResourcePrefetchPredictor(const LoadingPredictorConfig& config,
                                Profile* profile);
  ~MockResourcePrefetchPredictor() override;

  void RecordPageRequestSummary(
      std::unique_ptr<PageRequestSummary> summary) override {
    RecordPageRequestSummaryProxy(summary.get());
  }

  MOCK_CONST_METHOD2(PredictPreconnectOrigins,
                     bool(const GURL&, PreconnectPrediction*));
  MOCK_METHOD1(RecordPageRequestSummaryProxy, void(PageRequestSummary*));
};

// |include_scheme| and |include_port| can be set to false to simulate legacy
// data, which doesn't have new fields.
void InitializeRedirectStat(RedirectStat* redirect,
                            const GURL& url,
                            int number_of_hits,
                            int number_of_misses,
                            int consecutive_misses,
                            bool include_scheme = true,
                            bool include_port = true);

void InitializeOriginStat(OriginStat* origin_stat,
                          const std::string& origin,
                          int number_of_hits,
                          int number_of_misses,
                          int consecutive_misses,
                          double average_position,
                          bool always_access_network,
                          bool accessed_network);

RedirectData CreateRedirectData(const std::string& primary_key,
                                uint64_t last_visit_time = 0);
OriginData CreateOriginData(const std::string& host,
                            uint64_t last_visit_time = 0);

NavigationID CreateNavigationID(SessionID tab_id,
                                const std::string& main_frame_url);

PageRequestSummary CreatePageRequestSummary(
    const std::string& main_frame_url,
    const std::string& initial_url,
    const std::vector<content::mojom::ResourceLoadInfoPtr>&
        resource_load_infos);

content::mojom::ResourceLoadInfoPtr CreateResourceLoadInfo(
    const std::string& url,
    content::ResourceType resource_type = content::ResourceType::kMainFrame,
    bool always_access_network = false);

content::mojom::ResourceLoadInfoPtr CreateLowPriorityResourceLoadInfo(
    const std::string& url,
    content::ResourceType resource_type = content::ResourceType::kMainFrame);

content::mojom::ResourceLoadInfoPtr CreateResourceLoadInfoWithRedirects(
    const std::vector<std::string>& redirect_chain,
    content::ResourceType resource_type = content::ResourceType::kMainFrame);

PreconnectPrediction CreatePreconnectPrediction(
    std::string host,
    bool is_redirected,
    const std::vector<PreconnectRequest>& requests);

void PopulateTestConfig(LoadingPredictorConfig* config, bool small_db = true);

// For printing failures nicely.
std::ostream& operator<<(std::ostream& stream, const RedirectData& data);
std::ostream& operator<<(std::ostream& stream, const RedirectStat& redirect);
std::ostream& operator<<(std::ostream& stream,
                         const PageRequestSummary& summary);
std::ostream& operator<<(std::ostream& stream, const NavigationID& id);

std::ostream& operator<<(std::ostream& os, const OriginData& data);
std::ostream& operator<<(std::ostream& os, const OriginStat& redirect);
std::ostream& operator<<(std::ostream& os, const PreconnectRequest& request);
std::ostream& operator<<(std::ostream& os,
                         const PreconnectPrediction& prediction);

bool operator==(const RedirectData& lhs, const RedirectData& rhs);
bool operator==(const RedirectStat& lhs, const RedirectStat& rhs);
bool operator==(const PageRequestSummary& lhs, const PageRequestSummary& rhs);
bool operator==(const OriginRequestSummary& lhs,
                const OriginRequestSummary& rhs);
bool operator==(const OriginData& lhs, const OriginData& rhs);
bool operator==(const OriginStat& lhs, const OriginStat& rhs);
bool operator==(const PreconnectRequest& lhs, const PreconnectRequest& rhs);
bool operator==(const PreconnectPrediction& lhs,
                const PreconnectPrediction& rhs);

}  // namespace predictors

namespace content {
namespace mojom {

std::ostream& operator<<(std::ostream& os, const CommonNetworkInfo& info);
std::ostream& operator<<(std::ostream& os, const ResourceLoadInfo& info);

bool operator==(const CommonNetworkInfo& lhs, const CommonNetworkInfo& rhs);
bool operator==(const ResourceLoadInfo& lhs, const ResourceLoadInfo& rhs);

}  // namespace mojom
}  // namespace content

#endif  // CHROME_BROWSER_PREDICTORS_LOADING_TEST_UTIL_H_
