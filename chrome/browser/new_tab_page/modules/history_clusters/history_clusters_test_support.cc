// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/new_tab_page/modules/history_clusters/history_clusters_test_support.h"

#include <string>
#include <vector>

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/new_tab_page/modules/history_clusters/ranking/history_clusters_module_ranking_signals.h"
#include "components/history/core/browser/history_types.h"
#include "components/history_clusters/core/history_clusters_types.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_contents_user_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

MockHistoryClustersTabHelper::MockHistoryClustersTabHelper(
    content::WebContents* web_contents)
    : HistoryClustersTabHelper(web_contents) {}

MockHistoryClustersTabHelper::~MockHistoryClustersTabHelper() = default;

MockHistoryClustersModuleService::MockHistoryClustersModuleService()
    : HistoryClustersModuleService(nullptr, nullptr, nullptr, nullptr) {}

MockHistoryClustersModuleService::~MockHistoryClustersModuleService() = default;

MockHistoryService::MockHistoryService() = default;

MockHistoryService::~MockHistoryService() = default;

history::ClusterVisit SampleVisitForURL(
    history::VisitID id,
    GURL url,
    bool has_url_keyed_image,
    const std::vector<std::string>& related_searches) {
  history::VisitRow visit_row;
  visit_row.visit_id = id;
  visit_row.visit_time = base::Time::Now();
  visit_row.is_known_to_sync = true;
  auto content_annotations = history::VisitContentAnnotations();
  content_annotations.has_url_keyed_image = has_url_keyed_image;
  content_annotations.related_searches = related_searches;
  history::AnnotatedVisit annotated_visit;
  annotated_visit.visit_row = std::move(visit_row);
  annotated_visit.content_annotations = std::move(content_annotations);
  std::string kSampleUrl = url.spec();
  history::ClusterVisit sample_visit;
  sample_visit.url_for_display = base::UTF8ToUTF16(kSampleUrl);
  sample_visit.normalized_url = url;
  sample_visit.annotated_visit = std::move(annotated_visit);
  sample_visit.score = 1.0f;
  return sample_visit;
}

history::Cluster SampleCluster(
    int id,
    int srp_visits,
    int non_srp_visits,
    const std::vector<std::string> related_searches) {
  history::ClusterVisit sample_srp_visit =
      SampleVisitForURL(1, GURL(kSampleSearchUrl), false);
  history::ClusterVisit sample_non_srp_visit =
      SampleVisitForURL(2, GURL(kSampleNonSearchUrl), true, related_searches);

  std::vector<history::ClusterVisit> visits;
  visits.insert(visits.end(), srp_visits, sample_srp_visit);
  visits.insert(visits.end(), non_srp_visits, sample_non_srp_visit);

  std::string kSampleLabel = "LabelOne";
  return history::Cluster(id, std::move(visits),
                          {{u"apples", history::ClusterKeywordData()},
                           {u"Red Oranges", history::ClusterKeywordData()}},
                          /*should_show_on_prominent_ui_surfaces=*/true,
                          /*label=*/
                          l10n_util::GetStringFUTF16(
                              IDS_HISTORY_CLUSTERS_CLUSTER_LABEL_SEARCH_TERMS,
                              base::UTF8ToUTF16(kSampleLabel)));
}

history::Cluster SampleCluster(
    int srp_visits,
    int non_srp_visits,
    const std::vector<std::string> related_searches) {
  return SampleCluster(1, srp_visits, non_srp_visits, related_searches);
}
