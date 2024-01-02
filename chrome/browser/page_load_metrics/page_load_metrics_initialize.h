// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PAGE_LOAD_METRICS_PAGE_LOAD_METRICS_INITIALIZE_H_
#define CHROME_BROWSER_PAGE_LOAD_METRICS_PAGE_LOAD_METRICS_INITIALIZE_H_

#include <string>

namespace content {
class WebContents;
}

namespace chrome {

void InitializePageLoadMetricsForWebContents(
    content::WebContents* web_contents);

void InitializePageLoadMetricsForNonTabWebUI(content::WebContents* web_contents,
                                             const std::string& webui_name);

}  // namespace chrome

#endif  // CHROME_BROWSER_PAGE_LOAD_METRICS_PAGE_LOAD_METRICS_INITIALIZE_H_
