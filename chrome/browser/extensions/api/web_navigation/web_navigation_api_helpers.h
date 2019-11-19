// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_WEB_NAVIGATION_WEB_NAVIGATION_API_HELPERS_H_
#define CHROME_BROWSER_EXTENSIONS_API_WEB_NAVIGATION_WEB_NAVIGATION_API_HELPERS_H_

#include <memory>
#include <string>

#include "extensions/browser/extension_event_histogram_value.h"
#include "ui/base/page_transition_types.h"

namespace content {
class BrowserContext;
class NavigationHandle;
class RenderFrameHost;
class WebContents;
}

class GURL;

namespace extensions {

struct Event;

namespace web_navigation_api_helpers {

// Returns the frame ID as it will be passed to the extension:
// 0 if the navigation happens in the main frame, or the frame routing ID
// otherwise.
int GetFrameId(content::RenderFrameHost* frame_host);

// Create and dispatch the various events of the webNavigation API.
std::unique_ptr<Event> CreateOnBeforeNavigateEvent(
    content::NavigationHandle* navigation_handle);

void DispatchOnCommitted(events::HistogramValue histogram_value,
                         const std::string& event_name,
                         content::NavigationHandle* navigation_handle);

void DispatchOnDOMContentLoaded(content::WebContents* web_contents,
                                content::RenderFrameHost* frame_host,
                                const GURL& url);

void DispatchOnCompleted(content::WebContents* web_contents,
                         content::RenderFrameHost* frame_host,
                         const GURL& url);

void DispatchOnCreatedNavigationTarget(
    int source_tab_id,
    int source_render_process_id,
    int source_extension_frame_id,
    content::BrowserContext* browser_context,
    content::WebContents* target_web_contents,
    const GURL& target_url);

void DispatchOnErrorOccurred(content::WebContents* web_contents,
                             content::RenderFrameHost* frame_host,
                             const GURL& url,
                             int error_code);
void DispatchOnErrorOccurred(content::NavigationHandle* navigation_handle);

void DispatchOnTabReplaced(
    content::WebContents* old_web_contents,
    content::BrowserContext* browser_context,
    content::WebContents* new_web_contents);

}  // namespace web_navigation_api_helpers

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_WEB_NAVIGATION_WEB_NAVIGATION_API_HELPERS_H_
