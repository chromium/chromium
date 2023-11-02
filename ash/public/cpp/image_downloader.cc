// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/image_downloader.h"

#include "base/check_op.h"

namespace ash {
namespace {
ImageDownloader* g_image_downloader = nullptr;
}

// static
ImageDownloader* ImageDownloader::Get() {
  return g_image_downloader;
}

ImageDownloader::ImageDownloader() {
  DCHECK(!g_image_downloader);
  g_image_downloader = this;
}

ImageDownloader::~ImageDownloader() {
  DCHECK_EQ(g_image_downloader, this);
  g_image_downloader = nullptr;
}

}  // namespace ash
