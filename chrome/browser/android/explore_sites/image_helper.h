// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_EXPLORE_SITES_IMAGE_HELPER_H_
#define CHROME_BROWSER_ANDROID_EXPLORE_SITES_IMAGE_HELPER_H_

#include <map>
#include <memory>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/android/explore_sites/explore_sites_types.h"
#include "services/service_manager/public/cpp/connector.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace explore_sites {

enum class ImageJobType { kSiteIcon, kCategoryImage };

// A helper for converting webp images to bitmaps for the frontend.
//
// The ComposeSiteImage() and ComposeCategoryImage() functions are used for
// gettting the icons for the ESP and the NTP, respectively.
class ImageHelper {
 public:
  ImageHelper();
  virtual ~ImageHelper();

  // Compose a single site icon and return via |callback|.
  void ComposeSiteImage(
      BitmapCallback callback,
      EncodedImageList images,
      std::unique_ptr<service_manager::Connector> connector = nullptr);

  // Compose a category icon containing [1 - 4] site icons and return via
  // |callback|.
  void ComposeCategoryImage(
      BitmapCallback callback,
      int pixel_size,
      EncodedImageList images,
      std::unique_ptr<service_manager::Connector> connector = nullptr);

 private:
  class Job;

  void NewJob(ImageJobType job_type,
              ImageJobFinishedCallback job_finished_callback,
              BitmapCallback bitmap_callback,
              EncodedImageList images,
              int pixel_size,
              std::unique_ptr<service_manager::Connector> connector);

  void OnJobFinished(int job_id);

  std::map<int, std::unique_ptr<Job>> id_to_job_;
  int last_used_job_id_;

  base::WeakPtrFactory<ImageHelper> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ImageHelper);
};

}  // namespace explore_sites

#endif  // CHROME_BROWSER_ANDROID_EXPLORE_SITES_IMAGE_HELPER_H_
