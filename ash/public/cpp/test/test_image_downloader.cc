// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "ash/public/cpp/test/test_image_downloader.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/task/sequenced_task_runner.h"
#include "components/account_id/account_id.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_unittest_util.h"

namespace ash {

TestImageDownloader::TestImageDownloader() = default;

TestImageDownloader::~TestImageDownloader() = default;

void TestImageDownloader::Download(
    const GURL& url,
    const net::NetworkTrafficAnnotationTag& annotation_tag,
    DownloadCallback callback) {
  Download(url, annotation_tag, /*additional_headers=*/{},
           /*credentials_account_id=*/absl::nullopt, std::move(callback));
}

void TestImageDownloader::Download(
    const GURL& url,
    const net::NetworkTrafficAnnotationTag& annotation_tag,
    const net::HttpRequestHeaders& additional_headers,
    absl::optional<AccountId> credentials_account_id,
    DownloadCallback callback) {
  last_request_headers_ = additional_headers;
  // Pretend to respond asynchronously.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback),
                     should_fail_ ? gfx::ImageSkia()
                                  : gfx::test::CreateImageSkia(
                                        /*width=*/10, /*height=*/20)));
}

}  // namespace ash
