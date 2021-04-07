// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SUBRESOURCE_FILTER_CHROME_SUBRESOURCE_FILTER_CLIENT_H_
#define CHROME_BROWSER_SUBRESOURCE_FILTER_CHROME_SUBRESOURCE_FILTER_CLIENT_H_

#include <memory>

#include "base/macros.h"
#include "components/subresource_filter/content/browser/subresource_filter_client.h"

namespace content {
class WebContents;
}  // namespace content

namespace subresource_filter {
class ContentSubresourceFilterThrottleManager;
}  // namespace subresource_filter

// Chrome implementation of SubresourceFilterClient. Instances are associated
// with and owned by ContentSubresourceFilterThrottleManager instances.
class ChromeSubresourceFilterClient
    : public subresource_filter::SubresourceFilterClient {
 public:
  explicit ChromeSubresourceFilterClient(content::WebContents* web_contents);
  ~ChromeSubresourceFilterClient() override;

  // Creates a ContentSubresourceFilterThrottleManager and attaches it to
  // |web_contents|, passing it an instance of this client and other
  // embedder-level state.
  static void CreateThrottleManagerWithClientForWebContents(
      content::WebContents* web_contents);

  // SubresourceFilterClient:
  void ShowNotification() override;
  const scoped_refptr<safe_browsing::SafeBrowsingDatabaseManager>
  GetSafeBrowsingDatabaseManager() override;

 private:
  content::WebContents* web_contents_;

  std::unique_ptr<subresource_filter::ContentSubresourceFilterThrottleManager>
      throttle_manager_;

  DISALLOW_COPY_AND_ASSIGN(ChromeSubresourceFilterClient);
};

#endif  // CHROME_BROWSER_SUBRESOURCE_FILTER_CHROME_SUBRESOURCE_FILTER_CLIENT_H_
