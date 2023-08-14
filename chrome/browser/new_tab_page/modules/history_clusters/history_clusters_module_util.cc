// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/new_tab_page/modules/history_clusters/history_clusters_module_util.h"

#include "base/ranges/algorithm.h"
#include "base/strings/utf_string_conversions.h"
#include "components/history_clusters/core/history_clusters_util.h"
#include "components/search/ntp_features.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/url_util.h"

namespace {

int GetMinVisitsToShow(int min_required_visits) {
  static int min_visits = base::GetFieldTrialParamByFeatureAsInt(
      ntp_features::kNtpHistoryClustersModuleMinimumVisitsRequired,
      ntp_features::kNtpHistoryClustersModuleMinimumVisitsRequiredParam,
      min_required_visits);
  if (min_visits < 0) {
    return min_required_visits;
  }
  return min_visits;
}

history::ClusterVisit GenerateSampleVisit(
    history::VisitID visit_id,
    const std::string& page_title,
    const GURL& url,
    bool has_url_keyed_image,
    const base::Time visit_time = base::Time::Now()) {
  history::URLRow url_row = history::URLRow(url);
  url_row.set_title(base::UTF8ToUTF16(page_title));
  history::VisitRow visit_row;
  visit_row.visit_id = visit_id;
  visit_row.visit_time = visit_time;
  visit_row.is_known_to_sync = true;
  auto content_annotations = history::VisitContentAnnotations();
  content_annotations.has_url_keyed_image = has_url_keyed_image;
  history::AnnotatedVisit annotated_visit;
  annotated_visit.url_row = std::move(url_row);
  annotated_visit.visit_row = std::move(visit_row);
  annotated_visit.content_annotations = std::move(content_annotations);
  history::ClusterVisit sample_visit;
  sample_visit.normalized_url = url;
  sample_visit.url_for_display =
      history_clusters::ComputeURLForDisplay(url, false);
  sample_visit.annotated_visit = std::move(annotated_visit);

  return sample_visit;
}

}  // namespace

bool CompareClustersUsingHeuristic(
    const base::flat_set<std::string>& category_boostlist,
    const history::Cluster& c1,
    const history::Cluster& c2) {
  if (c1.visits.empty()) {
    return false;
  }
  if (c2.visits.empty()) {
    return true;
  }

  // Boost categories if provided.
  if (!category_boostlist.empty()) {
    bool c1_has_visit_in_categories =
        history_clusters::IsClusterInCategories(c1, category_boostlist);
    bool c2_has_visit_in_categories =
        history_clusters::IsClusterInCategories(c2, category_boostlist);

    if (c1_has_visit_in_categories ^ c2_has_visit_in_categories) {
      return c1_has_visit_in_categories;
    }
  }

  // Otherwise, fall back to reverse chronological.
  base::Time c1_time = c1.visits.front().annotated_visit.visit_row.visit_time;
  base::Time c2_time = c2.visits.front().annotated_visit.visit_row.visit_time;

  // Use c1 > c2 to get more recent clusters BEFORE older clusters.
  return c1_time > c2_time;
}

void SortClustersUsingHeuristic(
    const base::flat_set<std::string>& category_boostlist,
    std::vector<history::Cluster>& clusters) {
  base::ranges::stable_sort(clusters, [&](const auto& c1, const auto& c2) {
    return CompareClustersUsingHeuristic(category_boostlist, c1, c2);
  });
}

history_clusters::QueryClustersFilterParams CreateFilterParamsFromFeatureFlags(
    int min_required_visits,
    int min_required_related_searches) {
  history_clusters::QueryClustersFilterParams filter_params;
  filter_params.min_visits = GetMinVisitsToShow(min_required_visits);
  filter_params.min_visits_with_images = GetMinImagesToShow();
  filter_params.categories_allowlist = GetCategories(
      ntp_features::kNtpHistoryClustersModuleCategoriesAllowlistParam);
  filter_params.categories_blocklist = GetCategories(
      ntp_features::kNtpHistoryClustersModuleCategoriesBlocklistParam);
  filter_params.is_search_initiated = true;
  filter_params.has_related_searches = min_required_related_searches > 0;
  filter_params.is_shown_on_prominent_ui_surfaces = true;
  filter_params.filter_done_clusters = true;
  filter_params.filter_hidden_visits = true;
  filter_params.include_synced_visits = base::FeatureList::IsEnabled(
      ntp_features::kNtpHistoryClustersModuleIncludeSyncedVisits);
  filter_params.group_clusters_by_content = base::FeatureList::IsEnabled(
      ntp_features::kNtpHistoryClustersModuleEnableContentClustering);

  return filter_params;
}

