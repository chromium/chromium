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

AppIconWriter::Key::Key(const std::string& app_id,
                        int32_t size_in_dip,
                        IconEffects icon_effects,
                        IconType icon_type)
    : app_id_(app_id),
      size_in_dip_(size_in_dip),
      icon_effects_(icon_effects),
      icon_type_(icon_type) {}

AppIconWriter::Key::~Key() = default;

bool AppIconWriter::Key::operator<(const Key& other) const {
  if (this->app_id_ != other.app_id_) {
    return this->app_id_ < other.app_id_;
  }
  if (this->size_in_dip_ != other.size_in_dip_) {
    return this->size_in_dip_ < other.size_in_dip_;
  }
  if (this->icon_effects_ != other.icon_effects_) {
    return this->icon_effects_ < other.icon_effects_;
  }
  return this->icon_type_ < other.icon_type_;
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
                                IconEffects icon_effects,
                                IconType icon_type,
                                base::OnceCallback<void(bool)> callback) {
  DCHECK(publisher);

  if (icon_type == IconType::kUnknown) {
    std::move(callback).Run(false);
    return;
  }

  Key key(app_id, size_in_dip, icon_effects, icon_type);
  auto it = pending_results_.find(key);
  if (it != pending_results_.end()) {
    it->second.callbacks.push_back(std::move(callback));
    return;
  }

  PendingResult pending_result;
  std::set<ui::ResourceScaleFactor> scale_factors;
  if (icon_type == IconType::kCompressed &&
      icon_effects == apps::IconEffects::kNone) {
    scale_factors.insert(ui::GetSupportedResourceScaleFactor(
        apps_util::GetPrimaryDisplayUIScaleFactor()));
  } else {
    for (auto scale_factor : ui::GetSupportedResourceScaleFactors()) {
      scale_factors.insert(scale_factor);
    }
  }

  pending_result.callbacks.push_back(std::move(callback));
  pending_result.scale_factors = scale_factors;

  pending_results_[Key(app_id, size_in_dip, icon_effects, icon_type)] =
      std::move(pending_result);
  for (auto scale_factor : scale_factors) {
    auto pending_results_it = pending_results_.find(key);
    if (pending_results_it == pending_results_.end()) {
      // If the getting icon request has been removed (e.g. the compressed
      // icon data doesn't exist) by OnIconLoad, we don't need to continue
      // getting other scale factors for the icon request.
      return;
    }

    pending_results_it->second.scale_factors.insert(scale_factor);
    publisher->GetCompressedIconData(
        app_id, size_in_dip, scale_factor,
        base::BindOnce(&AppIconWriter::OnIconLoad,
                       weak_ptr_factory_.GetWeakPtr(), app_id, size_in_dip,
                       icon_effects, icon_type, scale_factor));
  }
}

void AppIconWriter::OnIconLoad(const std::string& app_id,
                               int32_t size_in_dip,
                               IconEffects icon_effects,
                               IconType icon_type,
                               ui::ResourceScaleFactor scale_factor,
                               IconValuePtr iv) {
  auto it =
      pending_results_.find(Key(app_id, size_in_dip, icon_effects, icon_type));
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

  if (it->second.scale_factors.find(scale_factor) ==
      it->second.scale_factors.end()) {
    // If the getting icon request for `scale_factor` has been removed (e.g. the
    // compressed icon data has been written to the local disk), we can call
    // OnWriteIconFile directly and don't need to call OnWriteIconFile to write
    // the icon data.
    OnWriteIconFile(app_id, size_in_dip, icon_effects, icon_type, scale_factor,
                    std::move(iv));
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
                     icon_effects, icon_type, scale_factor, std::move(iv)));
}

void AppIconWriter::OnWriteIconFile(const std::string& app_id,
                                    int32_t size_in_dip,
                                    IconEffects icon_effects,
                                    IconType icon_type,
                                    ui::ResourceScaleFactor scale_factor,
                                    IconValuePtr iv) {
  auto it =
      pending_results_.find(Key(app_id, size_in_dip, icon_effects, icon_type));
  if (it == pending_results_.end()) {
    return;
  }

  it->second.scale_factors.erase(scale_factor);
  if (!it->second.scale_factors.empty()) {
    // There are other icon fetching requests, so wait for other icon data.
    return;
  }

  for (auto& callback : it->second.callbacks) {
    std::move(callback).Run(true);
  }

  pending_results_.erase(it);
}

}  // namespace apps
