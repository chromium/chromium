// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/data_sharing/data_sharing_navigation_utils.h"

#include <algorithm>

namespace data_sharing {
namespace {
constexpr base::TimeDelta kUserGestureExpirationDuration =
    base::Milliseconds(1000);
}  // namespace

// static
DataSharingNavigationUtils* DataSharingNavigationUtils::GetInstance() {
  static base::NoDestructor<DataSharingNavigationUtils> instance;
  return instance.get();
}

DataSharingNavigationUtils::DataSharingNavigationUtils() = default;

DataSharingNavigationUtils::~DataSharingNavigationUtils() = default;

void DataSharingNavigationUtils::UpdateLastUserInteractionTime(
    content::WebContents* web_contents) {
  base::Time now = clock_->Now();
  std::erase_if(recent_interractions_map_, [now](const auto& pair) {
    return (now - pair.second) > kUserGestureExpirationDuration;
  });
  uintptr_t key = reinterpret_cast<uintptr_t>(web_contents);
  recent_interractions_map_[key] = now;
}

bool DataSharingNavigationUtils::IsLastUserInteractionExpired(
    content::WebContents* web_contents) {
  uintptr_t key = reinterpret_cast<uintptr_t>(web_contents);
  if (recent_interractions_map_.find(key) == recent_interractions_map_.end()) {
    return true;
  }

  return clock_->Now() - recent_interractions_map_[key] >
         kUserGestureExpirationDuration;
}
}  // namespace data_sharing
