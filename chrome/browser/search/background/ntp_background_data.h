// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SEARCH_BACKGROUND_NTP_BACKGROUND_DATA_H_
#define CHROME_BROWSER_SEARCH_BACKGROUND_NTP_BACKGROUND_DATA_H_

#include <optional>
#include <string>

#include "base/token.h"
#include "chrome/browser/search/background/ntp_background.pb.h"
#include "third_party/skia/include/core/SkColor.h"
#include "url/gurl.h"

enum class ErrorType {
  // Data retrieved successfully.
  NONE,

  // Network error occurred.
  NET_ERROR,

  // Response from backend couldn't be read.
  SERVICE_ERROR,
};

std::string GetThumbnailImageOptions();
// Adds options for resizing an image to its url.
// Without options added to the image, it is 512x512.
// TODO(crbug.com/41408116): Request resolution from service, instead of
// setting it here.
GURL AddOptionsToImageURL(const std::string& image_url,
                          const std::string& image_options);

// Background images are organized into collections, according to a theme. This
// struct contains the data required to display information about a collection,
// including a representative image. The complete set of CollectionImages must
// be requested separately, by referencing the identifier for this collection.
struct CollectionInfo {
  CollectionInfo();
  CollectionInfo(const CollectionInfo&);
  CollectionInfo(CollectionInfo&&);
  ~CollectionInfo();

  CollectionInfo& operator=(const CollectionInfo&);
  CollectionInfo& operator=(CollectionInfo&&);

  static CollectionInfo CreateFromProto(
      const ntp::background::Collection& collection,
      std::optional<GURL> preview_image_url);

  // A unique identifier for the collection.
  std::string collection_id;
  // A human-readable name for the collection.
  std::string collection_name;
  // A representative image from the collection.
  GURL preview_image_url;
};

bool operator==(const CollectionInfo& lhs, const CollectionInfo& rhs);
bool operator!=(const CollectionInfo& lhs, const CollectionInfo& rhs);

// Represents an image within a collection. The associated collection_id may be
// used to get CollectionInfo.
struct CollectionImage {
  CollectionImage();
  CollectionImage(const CollectionImage&);
  CollectionImage(CollectionImage&&);
  ~CollectionImage();

  CollectionImage& operator=(const CollectionImage&);
  CollectionImage& operator=(CollectionImage&&);

  // default_image_options are applied to the image.image_url() if options
  // (specifying resolution, cropping, etc) are not already present.
  static CollectionImage CreateFromProto(const std::string& collection_id,
                                         const ntp::background::Image& image,
                                         const GURL& default_image_url,
                                         const GURL& thumbnail_image_url);

  // A unique identifier for the collection the image is in.
  std::string collection_id;
  // A unique identifier for the image.
  uint64_t asset_id;
  // The thumbnail image URL, typically lower resolution than the image_url.
  GURL thumbnail_image_url;
  // The image URL.
  GURL image_url;
  // The attribution list for the image.
  std::vector<std::string> attribution;
  // A URL that can be accessed to find out more information about the image.
  GURL attribution_action_url;
};

bool operator==(const CollectionImage& lhs, const CollectionImage& rhs);
bool operator!=(const CollectionImage& lhs, const CollectionImage& rhs);

// Represents errors that occur when communicating with the Backdrop service and
// Google Photos.
struct ErrorInfo {
  ErrorInfo();
  ErrorInfo(const ErrorInfo&);
  ErrorInfo(ErrorInfo&&);
  ~ErrorInfo();

  ErrorInfo& operator=(const ErrorInfo&);
  ErrorInfo& operator=(ErrorInfo&&);

  void ClearError();

  // Network error number, listed at chrome://network-errors.
  int net_error;

  // Category of error that occured.
  ErrorType error_type;
};

// Represents a custom background on the new tab page.
struct CustomBackground {
  CustomBackground();
  CustomBackground(const CustomBackground&);
  CustomBackground(CustomBackground&&);
  ~CustomBackground();

  CustomBackground& operator=(const CustomBackground&);
  CustomBackground& operator=(CustomBackground&&);

  // Url of the custom background selected by the user.
  GURL custom_background_url;

  // Url of snapshot for the custom background selected by the user.
  GURL custom_background_snapshot_url;

  // Whether the image is a local resource.
  bool is_uploaded_image = false;

  // Id for local custom background. This can be empty if it is an uploaded
  // local background, rather than from wallpaper search.
  std::optional<base::Token> local_background_id;

  // Whether the image is an inspiration image. This information is only
  // used if local_background_id is set.
  bool is_inspiration_image = false;

  // First attribution string for custom background.
  std::string custom_background_attribution_line_1;

  // Second attribution string for custom background.
  std::string custom_background_attribution_line_2;

  // Url to learn more info about the custom background.
  GURL custom_background_attribution_action_url;

  // Id of the collection being used.
  std::string collection_id;

  // Main color of the image.
  std::optional<SkColor> custom_background_main_color;

  // Whether daily refresh is enabled.
  bool daily_refresh_enabled = false;
};

#endif  // CHROME_BROWSER_SEARCH_BACKGROUND_NTP_BACKGROUND_DATA_H_
