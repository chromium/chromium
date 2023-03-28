// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/dlp/dlp_file_destination.h"

namespace policy {

DlpFileDestination::DlpFileDestination() = default;
DlpFileDestination::DlpFileDestination(const std::string& url)
    : url_or_path(url) {}
DlpFileDestination::DlpFileDestination(
    const DlpRulesManager::Component component)
    : component(component) {}

DlpFileDestination::DlpFileDestination(const DlpFileDestination&) = default;
DlpFileDestination& DlpFileDestination::operator=(const DlpFileDestination&) =
    default;
DlpFileDestination::DlpFileDestination(DlpFileDestination&&) = default;
DlpFileDestination& DlpFileDestination::operator=(DlpFileDestination&&) =
    default;
bool DlpFileDestination::operator==(const DlpFileDestination& other) const {
  return component == other.component && url_or_path == other.url_or_path;
}
bool DlpFileDestination::operator!=(const DlpFileDestination& other) const {
  return !(*this == other);
}
bool DlpFileDestination::operator<(const DlpFileDestination& other) const {
  if (component.has_value() && other.component.has_value()) {
    return static_cast<int>(component.value()) <
           static_cast<int>(other.component.value());
  }
  if (component.has_value()) {
    return true;
  }
  if (other.component.has_value()) {
    return false;
  }
  DCHECK(url_or_path.has_value() && other.url_or_path.has_value());
  return url_or_path.value() < other.url_or_path.value();
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

}  // namespace policy
