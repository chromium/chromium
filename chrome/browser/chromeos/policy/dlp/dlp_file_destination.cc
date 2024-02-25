// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/dlp/dlp_file_destination.h"

namespace policy {

DlpFileDestination::DlpFileDestination() = default;
DlpFileDestination::DlpFileDestination(const GURL& url) : url_(url) {
  CHECK(url_->is_valid());
}
DlpFileDestination::DlpFileDestination(const data_controls::Component component)
    : component_(component) {}

DlpFileDestination::DlpFileDestination(const DlpFileDestination&) = default;
DlpFileDestination& DlpFileDestination::operator=(const DlpFileDestination&) =
    default;
DlpFileDestination::DlpFileDestination(DlpFileDestination&&) = default;
DlpFileDestination& DlpFileDestination::operator=(DlpFileDestination&&) =
    default;
bool DlpFileDestination::operator==(const DlpFileDestination& other) const {
  return component_ == other.component_ && url_ == other.url_;
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
  if (url_.has_value() && other.url_.has_value()) {
    return url_.value() < other.url_.value();
  }
  if (url_.has_value()) {
    return true;
  }
  if (other.url_.has_value()) {
    return false;
  }
  return false;
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

std::optional<GURL> DlpFileDestination::url() const {
  return url_;
}

std::optional<data_controls::Component> DlpFileDestination::component() const {
  return component_;
}

bool DlpFileDestination::IsFileSystem() const {
  return !url_.has_value();
}

bool DlpFileDestination::IsMyFiles() const {
  return !url_.has_value() && !component_.has_value();
}

}  // namespace policy
