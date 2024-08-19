// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_CORAL_UTIL_H_
#define ASH_PUBLIC_CPP_CORAL_UTIL_H_

#include <string>

#include "ash/public/cpp/ash_public_export.h"
#include "url/gurl.h"

namespace ash::coral_util {

// TODO(zxdan) Look into additional metadata.
struct ASH_PUBLIC_EXPORT AppData {
  std::string app_id;
  std::string app_name;
};

// TODO(zxdan) Look into additional metadata.
struct ASH_PUBLIC_EXPORT TabData {
  std::string tab_title;
  // The url or source link of a tab.
  std::string source;
};

class ASH_PUBLIC_EXPORT CoralRequest {
 public:
  enum class RequestType {
    kCacheEmbedding,  // Embed and cache. No response expected.
    kGrouping,        // Embed and group. response expected.
    kMaxValue = kGrouping,
  };

  CoralRequest();
  CoralRequest(const CoralRequest&) = delete;
  CoralRequest& operator=(const CoralRequest&) = delete;
  ~CoralRequest();

  void set_tab_data(std::vector<TabData>&& tab_data) {
    tab_data_ = std::move(tab_data);
  }

 private:
  std::vector<AppData> app_data_;
  std::vector<TabData> tab_data_;
};

// `CoralCluster` holds a title describing the cluster, and a vector
// of 4-10 semantically similar tabs and apps and their score.
// The scores range between -1 and 1 and are the cosine similarity
// between the center of mass of the cluster and the tab/app.
class ASH_PUBLIC_EXPORT CoralCluster {
 public:
  CoralCluster();
  CoralCluster(const CoralCluster&) = delete;
  CoralCluster& operator=(const CoralCluster&) = delete;
  ~CoralCluster();

 private:
  std::string title_;
  std::vector<std::pair<AppData, float>> scored_app_data_;
  std::vector<std::pair<TabData, float>> scored_tab_data_;
};

// `CoralResponse` contains 0-2 `CoralCluster`s in order of relevance.
class ASH_PUBLIC_EXPORT CoralResponse {
 public:
  CoralResponse();
  CoralResponse(const CoralResponse&) = delete;
  CoralResponse& operator=(const CoralResponse&) = delete;
  ~CoralResponse();

  const std::vector<CoralCluster>& clusters() const { return clusters_; }

 private:
  std::vector<CoralCluster> clusters_;
};

}  // namespace ash::coral_util

#endif  // ASH_PUBLIC_CPP_CORAL_UTIL_H_
