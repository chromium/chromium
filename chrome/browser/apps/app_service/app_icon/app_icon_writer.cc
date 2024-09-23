// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/apps/app_service/app_icon/app_icon_writer.h"

#include "base/containers/span.h"
#include "base/files/file_util.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/apps/app_service/app_icon/app_icon_util.h"
#include "chrome/browser/apps/app_service/app_icon/compressed_icon_getter.h"
#include "chrome/browser/apps/app_service/app_icon/dip_px_util.h"
#include "chrome/browser/profiles/profile.h"
#include "ui/base/resource/resource_scale_factor.h"

namespace {

bool WriteIconFiles(const base::FilePath& base_path,
                    const std::string& id,
                    int32_t icon_size_in_px,
                    apps::IconValuePtr iv) {
  if (!iv || iv->icon_type != apps::IconType::kCompressed) {
    return false;
  }

  if (!iv->foreground_icon_png_data.empty() &&
      !iv->background_icon_png_data.empty()) {
    // For the adaptive icon, write the foreground and background icon data to
    // the local files.
    const auto foreground_icon_path =
        apps::GetForegroundIconPath(base_path, id, icon_size_in_px);
    const auto background_icon_path =
        apps::GetBackgroundIconPath(base_path, id, icon_size_in_px);
    if (!base::CreateDirectory(foreground_icon_path.DirName()) ||
        !base::CreateDirectory(background_icon_path.DirName())) {
      return false;
    }

    auto foreground_icon_data = base::make_span(
        &iv->foreground_icon_png_data[0], iv->foreground_icon_png_data.size());
    auto background_icon_data = base::make_span(
        &iv->background_icon_png_data[0], iv->background_icon_png_data.size());
    return base::WriteFile(foreground_icon_path, foreground_icon_data) &&
           base::WriteFile(background_icon_path, background_icon_data);
  }

  if (iv->compressed.empty()) {
    return false;
  }

  const auto icon_path =
      apps::GetIconPath(base_path, id, icon_size_in_px, iv->is_maskable_icon);
  if (!base::CreateDirectory(icon_path.DirName())) {
    return false;
  }

  auto icon_data = base::make_span(&iv->compressed[0], iv->compressed.size());
  return base::WriteFile(icon_path, icon_data);
}

}  // namespace

namespace apps {

AppIconWriter::Key::Key(const std::string& id, int32_t size_in_dip)
    : id_(id), size_in_dip_(size_in_dip) {}

AppIconWriter::Key::~Key() = default;

bool AppIconWriter::Key::operator<(const Key& other) const {
  if (this->id_ != other.id_) {
    return this->id_ < other.id_;
  }
  return this->size_in_dip_ < other.size_in_dip_;
}

AppIconWriter::PendingResult::PendingResult() = default;
AppIconWriter::PendingResult::~PendingResult() = default;
AppIconWriter::PendingResult::PendingResult(PendingResult&&) = default;
AppIconWriter::PendingResult& AppIconWriter::PendingResult::operator=(
    PendingResult&&) = default;

AppIconWriter::AppIconWriter(Profile* profile) : profile_(profile) {}

AppIconWriter::~AppIconWriter() = default;

void AppIconWriter::InstallIcon(CompressedIconGetter* compressed_icon_getter,
                                const std::string& id,
                                int32_t size_in_dip,
                                base::OnceCallback<void(bool)> callback) {
  CHECK(compressed_icon_getter);
  Key key(id, size_in_dip);
  auto it = pending_results_.find(key);
  if (it != pending_results_.end()) {
    it->second.callbacks.push_back(std::move(callback));
    return;
  }

  pending_results_[Key(id, size_in_dip)].callbacks.push_back(
      std::move(callback));
  it = pending_results_.find(key);

  std::set<ui::ResourceScaleFactor> scale_factors;
  // For the adaptive icon, we need to get the raw icon data for all scale
  // factors to convert to the uncompressed icon, then generate the adaptive
  // icon with both the foreground and the background icon files. Since we don't
  // know whether the icon is an adaptive icon, we always get the raw icon data
  // for all scale factors.
  for (const auto scale_factor : ui::GetSupportedResourceScaleFactors()) {
    it->second.scale_factors.insert(scale_factor);
    scale_factors.insert(scale_factor);
  }

  for (const auto scale_factor : scale_factors) {
    auto pending_results_it = pending_results_.find(key);
    if (pending_results_it == pending_results_.end()) {
      // If the getting icon request has been removed (e.g. the compressed
      // icon data doesn't exist) by OnIconLoad, we don't need to continue
      // getting other scale factors for the icon request.
      return;
    }

    compressed_icon_getter->GetCompressedIconData(
        id, size_in_dip, scale_factor,
        base::BindOnce(&AppIconWriter::OnIconLoad,
                       weak_ptr_factory_.GetWeakPtr(), id, size_in_dip,
                       scale_factor));
  }
}

void AppIconWriter::OnIconLoad(const std::string& id,
                               int32_t size_in_dip,
                               ui::ResourceScaleFactor scale_factor,
                               IconValuePtr iv) {
  auto it = pending_results_.find(Key(id, size_in_dip));
  if (it == pending_results_.end()) {
    return;
  }

  if (!iv || iv->icon_type != IconType::kCompressed ||
      (iv->compressed.empty() && iv->foreground_icon_png_data.empty() &&
       iv->background_icon_png_data.empty())) {
    for (auto& callback : it->second.callbacks) {
      std::move(callback).Run(false);
    }
    pending_results_.erase(it);
    return;
  }

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::BindOnce(
          &WriteIconFiles, profile_->GetPath(), id,
          apps_util::ConvertDipToPxForScale(size_in_dip, scale_factor),
          std::move(iv)),
      base::BindOnce(&AppIconWriter::OnWriteIconFile,
                     weak_ptr_factory_.GetWeakPtr(), id, size_in_dip,
                     scale_factor));
}

void AppIconWriter::OnWriteIconFile(const std::string& id,
                                    int32_t size_in_dip,
                                    ui::ResourceScaleFactor scale_factor,
                                    bool ret) {
  auto it = pending_results_.find(Key(id, size_in_dip));
  if (it == pending_results_.end()) {
    return;
  }

  it->second.complete_scale_factors.insert(scale_factor);
  if (it->second.scale_factors != it->second.complete_scale_factors) {
    // There are other icon fetching requests, so wait for other icon data.
    return;
  }

  // The icon fetching requests have returned for all scale factors, so we can
  // call callbacks to return the result, and remove the icon request from
  // `pending_results_`.
  for (auto& callback : it->second.callbacks) {
    std::move(callback).Run(ret);
  }

  pending_results_.erase(it);
}

}  // namespace apps
