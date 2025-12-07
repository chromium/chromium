// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MANAGER_MECHANISMS_PAGE_LOADER_H_
#define CHROME_BROWSER_PERFORMANCE_MANAGER_MECHANISMS_PAGE_LOADER_H_

#include <vector>

namespace performance_manager {

class PageNode;

namespace mechanism {

// Mechanism that allows loading of the Page associated with a PageNode.
class PageLoader {
 public:
  PageLoader() = default;
  virtual ~PageLoader() = default;
  PageLoader(const PageLoader& other) = delete;
  PageLoader& operator=(const PageLoader&) = delete;

  // Starts loading |page_node| if not already loaded.
  virtual void LoadPageNode(const PageNode* page_node);

  // Returns a vector of PageNodes that should be loaded when |page_node| is set
  // to be loaded. Defaults to just returning |page_node| but in a split view it
  // will return all nodes in the split.
  virtual std::vector<const PageNode*> GetPageNodesToLoad(
      const PageNode* page_node);
};

}  // namespace mechanism

}  // namespace performance_manager

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_MECHANISMS_PAGE_LOADER_H_
