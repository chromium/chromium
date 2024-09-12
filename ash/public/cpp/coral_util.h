// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_CORAL_UTIL_H_
#define ASH_PUBLIC_CPP_CORAL_UTIL_H_

#include <string>
#include <variant>
#include <vector>

#include "ash/public/cpp/ash_public_export.h"

namespace ash::coral_util {

// TODO(zxdan) Look into additional metadata.
struct ASH_PUBLIC_EXPORT AppData {
  std::string app_id;
  std::string app_name;

  // Enable the auto comparator.
  auto operator<=>(const AppData&) const = default;
};

// TODO(zxdan) Look into additional metadata.
struct ASH_PUBLIC_EXPORT TabData {
  std::string tab_title;
  // The url or source link of a tab.
  std::string source;

  // Enable the auto comparator.
  auto operator<=>(const TabData&) const = default;
};

using ContentItem = std::variant<AppData, TabData>;

// Gets the unique identifier for `item`.
std::string ASH_PUBLIC_EXPORT GetIdentifier(const ContentItem& item);

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

  void set_content(std::vector<ContentItem>&& content) {
    content_ = std::move(content);
  }

  const std::vector<ContentItem>& content() const { return content_; }

 private:
  // Tab/app content with arbitrary ordering.
  std::vector<ContentItem> content_;
};

struct ASH_PUBLIC_EXPORT AppKey {
  std::string app_id;
};

struct ASH_PUBLIC_EXPORT TabKey {
  // The url or source link of a tab.
  std::string source;
};

using ContentKey = std::variant<AppKey, TabKey>;

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

  const std::u16string& title() const { return title_; }
  void set_title(const std::u16string& title) { title_ = title; }

 private:
  std::u16string title_;
  // Tab/app content keys sorted by relevance to the cluster.
  std::vector<ContentKey> content_keys_;
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
