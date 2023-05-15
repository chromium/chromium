// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/explore_sites/catalog.h"

#include <sstream>

#include "base/logging.h"
#include "base/values.h"

namespace explore_sites {

// static
std::unique_ptr<NTPCatalog> NTPCatalog::create(const base::Value& json) {
  if (!json.is_dict())
    return nullptr;

  const base::Value::List* categories = json.GetDict().FindList("categories");
  if (!categories)
    return nullptr;

  std::vector<NTPCatalog::Category> catalog_categories;
  for (const auto& category : *categories) {
    auto* category_dict = category.GetIfDict();
    if (!category_dict) {
      return nullptr;
    }
    const std::string* id = category_dict->FindString("id");
    const std::string* title = category_dict->FindString("title");
    const std::string* icon_url_str = category_dict->FindString("icon_url");

    if (!id || !title || !icon_url_str)
      continue;

    GURL icon_url(*icon_url_str);
    if (icon_url.is_empty())
      continue;

    catalog_categories.push_back({*id, *title, icon_url});
  }

  auto catalog = std::make_unique<NTPCatalog>(catalog_categories);
  DVLOG(1) << "Catalog parsed: " << catalog->ToString();

  return catalog;
}

NTPCatalog::~NTPCatalog() = default;
NTPCatalog::NTPCatalog(const std::vector<Category>& category_list) {
  categories = category_list;
}

std::string NTPCatalog::ToString() {
  std::ostringstream ss;
  ss << " NTPCatalog {\n";
  for (auto& category : categories) {
    ss << "  category " << category.id << " {\n"
       << "    title: " << category.title << "\n"
       << "    icon_url: " << category.icon_url.spec() << "\n";
  }
  ss << "}\n";
  return ss.str();
}

bool operator==(const NTPCatalog::Category& a, const NTPCatalog::Category& b) {
  return a.id == b.id && a.title == b.title && a.icon_url == b.icon_url;
}

bool operator==(const NTPCatalog& a, const NTPCatalog& b) {
  return a.categories == b.categories;
}

}  // namespace explore_sites
