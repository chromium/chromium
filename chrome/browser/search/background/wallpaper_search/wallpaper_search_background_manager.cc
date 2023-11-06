// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search/background/wallpaper_search/wallpaper_search_background_manager.h"

#include <string>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/thread_pool.h"
#include "base/token.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/background/ntp_custom_background_service_factory.h"
#include "chrome/common/url_constants.h"
#include "components/keyed_service/core/keyed_service.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/image/image.h"

namespace {

void WriteFileToPath(const std::string& data, const base::FilePath& path) {
  base::WriteFile(path, base::as_bytes(base::make_span(data)));
}

}  // namespace

WallpaperSearchBackgroundManager::WallpaperSearchBackgroundManager(
    Profile* profile)
    : ntp_custom_background_service_(
          NtpCustomBackgroundServiceFactory::GetForProfile(profile)),
      profile_(profile) {
  CHECK(ntp_custom_background_service_);
}

WallpaperSearchBackgroundManager::~WallpaperSearchBackgroundManager() = default;

void WallpaperSearchBackgroundManager::SelectLocalBackgroundImage(
    const base::Token& id,
    const SkBitmap& bitmap) {
  if (ntp_custom_background_service_->IsCustomBackgroundDisabledByPolicy()) {
    return;
  }

  std::vector<unsigned char> encoded;
  const bool success = gfx::PNGCodec::EncodeBGRASkBitmap(
      bitmap, /*discard_transparency=*/false, &encoded);
  if (success) {
    base::ThreadPool::PostTaskAndReply(
        FROM_HERE, {base::TaskPriority::USER_VISIBLE, base::MayBlock()},
        base::BindOnce(
            &WriteFileToPath, std::string(encoded.begin(), encoded.end()),
            profile_->GetPath().AppendASCII(
                id.ToString() +
                chrome::kChromeUIUntrustedNewTabPageBackgroundFilename)),
        base::BindOnce(
            &NtpCustomBackgroundService::SetBackgroundToLocalResourceWithId,
            base::Unretained(ntp_custom_background_service_), id));
    ntp_custom_background_service_->UpdateCustomLocalBackgroundColorAsync(
        gfx::Image::CreateFrom1xBitmap(bitmap));
  }
}
