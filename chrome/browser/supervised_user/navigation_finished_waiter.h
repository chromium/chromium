// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SUPERVISED_USER_NAVIGATION_FINISHED_WAITER_H_
#define CHROME_BROWSER_SUPERVISED_USER_NAVIGATION_FINISHED_WAITER_H_

#include "base/run_loop.h"
#include "content/public/browser/web_contents_observer.h"
#include "url/gurl.h"

namespace content {
class NavigationHandle;
class WebContents;
}  // namespace content

// Helper class to wait for a particular navigation in a particular render
// frame in tests.
class NavigationFinishedWaiter : public content::WebContentsObserver {
 public:
  NavigationFinishedWaiter(content::WebContents* web_contents,
                           int frame_id,
                           const GURL& url);
  NavigationFinishedWaiter(content::WebContents* web_contents, const GURL& url);
  NavigationFinishedWaiter(const NavigationFinishedWaiter&) = delete;
  NavigationFinishedWaiter& operator=(const NavigationFinishedWaiter&) = delete;

  ~NavigationFinishedWaiter() override = default;

  void Wait();

  // content::WebContentsObserver:
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;

 private:
  int frame_id_;
  GURL url_;
  bool did_finish_ = false;
  base::RunLoop run_loop_{base::RunLoop::Type::kNestableTasksAllowed};
};

#endif  // CHROME_BROWSER_SUPERVISED_USER_NAVIGATION_FINISHED_WAITER_H_
