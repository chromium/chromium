// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_EXPLORE_SITES_EXPLORE_SITES_TYPES_H_
#define CHROME_BROWSER_ANDROID_EXPLORE_SITES_EXPLORE_SITES_TYPES_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/time/time.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "url/gurl.h"

namespace explore_sites {
constexpr int kFaviconsPerCategoryImage = 4;

// The in-memory representation of a site in the ExploreSitesStore.
// Image data is not represented here because it is requested separately from
// the UI layer.
struct ExploreSitesSite {
  ExploreSitesSite(int site_id, int category_id, GURL url, std::string title);
  ExploreSitesSite(ExploreSitesSite&& other);
  virtual ~ExploreSitesSite();

  int site_id;
  int category_id;
  GURL url;
  std::string title;

  DISALLOW_COPY_AND_ASSIGN(ExploreSitesSite);
};

// The in-memory representation of a category in the ExploreSitesStore.
// Image data is not represented here because it is requested separately from
// the UI layer.
struct ExploreSitesCategory {
  // Creates a category.  Sites should be populated separately.
  ExploreSitesCategory(int category_id,
                       std::string version_token,
                       int category_type,
                       std::string label);
  ExploreSitesCategory(ExploreSitesCategory&& other);
  virtual ~ExploreSitesCategory();

  int category_id;
  std::string version_token;
  int category_type;
  std::string label;

  std::vector<ExploreSitesSite> sites;

  DISALLOW_COPY_AND_ASSIGN(ExploreSitesCategory);
};

enum class GetCatalogStatus { kFailed, kNoCatalog, kSuccess };

using CatalogCallback = base::OnceCallback<
    void(GetCatalogStatus, std::unique_ptr<std::vector<ExploreSitesCategory>>)>;
using BooleanCallback = base::OnceCallback<void(bool)>;
using EncodedImageBytes = std::vector<uint8_t>;
using EncodedImageList = std::vector<std::unique_ptr<EncodedImageBytes>>;
using EncodedImageListCallback = base::OnceCallback<void(EncodedImageList)>;
using ImageJobFinishedCallback = base::OnceCallback<void(void)>;

using BitmapCallback = base::OnceCallback<void(std::unique_ptr<SkBitmap>)>;

// Status for sending request to the server.
// Must be kept in sync with ExploreSitesRequestStatus enum in enums.xml.
// This enum should be treated as append-only.
enum class ExploreSitesRequestStatus {
  // Request completed successfully.
  kSuccess = 0,
  // Request failed even after all the retries.
  kFailure = 1,
  // Request failed with error indicating that the request can not be serviced
  // by the server.
  kShouldSuspendBadRequest = 2,
  // The request was blocked by a URL blacklist configured by the domain
  // administrator.
  kShouldSuspendBlockedByAdministrator = 3,
  // kMaxValue should always be the last type.
  kMaxValue = kShouldSuspendBlockedByAdministrator
};

// Must be kept in sync with ExploreSitesCatalogError enum in enums.xml.
// This enum should be treated as append-only.
enum class ExploreSitesCatalogError {
  // Catalog parse from protobuf string failed.
  kParseFailure = 0,
  // Category with a missing title.
  kCategoryMissingTitle = 1,
  // Category with a type enum that this version does not support.
  kCategoryWithUnknownType = 2,
  // Category with no sites present.
  kCategoryWithNoSites = 3,
  // Site with a malformed or empty URL.
  kSiteWithBadUrl = 4,
  // Site with no title.
  kSiteMissingTitle = 5,
  // Site with a missing icon.
  kSiteMissingIcon = 6,
  kMaxValue = kSiteMissingIcon
};
}  // namespace explore_sites
#endif  // CHROME_BROWSER_ANDROID_EXPLORE_SITES_EXPLORE_SITES_TYPES_H_
