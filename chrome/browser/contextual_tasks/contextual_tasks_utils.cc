// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/contextual_tasks_utils.h"

#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "build/build_config.h"
#include "chrome/browser/contextual_tasks/contextual_tasks.mojom.h"
#include "components/contextual_search/contextual_search_metrics_recorder.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/contextual_tasks/contextual_tasks_ui.h"
#endif

namespace contextual_tasks {

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

ContextualTasksUIInterface* GetWebUiInterface(
    content::WebContents* web_contents) {
#if !BUILDFLAG(IS_ANDROID)
  if (!web_contents || !web_contents->GetWebUI()) {
    return nullptr;
  }

  return web_contents->GetWebUI()->GetController()->GetAs<ContextualTasksUI>();
#else
  // TODO(crbug.com/478283549): Provide android implementation.
  return nullptr;
#endif
}

}  // namespace contextual_tasks
