// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "ash/public/cpp/test/test_image_downloader.h"
#include "base/callback.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_unittest_util.h"

namespace ash {

TestImageDownloader::TestImageDownloader() = default;

TestImageDownloader::~TestImageDownloader() = default;

void TestImageDownloader::Download(
    const GURL& url,
    const net::NetworkTrafficAnnotationTag& annotation_tag,
    DownloadCallback callback) {
  // Pretend to respond asynchronously.
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), gfx::test::CreateImageSkia(
                                              /*width=*/10, /*height=*/20)));
}

}  // namespace ash
