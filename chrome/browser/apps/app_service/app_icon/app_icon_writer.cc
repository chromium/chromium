// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/app_icon/app_icon_writer.h"

#include "base/files/file_util.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/apps/app_service/app_icon/app_icon_util.h"
#include "chrome/browser/apps/app_service/app_icon/dip_px_util.h"
#include "chrome/browser/apps/app_service/publishers/app_publisher.h"
#include "chrome/browser/profiles/profile.h"
#include "ui/base/layout.h"

namespace {

void WriteIconFile(const base::FilePath& base_path,
                   const std::string& app_id,
                   int32_t icon_size_in_px,
                   bool is_maskable_icon,
                   const std::vector<uint8_t>& icon_data) {
  if (icon_data.empty()) {
    return;
  }

  const auto icon_path =
      apps::GetIconPath(base_path, app_id, icon_size_in_px, is_maskable_icon);
  if (!base::CreateDirectory(icon_path.DirName())) {
    return;
  }

  base::WriteFile(icon_path, reinterpret_cast<const char*>(&icon_data[0]),
                  icon_data.size());
}

}  // namespace

namespace apps {

AppIconWriter::Key::Key(const std::string& app_id, int32_t size_in_dip)
    : app_id_(app_id), size_in_dip_(size_in_dip) {}

AppIconWriter::Key::~Key() = default;

bool AppIconWriter::Key::operator<(const Key& other) const {
  if (this->app_id_ != other.app_id_) {
    return this->app_id_ < other.app_id_;
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

void AppIconWriter::InstallIcon(AppPublisher* publisher,
                                const std::string& app_id,
                                int32_t size_in_dip,
                                base::OnceCallback<void(bool)> callback) {
  DCHECK(publisher);

  Key key(app_id, size_in_dip);
  auto it = pending_results_.find(key);
  if (it != pending_results_.end()) {
    it->second.callbacks.push_back(std::move(callback));
    return;
  }

  pending_results_[Key(app_id, size_in_dip)].callbacks.push_back(
      std::move(callback));
  it = pending_results_.find(key);

  std::set<ui::ResourceScaleFactor> scale_factors;
  // For the adaptive icon, we need to get the raw icon data for all scale
  // factors to convert to the uncompressed icon, then generate the adaptive
  // icon with both the foreground and the background icon files. Since we don't
  // know whether the icon is an adaptive icon, we always get the raw icon data
  // for all scale factors.
  for (auto scale_factor : ui::GetSupportedResourceScaleFactors()) {
    it->second.scale_factors.insert(scale_factor);
    scale_factors.insert(scale_factor);
  }

  for (auto scale_factor : scale_factors) {
    auto pending_results_it = pending_results_.find(key);
    if (pending_results_it == pending_results_.end()) {
      // If the getting icon request has been removed (e.g. the compressed
      // icon data doesn't exist) by OnIconLoad, we don't need to continue
      // getting other scale factors for the icon request.
      return;
    }

    publisher->GetCompressedIconData(
        app_id, size_in_dip, scale_factor,
        base::BindOnce(&AppIconWriter::OnIconLoad,
                       weak_ptr_factory_.GetWeakPtr(), app_id, size_in_dip,
                       scale_factor));
  }
}

void AppIconWriter::OnIconLoad(const std::string& app_id,
                               int32_t size_in_dip,
                               ui::ResourceScaleFactor scale_factor,
                               IconValuePtr iv) {
  auto it = pending_results_.find(Key(app_id, size_in_dip));
  if (it == pending_results_.end()) {
    return;
  }

  if (!iv || iv->icon_type != IconType::kCompressed || iv->compressed.empty()) {
    for (auto& callback : it->second.callbacks) {
      std::move(callback).Run(false);
    }
    pending_results_.erase(it);
    return;
  }

  std::vector<uint8_t> icon_data = iv->compressed;
  bool is_maskable_icon = iv->is_maskable_icon;
  base::ThreadPool::PostTaskAndReply(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::BindOnce(
          &WriteIconFile, profile_->GetPath(), app_id,
          apps_util::ConvertDipToPxForScale(size_in_dip, scale_factor),
          is_maskable_icon, std::move(icon_data)),
      base::BindOnce(&AppIconWriter::OnWriteIconFile,
                     weak_ptr_factory_.GetWeakPtr(), app_id, size_in_dip,
                     scale_factor, std::move(iv)));
}

void AppIconWriter::OnWriteIconFile(const std::string& app_id,
                                    int32_t size_in_dip,
                                    ui::ResourceScaleFactor scale_factor,
                                    IconValuePtr iv) {
  auto it = pending_results_.find(Key(app_id, size_in_dip));
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
    std::move(callback).Run(true);
  }

  pending_results_.erase(it);
}

}  // namespace apps
