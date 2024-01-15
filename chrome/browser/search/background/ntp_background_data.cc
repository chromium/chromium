// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search/background/ntp_background_data.h"

namespace {
// The options to be added to a thumbnail image URL, specifying resolution,
// cropping, etc. Options appear on an image URL after the '=' character. This
// resolution matches the height an width of bg-sel-tile.
constexpr char kThumbnailImageOptions[] = "=w156-h117-p-k-no-nd-mv";
}  // namespace

std::string GetThumbnailImageOptions() {
  return kThumbnailImageOptions;
}

GURL AddOptionsToImageURL(const std::string& image_url,
                          const std::string& image_options) {
  return GURL(image_url + ((image_url.find('=') == std::string::npos)
                               ? image_options
                               : std::string("")));
}

CollectionInfo::CollectionInfo() = default;
CollectionInfo::CollectionInfo(const CollectionInfo&) = default;
CollectionInfo::CollectionInfo(CollectionInfo&&) = default;
CollectionInfo::~CollectionInfo() = default;

CollectionInfo& CollectionInfo::operator=(const CollectionInfo&) = default;
CollectionInfo& CollectionInfo::operator=(CollectionInfo&&) = default;

bool operator==(const CollectionInfo& lhs, const CollectionInfo& rhs) {
  return lhs.collection_id == rhs.collection_id &&
         lhs.collection_name == rhs.collection_name &&
         lhs.preview_image_url == rhs.preview_image_url;
}

bool operator!=(const CollectionInfo& lhs, const CollectionInfo& rhs) {
  return !(lhs == rhs);
}

CollectionInfo CollectionInfo::CreateFromProto(
    const ntp::background::Collection& collection,
    std::optional<GURL> preview_image_url) {
  CollectionInfo collection_info;
  collection_info.collection_id = collection.collection_id();
  collection_info.collection_name = collection.collection_name();
  if (preview_image_url.has_value()) {
    collection_info.preview_image_url = preview_image_url.value();
  }

  return collection_info;
}

CollectionImage::CollectionImage() = default;
CollectionImage::CollectionImage(const CollectionImage&) = default;
CollectionImage::CollectionImage(CollectionImage&&) = default;
CollectionImage::~CollectionImage() = default;

CollectionImage& CollectionImage::operator=(const CollectionImage&) = default;
CollectionImage& CollectionImage::operator=(CollectionImage&&) = default;

bool operator==(const CollectionImage& lhs, const CollectionImage& rhs) {
  return lhs.collection_id == rhs.collection_id &&
         lhs.asset_id == rhs.asset_id &&
         lhs.thumbnail_image_url == rhs.thumbnail_image_url &&
         lhs.image_url == rhs.image_url && lhs.attribution == rhs.attribution &&
         lhs.attribution_action_url == rhs.attribution_action_url;
}

bool operator!=(const CollectionImage& lhs, const CollectionImage& rhs) {
  return !(lhs == rhs);
}

CollectionImage CollectionImage::CreateFromProto(
    const std::string& collection_id,
    const ntp::background::Image& image,
    const GURL& default_image_url,
    const GURL& thumbnail_image_url) {
  CollectionImage collection_image;
  collection_image.collection_id = collection_id;
  collection_image.asset_id = image.asset_id();
  collection_image.thumbnail_image_url = thumbnail_image_url;
  collection_image.image_url = default_image_url;
  for (const auto& attribution : image.attribution()) {
    collection_image.attribution.push_back(attribution.text());
  }
  collection_image.attribution_action_url = GURL(image.action_url());

  return collection_image;
}

ErrorInfo::ErrorInfo() : net_error(0), error_type(ErrorType::NONE) {}
ErrorInfo::ErrorInfo(const ErrorInfo&) = default;
ErrorInfo::ErrorInfo(ErrorInfo&&) = default;
ErrorInfo::~ErrorInfo() = default;

ErrorInfo& ErrorInfo::operator=(const ErrorInfo&) = default;
ErrorInfo& ErrorInfo::operator=(ErrorInfo&&) = default;

void ErrorInfo::ClearError() {
  error_type = ErrorType::NONE;
  net_error = 0;
}

CustomBackground::CustomBackground() = default;
CustomBackground::CustomBackground(const CustomBackground&) = default;
CustomBackground::CustomBackground(CustomBackground&&) = default;
CustomBackground::~CustomBackground() = default;

CustomBackground& CustomBackground::operator=(const CustomBackground&) =
    default;
CustomBackground& CustomBackground::operator=(CustomBackground&&) = default;
