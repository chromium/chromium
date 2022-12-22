// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dips/dips_utils.h"

#include "base/cxx17_backports.h"
#include "base/strings/string_piece.h"
#include "base/time/time.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "url/gurl.h"

bool TimestampRange::Update(base::Time time) {
  bool modified = false;

  if (!first.has_value() || time < first.value()) {
    first = time;
    modified = true;
  }

  if (!last.has_value() || time > last.value()) {
    last = time;
    modified = true;
  }

  return modified;
}

bool TimestampRange::IsNullOrWithin(TimestampRange other) const {
  if (first.has_value()) {
    if (!other.first.has_value() || other.first.value() > first.value()) {
      return false;
    }
  }
  if (last.has_value()) {
    if (!other.last.has_value() || other.last.value() < last.value()) {
      return false;
    }
  }

  return true;
}

std::ostream& operator<<(std::ostream& os, absl::optional<base::Time> time) {
  if (time.has_value()) {
    return os << time.value();
  }
  return os << "NULL";
}

std::ostream& operator<<(std::ostream& os, TimestampRange range) {
  return os << "[" << range.first << ", " << range.last << "]";
}

// CookieAccessType:
base::StringPiece CookieAccessTypeToString(CookieAccessType type) {
  switch (type) {
    case CookieAccessType::kUnknown:
      return "Unknown";
    case CookieAccessType::kNone:
      return "None";
    case CookieAccessType::kRead:
      return "Read";
    case CookieAccessType::kWrite:
      return "Write";
    case CookieAccessType::kReadWrite:
      return "ReadWrite";
  }
}

// DIPSCookieMode:
DIPSCookieMode GetDIPSCookieMode(bool is_otr, bool block_third_party_cookies) {
  if (is_otr) {
    if (block_third_party_cookies) {
      return DIPSCookieMode::kOffTheRecord_Block3PC;
    }
    return DIPSCookieMode::kOffTheRecord;
  }

  if (block_third_party_cookies) {
    return DIPSCookieMode::kBlock3PC;
  }

  return DIPSCookieMode::kStandard;
}

base::StringPiece GetHistogramSuffix(DIPSCookieMode mode) {
  // Any changes here need to be reflected in DIPSCookieMode in
  // tools/metrics/histograms/metadata/others/histograms.xml
  switch (mode) {
    case DIPSCookieMode::kStandard:
      return ".Standard";
    case DIPSCookieMode::kOffTheRecord:
      return ".OffTheRecord";
    case DIPSCookieMode::kBlock3PC:
      return ".Block3PC";
    case DIPSCookieMode::kOffTheRecord_Block3PC:
      return ".OffTheRecord_Block3PC";
  }
  DCHECK(false) << "Invalid DIPSCookieMode";
  return base::StringPiece();
}

const char* DIPSCookieModeToString(DIPSCookieMode mode) {
  switch (mode) {
    case DIPSCookieMode::kStandard:
      return "Standard";
    case DIPSCookieMode::kOffTheRecord:
      return "OffTheRecord";
    case DIPSCookieMode::kBlock3PC:
      return "Block3PC";
    case DIPSCookieMode::kOffTheRecord_Block3PC:
      return "OffTheRecord_Block3PC";
  }
}

std::ostream& operator<<(std::ostream& os, DIPSCookieMode mode) {
  return os << DIPSCookieModeToString(mode);
}

// DIPSRedirectType:
base::StringPiece GetHistogramPiece(DIPSRedirectType type) {
  // Any changes here need to be reflected in
  // tools/metrics/histograms/metadata/privacy/histograms.xml
  switch (type) {
    case DIPSRedirectType::kClient:
      return "Client";
    case DIPSRedirectType::kServer:
      return "Server";
  }
  DCHECK(false) << "Invalid DIPSRedirectType";
  return base::StringPiece();
}

const char* DIPSRedirectTypeToString(DIPSRedirectType type) {
  switch (type) {
    case DIPSRedirectType::kClient:
      return "Client";
    case DIPSRedirectType::kServer:
      return "Server";
  }
}

std::ostream& operator<<(std::ostream& os, DIPSRedirectType type) {
  return os << DIPSRedirectTypeToString(type);
}

int64_t BucketizeBounceDelay(base::TimeDelta delta) {
  return base::clamp(delta.InSeconds(), INT64_C(0), INT64_C(10));
}

std::string GetSiteForDIPS(const GURL& url) {
  const auto domain = net::registry_controlled_domains::GetDomainAndRegistry(
      url, net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
  return domain.empty() ? url.host() : domain;
}
