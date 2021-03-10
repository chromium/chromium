// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MANAGER_MECHANISMS_PAGE_FREEZER_H_
#define CHROME_BROWSER_PERFORMANCE_MANAGER_MECHANISMS_PAGE_FREEZER_H_

#include "base/callback_forward.h"

namespace performance_manager {

class PageNode;

namespace mechanism {

// Mechanism to freeze a PageNode.
class PageFreezer {
 public:
  PageFreezer() = default;
  virtual ~PageFreezer() = default;
  PageFreezer(const PageFreezer& other) = delete;
  PageFreezer& operator=(const PageFreezer&) = delete;

  // Attempt to freeze |page_node|. Virtual for testing.
  virtual void MaybeFreezePageNode(const PageNode* page_node);

  // Unfreeze |page_node|. Virtual for testing.
  virtual void UnfreezePageNode(const PageNode* page_node);

  // Attempt to freeze |page_node| and run |reply_cb| to indicate if the freeze
  // operation has been successful.
  virtual void MaybeFreezePageNodeWithReplyForTesting(
      const PageNode* page_node,
      base::OnceCallback<void(bool)> reply_cb);
};

}  // namespace mechanism
}  // namespace performance_manager

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_MECHANISMS_PAGE_FREEZER_H_
