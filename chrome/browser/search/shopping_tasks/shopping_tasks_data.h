// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SEARCH_SHOPPING_TASKS_SHOPPING_TASKS_DATA_H_
#define CHROME_BROWSER_SEARCH_SHOPPING_TASKS_SHOPPING_TASKS_DATA_H_

#include <string>

#include "url/gurl.h"

// Represents a single shopping task.
struct ShoppingTasksData {
  struct Product {
    Product();
    Product(const Product&);
    ~Product();

    std::string name;
    GURL image_url;
    std::string price;
    std::string info;
    GURL target_url;
  };

  struct RelatedSearch {
    RelatedSearch();
    RelatedSearch(const RelatedSearch&);
    ~RelatedSearch();

    std::string text;
    GURL target_url;
  };

  ShoppingTasksData();
  ShoppingTasksData(const ShoppingTasksData&);
  ~ShoppingTasksData();

  std::string title;
  std::vector<Product> products;
  std::vector<RelatedSearch> related_searches;
};

#endif  // CHROME_BROWSER_SEARCH_SHOPPING_TASKS_SHOPPING_TASKS_DATA_H_
