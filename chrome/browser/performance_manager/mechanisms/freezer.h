// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MANAGER_MECHANISMS_FREEZER_H_
#define CHROME_BROWSER_PERFORMANCE_MANAGER_MECHANISMS_FREEZER_H_

namespace performance_manager {

class PageNode;

namespace mechanism {

// Mechanism to freeze a PageNode.
class Freezer {
 public:
  Freezer() = default;
  virtual ~Freezer() = default;
  Freezer(const Freezer& other) = delete;
  Freezer& operator=(const Freezer&) = delete;

  // Attempt to freeze |page_node|. Virtual for testing.
  virtual void MaybeFreezePageNode(const PageNode* page_node);

  // Unfreeze |page_node|. Virtual for testing.
  virtual void UnfreezePageNode(const PageNode* page_node);
};

}  // namespace mechanism
}  // namespace performance_manager

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_MECHANISMS_FREEZER_H_
