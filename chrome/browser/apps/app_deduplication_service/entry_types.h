// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_DEDUPLICATION_SERVICE_ENTRY_TYPES_H_
#define CHROME_BROWSER_APPS_APP_DEDUPLICATION_SERVICE_ENTRY_TYPES_H_

#include <string>

#include "components/services/app_service/public/cpp/app_types.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class GURL;

namespace apps::deduplication {

enum class EntryStatus {
  // This entry is not an app entry (could be website, etc.).
  kNonApp = 0,
  kInstalledApp = 1,
  kNotInstalledApp = 2
};

enum class EntryType { kApp, kWebPage };

// Deduplication entry, each entry represents an app or a web page that could be
// identified as duplicates with each other.
struct Entry {
  Entry() = default;

  // Constructor for apps.
  Entry(std::string app_id, AppType app_type);

  // Constructor for web pages.
  explicit Entry(const GURL& url);

  Entry(const Entry&) = default;
  Entry& operator=(const Entry&) = default;
  Entry(Entry&&) = default;
  Entry& operator=(Entry&&) = default;

  bool operator==(const Entry& other) const;
  bool operator<(const Entry& other) const;

  EntryStatus entry_status;

  EntryType entry_type;

  // The identifier id. If it is website, the id is the url.spec().
  std::string id;

  // The app type for EntryType::kApp.
  absl::optional<AppType> app_type;
};

// For logging and debugging purposes.
std::ostream& operator<<(std::ostream& out, const Entry& entry);

}  // namespace apps::deduplication

#endif  // CHROME_BROWSER_APPS_APP_DEDUPLICATION_SERVICE_ENTRY_TYPES_H_
