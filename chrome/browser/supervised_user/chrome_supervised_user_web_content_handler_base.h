// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SUPERVISED_USER_CHROME_SUPERVISED_USER_WEB_CONTENT_HANDLER_BASE_H_
#define CHROME_BROWSER_SUPERVISED_USER_CHROME_SUPERVISED_USER_WEB_CONTENT_HANDLER_BASE_H_

#include "base/memory/raw_ptr.h"
#include "components/supervised_user/core/browser/web_content_handler.h"
#include "content/public/browser/frame_tree_node_id.h"

namespace content {
class WebContents;
}  // namespace content

// Implements common Web Content Handler functionality that can be shared
// accross non-IOS platforms, but cannot belong in the base class due to
// prohibited dependencies in components.
class ChromeSupervisedUserWebContentHandlerBase
    : public supervised_user::WebContentHandler {
 public:
  ChromeSupervisedUserWebContentHandlerBase(
      const ChromeSupervisedUserWebContentHandlerBase&) = delete;
  ChromeSupervisedUserWebContentHandlerBase& operator=(
      const ChromeSupervisedUserWebContentHandlerBase&) = delete;
  ~ChromeSupervisedUserWebContentHandlerBase() override;

  // supervised_user::WebContentHandler implementation:
  bool IsMainFrame() const override;
  void CleanUpInfoBarOnMainFrame() override;
  int64_t GetInterstitialNavigationId() const override;
  void GoBack() override;

 protected:
  ChromeSupervisedUserWebContentHandlerBase(content::WebContents* web_contents,
                                            content::FrameTreeNodeId frame_id,
                                            int64_t interstitial_navigation_id);
  raw_ptr<content::WebContents> web_contents_;

 private:
  // Tries to navigate to the previous page (if one exists) and returns
  // if it was successful.
  bool AttemptMoveAwayFromCurrentFrameURL();
  // Notifies the consumers of the intestitial.
  void OnInterstitialDone();

  // The uniquely identifying global id for the frame.
  const content::FrameTreeNodeId frame_id_;
  // The Navigation id of the navigation that last triggered the interstitial.
  int64_t interstitial_navigation_id_;
};

#endif  // CHROME_BROWSER_SUPERVISED_USER_CHROME_SUPERVISED_USER_WEB_CONTENT_HANDLER_BASE_H_
