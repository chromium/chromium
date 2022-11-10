// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/app_icon/app_icon_reader.h"

#include "base/files/file_util.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/apps/app_service/app_icon/dip_px_util.h"
#include "chrome/browser/profiles/profile.h"

namespace {

std::vector<uint8_t> ReadOnBackgroundThread(Profile* profile,
                                            const std::string& app_id,
                                            int32_t icon_size_in_px) {
  const auto icon_path = apps::GetIconPath(profile, app_id, icon_size_in_px);
  if (icon_path.empty() || !base::PathExists(icon_path)) {
    return std::vector<uint8_t>{};
  }

  std::string unsafe_icon_data;
  if (!base::ReadFileToString(icon_path, &unsafe_icon_data)) {
    return std::vector<uint8_t>{};
  }

  return {unsafe_icon_data.begin(), unsafe_icon_data.end()};
}

}  // namespace

namespace apps {

AppIconReader::AppIconReader(Profile* profile) : profile_(profile) {}

AppIconReader::~AppIconReader() = default;

void AppIconReader::ReadIcons(const std::string& app_id,
                              int32_t size_hint_in_dip,
                              IconEffects icon_effects,
                              IconType icon_type,
                              LoadIconCallback callback) {
  switch (icon_type) {
    case IconType::kUnknown: {
      std::move(callback).Run(std::make_unique<apps::IconValue>());
      return;
    }
    case IconType::kCompressed:
      if (icon_effects == apps::IconEffects::kNone) {
        base::ThreadPool::PostTaskAndReplyWithResult(
            FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
            base::BindOnce(&ReadOnBackgroundThread, profile_, app_id,
                           apps_util::ConvertDipToPx(
                               size_hint_in_dip,
                               /*quantize_to_supported_scale_factor=*/true)),
            base::BindOnce(&AppIconReader::OnIconRead,
                           weak_ptr_factory_.GetWeakPtr(), icon_type,
                           std::move(callback)));
        return;
      }
      [[fallthrough]];
    case IconType::kUncompressed:
      [[fallthrough]];
    case IconType::kStandard: {
      // TODO(crbug.com/1380608): Implement the icon reading function.
    }
  }
}

void AppIconReader::OnIconRead(IconType icon_type,
                               LoadIconCallback callback,
                               std::vector<uint8_t> icon_data) {
  // TODO(crbug.com/1380608): Implement OnIconRead for uncompressed and standard
  // icons.

  auto iv = std::make_unique<apps::IconValue>();
  iv->icon_type = icon_type;
  iv->compressed = std::move(icon_data);

  std::move(callback).Run(std::move(iv));
}

}  // namespace apps
