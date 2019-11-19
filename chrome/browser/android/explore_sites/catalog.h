// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_EXPLORE_SITES_CATALOG_H_
#define CHROME_BROWSER_ANDROID_EXPLORE_SITES_CATALOG_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "url/gurl.h"

namespace base {
class Value;
}

namespace explore_sites {

// A representation of the Explore Sites catalog on the NTP.
class NTPCatalog {
 public:
  // Categories are mapped to individual tiles on the NTP.
  struct Category {
    // The category ID, used as a section identifier when opening the full
    // explore sites catalog.
    std::string id;

    // The textual name of the category.
    std::string title;

    // The icon image URL.
    GURL icon_url;
  };

  // The NTPCatalog does not take ownership of |json|.
  static std::unique_ptr<NTPCatalog> create(const base::Value& json);

  explicit NTPCatalog(const std::vector<Category>& category_list);
  ~NTPCatalog();

  std::vector<Category> categories;

 private:
  std::string ToString();
  DISALLOW_COPY_AND_ASSIGN(NTPCatalog);
};

bool operator==(const NTPCatalog::Category& a, const NTPCatalog::Category& b);
bool operator==(const NTPCatalog& a, const NTPCatalog& b);

}  // namespace explore_sites

#endif  // CHROME_BROWSER_ANDROID_EXPLORE_SITES_CATALOG_H_