base::flat_set<std::string> GetCategories(const char* feature_param) {
  std::string categories_string = base::GetFieldTrialParamValueByFeature(
      ntp_features::kNtpHistoryClustersModuleCategories, feature_param);
  if (categories_string.empty()) {
    return {};
  }

  auto categories = base::SplitString(categories_string, ",",
                                      base::WhitespaceHandling::TRIM_WHITESPACE,
                                      base::SplitResult::SPLIT_WANT_NONEMPTY);

  return categories.empty() ? base::flat_set<std::string>()
                            : base::flat_set<std::string>(categories.begin(),
                                                          categories.end());
}

int GetMinImagesToShow() {
  static int min_images_to_show = base::GetFieldTrialParamByFeatureAsInt(
      ntp_features::kNtpHistoryClustersModuleMinimumImagesRequired,
      ntp_features::kNtpHistoryClustersModuleMinimumImagesRequiredParam, 1);
  return min_images_to_show;
}

size_t GetMaxClusters() {
  static int max_clusters = base::GetFieldTrialParamByFeatureAsInt(
      ntp_features::kNtpHistoryClustersModuleMaxClusters,
      ntp_features::kNtpHistoryClustersModuleMaxClustersParam, 5);
  if (max_clusters < 0) {
    return 5;
  }
  return static_cast<size_t>(max_clusters);
}

history::Cluster GenerateSampleCluster(int64_t cluster_id,
                                       int num_visits,
                                       int num_images) {
  const base::Time current_time = base::Time::Now();
  const std::vector<std::tuple<std::string, GURL, base::Time>>
      kSampleUrlVisitData = {
          {"Pixel 7 Pro - The all-pro Google phone.",
           GURL("https://store.google.com/product/pixel_7?hl=en-US"),
           current_time - base::Minutes(1)},
          {"Pixel Buds Pro - How premium sounds.",
           GURL("https://store.google.com/product/pixel_buds_pro?hl=en-US"),
           current_time - base::Hours(1)},
          {"Pixel Watch - Help by Google. Health by Fitbit.",
           GURL("https://store.google.com/product/google_pixel_watch?hl=en-US"),
           current_time - base::Hours(4)},
          {"Next Door Bells - Know who's knocking.",
           GURL("https://store.google.com/product/nest_doorbell?hl=en-US"),
           current_time - base::Hours(8)}};

  std::vector<history::ClusterVisit> sample_visits;
  for (int i = 0; i < num_visits; i++) {
    const std::tuple<std::string, GURL, base::Time> kSampleData =
        kSampleUrlVisitData.at(i % kSampleUrlVisitData.size());
    sample_visits.push_back(GenerateSampleVisit(
        i, std::get<0>(kSampleData), std::get<1>(kSampleData), (i < num_images),
        std::get<2>(kSampleData)));
  }

  std::string kSampleSearchQuery = "google store products";
  url::RawCanonOutputT<char> encoded_query;
  url::EncodeURIComponent(kSampleSearchQuery.c_str(),
                          kSampleSearchQuery.length(), &encoded_query);
  sample_visits.insert(
      sample_visits.begin(),
      GenerateSampleVisit(
          0, kSampleSearchQuery + " - Google Search",
          GURL("https://www.google.com/search?q=" +
               std::string(encoded_query.data(), encoded_query.length())),
          false));

  return history::Cluster(
      cluster_id, sample_visits, {},
      /*should_show_on_prominent_ui_surfaces=*/true,
      /*label=*/
      l10n_util::GetStringFUTF16(
          IDS_HISTORY_CLUSTERS_CLUSTER_LABEL_SEARCH_TERMS,
          base::UTF8ToUTF16(kSampleSearchQuery)),
      /*raw_label=*/base::UTF8ToUTF16(kSampleSearchQuery), {},
      {"new google products", "google devices", "google stuff"}, 0);
}
