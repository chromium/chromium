// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/dlp/dlp_file_destination.h"

namespace policy {

DlpFileDestination::DlpFileDestination(const std::string& url)
    : url_or_path_(url) {}
DlpFileDestination::DlpFileDestination(
    const DlpRulesManager::Component component)
    : component_(component) {}

DlpFileDestination::DlpFileDestination(const DlpFileDestination&) = default;
DlpFileDestination& DlpFileDestination::operator=(const DlpFileDestination&) =
    default;
DlpFileDestination::DlpFileDestination(DlpFileDestination&&) = default;
DlpFileDestination& DlpFileDestination::operator=(DlpFileDestination&&) =
    default;
bool DlpFileDestination::operator==(const DlpFileDestination& other) const {
  return component_ == other.component_ && url_or_path_ == other.url_or_path_;
}
bool DlpFileDestination::operator!=(const DlpFileDestination& other) const {
  return !(*this == other);
}
bool DlpFileDestination::operator<(const DlpFileDestination& other) const {
  if (component_.has_value() && other.component_.has_value()) {
    return static_cast<int>(component_.value()) <
           static_cast<int>(other.component_.value());
  }
  if (component_.has_value()) {
    return true;
  }
  if (other.component_.has_value()) {
    return false;
  }
  DCHECK(url_or_path_.has_value() && other.url_or_path_.has_value());
  return url_or_path_.value() < other.url_or_path_.value();
}
bool DlpFileDestination::operator<=(const DlpFileDestination& other) const {
  return *this == other || *this < other;
}
bool DlpFileDestination::operator>(const DlpFileDestination& other) const {
  return !(*this <= other);
}
bool DlpFileDestination::operator>=(const DlpFileDestination& other) const {
  return !(*this < other);
}

DlpFileDestination::~DlpFileDestination() = default;

absl::optional<std::string> DlpFileDestination::url_or_path() const {
  return url_or_path_;
}

absl::optional<DlpRulesManager::Component> DlpFileDestination::component()
    const {
  return component_;
}

}  // namespace policy
