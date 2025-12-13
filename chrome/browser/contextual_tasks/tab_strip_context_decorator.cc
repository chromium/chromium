// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/tab_strip_context_decorator.h"

#include <map>
#include <set>

#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/contextual_tasks/public/contextual_task_context.h"
#include "components/url_deduplication/url_deduplication_helper.h"
#include "components/visited_url_ranking/public/url_visit_util.h"
#include "content/public/browser/web_contents.h"

namespace contextual_tasks {

TabStripContextDecorator::TabStripContextDecorator(Profile* profile)
    : profile_(profile) {}

TabStripContextDecorator::~TabStripContextDecorator() = default;

std::vector<TabStripContextDecorator::TabInfo>
TabStripContextDecorator::GetOpenTabUrls() {
  std::vector<TabInfo> open_tabs;
  ForEachCurrentBrowserWindowInterfaceOrderedByActivation(
      [profile = profile_, &open_tabs](BrowserWindowInterface* browser) {
        if (browser->GetProfile() != profile) {
          return true;
        }
        const TabStripModel* const tab_strip_model =
            browser->GetTabStripModel();
        for (int i = 0; i < tab_strip_model->count(); ++i) {
          content::WebContents* web_contents =
              tab_strip_model->GetWebContentsAt(i);
          if (web_contents) {
            open_tabs.push_back({web_contents->GetLastCommittedURL(),
                                 web_contents->GetTitle()});
          }
        }
        return true;
      });
  return open_tabs;
}

void TabStripContextDecorator::DecorateContext(
    std::unique_ptr<ContextualTaskContext> context,
    ContextDecorationParams* params,
    base::OnceCallback<void(std::unique_ptr<ContextualTaskContext>)>
        context_callback) {
  // This method performs a two-pass matching process to determine if a URL
  // from the ContextualTaskContext is currently open in the tab strip.
  // First, it attempts an exact URL match. If no exact match is found,
  // it then attempts a match against a canonical (deduplicated) version of the
  // URL, ignoring titles for the deduplication process.

  auto deduplication_helper =
      visited_url_ranking::CreateDefaultURLDeduplicationHelper();

  std::vector<TabInfo> open_tabs = GetOpenTabUrls();
  std::map<GURL, std::u16string> exact_url_open_tabs;
  std::map<visited_url_ranking::URLMergeKey, std::u16string>
      deduplicated_url_open_tabs;
  for (const auto& tab_info : open_tabs) {
    exact_url_open_tabs[tab_info.url] = tab_info.title;
    deduplicated_url_open_tabs.emplace(
        visited_url_ranking::ComputeURLMergeKey(tab_info.url, std::u16string(),
                                                deduplication_helper.get()),
        tab_info.title);
  }

  for (auto& attachment : GetMutableUrlAttachments(*context)) {
    auto& tab_strip_data =
        GetMutableUrlAttachmentDecoratorData(attachment).tab_strip_data;

    // First, check if the exact URL is open in the tab strip.
    auto it = exact_url_open_tabs.find(attachment.GetURL());
    if (it != exact_url_open_tabs.end()) {
      tab_strip_data.is_open_in_tab_strip = true;
      tab_strip_data.title = it->second;
      continue;
    }

    // If no exact URL match was found, attempt to match against a
    // deduplicated, canonical version of the URL. Exclude title from the
    // deduplication algorithm.
    visited_url_ranking::URLMergeKey attachment_merge_key =
        visited_url_ranking::ComputeURLMergeKey(
            attachment.GetURL(), std::u16string(), deduplication_helper.get());

    auto it_dedup = deduplicated_url_open_tabs.find(attachment_merge_key);
    if (it_dedup != deduplicated_url_open_tabs.end()) {
      tab_strip_data.is_open_in_tab_strip = true;
      tab_strip_data.title = it_dedup->second;
    }
  }

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(context_callback), std::move(context)));
}

}  // namespace contextual_tasks
