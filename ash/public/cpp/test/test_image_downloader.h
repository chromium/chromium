// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_TEST_TEST_IMAGE_DOWNLOADER_H_
#define ASH_PUBLIC_CPP_TEST_TEST_IMAGE_DOWNLOADER_H_

#include "ash/public/cpp/ash_public_export.h"
#include "ash/public/cpp/image_downloader.h"
#include "net/http/http_request_headers.h"

class AccountId;
class GURL;

namespace net {
struct NetworkTrafficAnnotationTag;
}  // namespace net

namespace ash {

class ASH_PUBLIC_EXPORT TestImageDownloader : public ImageDownloader {
 public:
  TestImageDownloader();
  ~TestImageDownloader() override;

  void set_should_fail(bool should_fail) { should_fail_ = should_fail; }
  const net::HttpRequestHeaders& last_request_headers() const {
    return last_request_headers_;
  }

  // ImageDownloader:
  void Download(const GURL& url,
                const net::NetworkTrafficAnnotationTag& annotation_tag,
                const AccountId& account_id,
                DownloadCallback callback) override;
  void Download(const GURL& url,
                const net::NetworkTrafficAnnotationTag& annotation_tag,
                const AccountId& account_id,
                const net::HttpRequestHeaders& additional_headers,
                DownloadCallback callback) override;

 private:
  bool should_fail_ = false;
  net::HttpRequestHeaders last_request_headers_;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_TEST_TEST_IMAGE_DOWNLOADER_H_
