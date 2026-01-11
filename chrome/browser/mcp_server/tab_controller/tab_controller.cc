// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/mcp_server/tab_controller/tab_controller.h"

#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_client.h"
#include "url/gurl.h"

namespace mcp_server {

TabController::TabController() {
  LOG(INFO) << "TabController initialized";
}

TabController::~TabController() = default;

base::Value::Dict TabController::BuildTabInfo(
    content::WebContents* web_contents) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  base::Value::Dict tab_info;

  // Use WebContents pointer value as unique ID
  int session_id = reinterpret_cast<intptr_t>(web_contents);
  tab_info.Set("id", session_id);

  // Get URL
  GURL url = web_contents->GetURL();
  tab_info.Set("url", url.spec());

  // Get title
  std::u16string title = web_contents->GetTitle();
  tab_info.Set("title", base::UTF16ToUTF8(title));

  // Get loading status
  tab_info.Set("loading", web_contents->IsLoading());

  // Get visibility (is tab active in its window)
  Browser* browser = nullptr;
  for (Browser* b : *BrowserList::GetInstance()) {
    TabStripModel* tab_strip = b->tab_strip_model();
    if (tab_strip->GetActiveWebContents() == web_contents) {
      tab_info.Set("active", true);
      browser = b;
      break;
    }
  }
  if (!browser) {
    tab_info.Set("active", false);
  }

  return tab_info;
}

content::WebContents* TabController::FindWebContentsBySessionId(
    int session_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Iterate through all browser windows
  for (Browser* browser : *BrowserList::GetInstance()) {
    TabStripModel* tab_strip = browser->tab_strip_model();

    // Check each tab in this browser window
    for (int i = 0; i < tab_strip->count(); ++i) {
      content::WebContents* web_contents = tab_strip->GetWebContentsAt(i);
      int current_id = reinterpret_cast<intptr_t>(web_contents);

      if (current_id == session_id) {
        return web_contents;
      }
    }
  }

  return nullptr;
}

std::string TabController::ListTabs() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  base::Value::List tabs_array;

  // Iterate through all browser windows
  for (Browser* browser : *BrowserList::GetInstance()) {
    TabStripModel* tab_strip = browser->tab_strip_model();

    // Add each tab to the result
    for (int i = 0; i < tab_strip->count(); ++i) {
      content::WebContents* web_contents = tab_strip->GetWebContentsAt(i);
      tabs_array.Append(BuildTabInfo(web_contents));
    }
  }

  // Convert to JSON string
  base::Value::Dict result;
  result.Set("tabs", std::move(tabs_array));
  result.Set("count", static_cast<int>(tabs_array.size()));

  std::string json_string;
  base::JSONWriter::Write(result, &json_string);
  return json_string;
}

std::string TabController::CreateTab(const std::string& url) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Get the last active browser window
  Browser* browser = chrome::FindLastActive();
  if (!browser) {
    LOG(ERROR) << "No active browser window found";
    base::Value::Dict error;
    error.Set("error", "No browser window available");
    std::string json_string;
    base::JSONWriter::Write(error, &json_string);
    return json_string;
  }

  // Parse URL
  GURL gurl(url);
  if (!gurl.is_valid()) {
    LOG(ERROR) << "Invalid URL: " << url;
    base::Value::Dict error;
    error.Set("error", "Invalid URL");
    std::string json_string;
    base::JSONWriter::Write(error, &json_string);
    return json_string;
  }

  // Create WebContents for new tab
  content::WebContents::CreateParams create_params(browser->profile());
  std::unique_ptr<content::WebContents> web_contents =
      content::WebContents::Create(create_params);

  // Navigate to URL
  content::NavigationController::LoadURLParams load_params(gurl);
  web_contents->GetController().LoadURLWithParams(load_params);

  // Store pointer before moving
  content::WebContents* web_contents_ptr = web_contents.get();

  // Add tab to browser (in foreground)
  TabStripModel* tab_strip = browser->tab_strip_model();
  tab_strip->AppendWebContents(std::move(web_contents), true);

  // Build response with new tab info
  base::Value::Dict tab_info = BuildTabInfo(web_contents_ptr);

  std::string json_string;
  base::JSONWriter::Write(tab_info, &json_string);
  return json_string;
}

bool TabController::CloseTab(int session_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  content::WebContents* web_contents = FindWebContentsBySessionId(session_id);
  if (!web_contents) {
    LOG(WARNING) << "Tab not found with session ID: " << session_id;
    return false;
  }

  // Find which browser and index contains this tab
  for (Browser* browser : *BrowserList::GetInstance()) {
    TabStripModel* tab_strip = browser->tab_strip_model();
    int index = tab_strip->GetIndexOfWebContents(web_contents);

    if (index != TabStripModel::kNoTab) {
      // Close the tab
      tab_strip->CloseWebContentsAt(index, TabCloseTypes::CLOSE_NONE);
      LOG(INFO) << "Closed tab with session ID: " << session_id;
      return true;
    }
  }

  return false;
}

bool TabController::ActivateTab(int session_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  content::WebContents* web_contents = FindWebContentsBySessionId(session_id);
  if (!web_contents) {
    LOG(WARNING) << "Tab not found with session ID: " << session_id;
    return false;
  }

  // Find which browser contains this tab
  for (Browser* browser : *BrowserList::GetInstance()) {
    TabStripModel* tab_strip = browser->tab_strip_model();
    int index = tab_strip->GetIndexOfWebContents(web_contents);

    if (index != TabStripModel::kNoTab) {
      // Activate the tab
      tab_strip->ActivateTabAt(index);
      LOG(INFO) << "Activated tab with session ID: " << session_id;
      return true;
    }
  }

  return false;
}

std::string TabController::GetTabState(int session_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  content::WebContents* web_contents = FindWebContentsBySessionId(session_id);
  if (!web_contents) {
    LOG(WARNING) << "Tab not found with session ID: " << session_id;
    base::Value::Dict error;
    error.Set("error", "Tab not found");
    std::string json_string;
    base::JSONWriter::Write(error, &json_string);
    return json_string;
  }

  // Build tab info
  base::Value::Dict tab_info = BuildTabInfo(web_contents);

  std::string json_string;
  base::JSONWriter::Write(tab_info, &json_string);
  return json_string;
}

}  // namespace mcp_server
