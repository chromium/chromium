// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/tab_strip_context_decorator.h"

#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/contextual_tasks/public/contextual_task_context.h"
#include "content/public/browser/web_contents.h"

namespace contextual_tasks {

TabStripContextDecorator::TabStripContextDecorator(Profile* profile)
    : profile_(profile) {}

TabStripContextDecorator::~TabStripContextDecorator() = default;

std::vector<TabStripContextDecorator::TabInfo>
TabStripContextDecorator::GetOpenTabUrls() {
  std::vector<TabInfo> open_tabs;
  for (const Browser* browser : *BrowserList::GetInstance()) {
    if (browser->profile() != profile_) {
      continue;
    }
    const TabStripModel* tab_strip_model = browser->tab_strip_model();
    for (int i = 0; i < tab_strip_model->count(); ++i) {
      content::WebContents* web_contents = tab_strip_model->GetWebContentsAt(i);
      if (web_contents) {
        open_tabs.push_back(
            {web_contents->GetLastCommittedURL(), web_contents->GetTitle()});
      }
    }
  }
  return open_tabs;
}

void TabStripContextDecorator::DecorateContext(
    std::unique_ptr<ContextualTaskContext> context,
    base::OnceCallback<void(std::unique_ptr<ContextualTaskContext>)>
        context_callback) {
  std::vector<TabInfo> open_tabs = GetOpenTabUrls();
  std::map<GURL, std::u16string> open_urls;
  for (const auto& tab_info : open_tabs) {
    open_urls[tab_info.url] = tab_info.title;
  }

  // TODO(shaktisahu): Dedup the URLs using canonicalization.
  for (auto& attachment : GetMutableUrlAttachments(*context)) {
    auto it = open_urls.find(attachment.GetURL());
    if (it != open_urls.end()) {
      auto& tab_strip_data =
          GetMutableUrlAttachmentDecoratorData(attachment).tab_strip_data;
      tab_strip_data.is_open_in_tab_strip = true;
      tab_strip_data.title = it->second;
    }
  }

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(context_callback), std::move(context)));
}

}  // namespace contextual_tasks
