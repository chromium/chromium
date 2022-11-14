// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/app_icon/app_icon_decoder.h"

#include "base/task/thread_pool.h"
#include "ui/base/layout.h"

namespace apps {

AppIconDecoder::AppIconDecoder(
    const base::FilePath& base_path,
    const std::string& app_id,
    int32_t size_in_dip,
    base::OnceCallback<void(AppIconDecoder* decoder, gfx::ImageSkia icon)>
        callback)
    : base_path_(base_path),
      app_id_(app_id),
      size_in_dip_(size_in_dip),
      callback_(std::move(callback)) {
  for (const auto& scale_factor : ui::GetSupportedResourceScaleFactors()) {
    incomplete_scale_factors_.insert(scale_factor);
  }
}

AppIconDecoder::~AppIconDecoder() = default;

void AppIconDecoder::Start() {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::BindOnce(&ReadIconFilesOnBackgroundThread, base_path_, app_id_,
                     size_in_dip_),
      base::BindOnce(&AppIconDecoder::OnIconRead,
                     weak_ptr_factory_.GetWeakPtr()));
}

void AppIconDecoder::OnIconRead(
    std::map<ui::ResourceScaleFactor, std::vector<uint8_t>> icon_datas) {
  // TODO(crbug.com/1380608): Implement the icon decode function.
}

}  // namespace apps
