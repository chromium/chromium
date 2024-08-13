// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_CORAL_UTIL_H_
#define ASH_PUBLIC_CPP_CORAL_UTIL_H_

#include <string>

#include "ash/public/cpp/ash_public_export.h"
#include "url/gurl.h"

namespace ash::coral_util {

struct ASH_PUBLIC_EXPORT AppData {
  std::string app_id;
  std::string app_name;
};

// TODO(yulunwu) Look into additional metadata
struct ASH_PUBLIC_EXPORT TabData {
  std::string tab_title;
  GURL gurl;
};

class ASH_PUBLIC_EXPORT CoralRequest {
 public:
  enum class RequestType {
    kCacheEmbedding,  // Embed and cache. No response expected.
    kGrouping,        // Embed and group. response expected.
    kMaxValue = kGrouping,
  };

  CoralRequest() = default;
  CoralRequest(const CoralRequest&) = delete;
  CoralRequest& operator=(const CoralRequest&) = delete;
  ~CoralRequest() = default;

 private:
  std::vector<AppData> app_data_;
  std::vector<TabData> tab_data_;
};

}  // namespace ash::coral_util

#endif  // ASH_PUBLIC_CPP_CORAL_UTIL_H_
