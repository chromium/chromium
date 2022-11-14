// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <ostream>
#include <utility>

#include "chrome/browser/apps/app_deduplication_service/entry_types.h"
#include "url/gurl.h"

namespace apps::deduplication {

EntryId::EntryId(std::string app_id, AppType app_type)
    : entry_type(EntryType::kApp), id(std::move(app_id)), app_type(app_type) {}
EntryId::EntryId(const GURL& url)
    : entry_type(EntryType::kWebPage), id(url.spec()) {}
EntryId::EntryId(std::string phone_hub_app_package_name)
    : entry_type(EntryType::kPhoneHubApp),
      id(std::move(phone_hub_app_package_name)) {}

bool EntryId::operator==(const EntryId& other) const {
  return entry_type == other.entry_type && id == other.id &&
         (entry_type != EntryType::kApp || app_type == other.app_type);
}

bool EntryId::operator<(const EntryId& other) const {
  if (entry_type != other.entry_type)
    return entry_type < other.entry_type;

  if (entry_type == EntryType::kApp && app_type != other.app_type)
    return app_type < other.app_type;

  return id < other.id;
}

Entry::Entry(EntryId entry_id) : entry_id(std::move(entry_id)) {}

bool Entry::operator==(const Entry& other) const {
  return entry_id == other.entry_id;
}

bool Entry::operator<(const Entry& other) const {
  return entry_id < other.entry_id;
}

std::ostream& operator<<(std::ostream& out, const EntryId& entry_id) {
  out << "EntryType: "
      << static_cast<std::underlying_type<EntryType>::type>(entry_id.entry_type)
      << std::endl;
  out << "Id: " << entry_id.id << std::endl;
  if (entry_id.app_type.has_value()) {
    out << "AppType: "
        << static_cast<std::underlying_type<AppType>::type>(
               entry_id.app_type.value())
        << std::endl;
  }
  return out;
}

}  // namespace apps::deduplication
