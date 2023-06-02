// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <ostream>
#include <utility>

#include "chrome/browser/apps/app_deduplication_service/entry_types.h"
#include "url/gurl.h"

namespace apps::deduplication {

Entry::Entry(std::string app_id, AppType app_type)
    : entry_type(EntryType::kApp), id(std::move(app_id)), app_type(app_type) {}
Entry::Entry(const GURL& url)
    : entry_type(EntryType::kWebPage), id(url.spec()) {}

bool Entry::operator==(const Entry& other) const {
  return entry_type == other.entry_type && id == other.id &&
         (entry_type != EntryType::kApp || app_type == other.app_type);
}

bool Entry::operator<(const Entry& other) const {
  if (entry_type != other.entry_type)
    return entry_type < other.entry_type;

  if (entry_type == EntryType::kApp && app_type != other.app_type)
    return app_type < other.app_type;

  return id < other.id;
}

std::ostream& operator<<(std::ostream& out, const Entry& entry) {
  out << "EntryStatus: "
      << static_cast<std::underlying_type<EntryStatus>::type>(
             entry.entry_status)
      << std::endl;
  out << "EntryType: "
      << static_cast<std::underlying_type<EntryType>::type>(entry.entry_type)
      << std::endl;
  out << "Id: " << entry.id << std::endl;
  if (entry.app_type.has_value()) {
    out << "AppType: "
        << static_cast<std::underlying_type<AppType>::type>(
               entry.app_type.value())
        << std::endl;
  }
  return out;
}

}  // namespace apps::deduplication
