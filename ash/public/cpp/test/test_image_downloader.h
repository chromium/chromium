// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_TEST_TEST_IMAGE_DOWNLOADER_H_
#define ASH_PUBLIC_CPP_TEST_TEST_IMAGE_DOWNLOADER_H_

#include "ash/public/cpp/ash_public_export.h"
#include "ash/public/cpp/image_downloader.h"

namespace ash {

class ASH_PUBLIC_EXPORT TestImageDownloader : public ImageDownloader {
 public:
  TestImageDownloader();
  ~TestImageDownloader() override;

  // ImageDownloader:
  void Download(const GURL& url,
                const net::NetworkTrafficAnnotationTag& annotation_tag,
                DownloadCallback callback) override;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_TEST_TEST_IMAGE_DOWNLOADER_H_
