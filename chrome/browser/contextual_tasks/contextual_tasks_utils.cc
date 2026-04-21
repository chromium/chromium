// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/contextual_tasks_utils.h"

#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/timer/elapsed_timer.h"
#include "build/build_config.h"
#include "chrome/browser/contextual_tasks/contextual_tasks.mojom.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui.h"
#include "chrome/browser/contextual_tasks/site_exclusion_detail.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/search.h"
#include "components/contextual_search/contextual_search_metrics_recorder.h"
#include "components/contextual_tasks/public/prefs.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"

namespace contextual_tasks {

std::unique_ptr<
    contextual_search::ContextualSearchContextController::ConfigParams>
CreateQueryControllerConfigParams() {
  auto config_params = std::make_unique<
      contextual_search::ContextualSearchContextController::ConfigParams>();
  config_params->send_lns_surface = true;
  config_params->enable_viewport_images = true;
  config_params->attach_page_title_and_url_to_suggest_requests = false;
  return config_params;
}

void ShowAndRecordErrorPage(mojo::Remote<contextual_tasks::mojom::Page>& page,
                            contextual_search::ContextualSearchSource source) {
  if (page) {
    page->ShowErrorPage();
  }
  RecordErrorPageShown(source);
}

void RecordErrorPageShown(contextual_search::ContextualSearchSource source) {
  base::UmaHistogramEnumeration(
      base::StrCat({"ContextualSearch.ErrorPageShown", ".",
                    contextual_search::ContextualSearchMetricsRecorder::
                        ContextualSearchSourceToString(source)}),
      contextual_search::ContextualSearchErrorPage::kPageContextNotEligible);
}

void RecordInnerFrameContentsHttpResponseCode(int http_status_code,
                                              bool is_zero_state) {
  base::UmaHistogramSparse(
      "ContextualTasks.InnerFrameContents.HttpResponseCode", http_status_code);
  if (!is_zero_state) {
    base::UmaHistogramSparse(
        "ContextualTasks.InnerFrameContents.HttpResponseCode."
        "ExcludeZeroStateLoads",
        http_status_code);
  }
}

ContextualTasksUIInterface* GetWebUiInterface(
    content::WebContents* web_contents) {
  if (!web_contents || !web_contents->GetWebUI() ||
      !web_contents->GetWebUI()->GetController()) {
    return nullptr;
  }

  return web_contents->GetWebUI()->GetController()->GetAs<ContextualTasksUI>();
}

bool IsValidUrlForSuggestedTab(const GURL& url,
                               Profile* profile,
                               SiteExclusionDetail& site_exclusion_detail) {
  if (!url.is_valid() || url.IsAboutBlank()) {
    return false;
  }

  if (!(url.SchemeIsHTTPOrHTTPS() || url.SchemeIsFile())) {
    return false;
  }

  if (search::IsNTPOrRelatedURL(url, profile)) {
    return false;
  }

  CHECK(profile);
  base::ElapsedTimer timer;
  // Since site exclusions are expected to be rare, it is generally faster
  // and simpler to use list-like key processing instead of allocating with
  // `url.GetHost()` and then having to check the dictionary for various
  // domain substrings. Using `DomainIs` means sites like `en.wikipedia.org`
  // will be filtered if the site exclusions contain `wikipedia.org`.
  site_exclusion_detail.tabs_checked++;
  for (auto it : ReadSiteExclusionsFromPrefs(profile->GetPrefs())) {
    site_exclusion_detail.exclusions_checked++;
    if (url.DomainIs(it.first)) {
      site_exclusion_detail.tabs_filtered++;
      site_exclusion_detail.duration += timer.Elapsed();
      return false;
    }
  }
  site_exclusion_detail.duration += timer.Elapsed();

  return true;
}

}  // namespace contextual_tasks
