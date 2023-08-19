// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SUPERVISED_USER_CHROMEOS_MOCK_LARGE_ICON_SERVICE_H_
#define CHROME_BROWSER_SUPERVISED_USER_CHROMEOS_MOCK_LARGE_ICON_SERVICE_H_

#include "components/favicon/core/large_icon_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/gfx/image/image_skia.h"
#include "url/gurl.h"

class MockLargeIconService : public favicon::LargeIconService {
 public:
  MockLargeIconService();

  MockLargeIconService(const MockLargeIconService&) = delete;
  MockLargeIconService& operator=(const MockLargeIconService&) = delete;
  ~MockLargeIconService() override;

  void StoreIconInCache();

  gfx::ImageSkia favicon() const { return favicon_; }

  // LargeIconService overrides.
  MOCK_METHOD5(GetLargeIconOrFallbackStyleFromGoogleServerSkippingLocalCache,
               void(const GURL& page_url,
                    bool may_page_url_be_private,
                    bool should_trim_page_url_path,
                    const net::NetworkTrafficAnnotationTag& traffic_annotation,
                    favicon_base::GoogleFaviconServerCallback callback));
  MOCK_METHOD5(GetLargeIconRawBitmapOrFallbackStyleForPageUrl,
               base::CancelableTaskTracker::TaskId(
                   const GURL& page_url,
                   int min_source_size_in_pixel,
                   int desired_size_in_pixel,
                   favicon_base::LargeIconCallback callback,
                   base::CancelableTaskTracker* tracker));
  MOCK_METHOD5(GetLargeIconImageOrFallbackStyleForPageUrl,
               base::CancelableTaskTracker::TaskId(
                   const GURL& page_url,
                   int min_source_size_in_pixel,
                   int desired_size_in_pixel,
                   favicon_base::LargeIconImageCallback callback,
                   base::CancelableTaskTracker* tracker));
  MOCK_METHOD4(GetLargeIconRawBitmapForPageUrl,
               base::CancelableTaskTracker::TaskId(
                   const GURL& page_url,
                   int min_source_size_in_pixel,
                   favicon_base::FaviconRawBitmapCallback callback,
                   base::CancelableTaskTracker* tracker));
  MOCK_METHOD5(GetLargeIconRawBitmapOrFallbackStyleForIconUrl,
               base::CancelableTaskTracker::TaskId(
                   const GURL& icon_url,
                   int min_source_size_in_pixel,
                   int desired_size_in_pixel,
                   favicon_base::LargeIconCallback callback,
                   base::CancelableTaskTracker* tracker));
  MOCK_METHOD4(GetIconRawBitmapOrFallbackStyleForPageUrl,
               base::CancelableTaskTracker::TaskId(
                   const GURL& page_url,
                   int desired_size_in_pixel,
                   favicon_base::LargeIconCallback callback,
                   base::CancelableTaskTracker* tracker));
  MOCK_METHOD1(TouchIconFromGoogleServer, void(const GURL& icon_url));

  const GURL kIconUrl = GURL("https://www.example.com/icon");

 private:
  gfx::ImageSkia favicon_;
};

#endif  // CHROME_BROWSER_SUPERVISED_USER_CHROMEOS_MOCK_LARGE_ICON_SERVICE_H_
