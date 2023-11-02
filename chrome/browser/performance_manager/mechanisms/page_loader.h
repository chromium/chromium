// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MANAGER_MECHANISMS_PAGE_LOADER_H_
#define CHROME_BROWSER_PERFORMANCE_MANAGER_MECHANISMS_PAGE_LOADER_H_

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
};

}  // namespace mechanism

}  // namespace performance_manager

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_MECHANISMS_PAGE_LOADER_H_
