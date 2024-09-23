// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/exo/chrome_data_exchange_delegate.h"

#include <string>
#include <vector>

#include "ash/public/cpp/app_types_util.h"
#include "chrome/browser/ash/crosapi/browser_util.h"
#include "chrome/browser/ash/crostini/crostini_util.h"
#include "chrome/browser/ash/file_manager/fileapi_util.h"
#include "chrome/browser/ash/file_manager/path_util.h"
#include "chrome/browser/ash/plugin_vm/plugin_vm_util.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chromeos/ash/components/borealis/borealis_util.h"
#include "content/public/common/drop_data.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/file_system_url.h"
#include "ui/aura/window.h"
#include "ui/base/data_transfer_policy/data_transfer_endpoint.h"

namespace ash {

namespace {

constexpr char kMimeTypeArcUriList[] = "application/x-arc-uri-list";
constexpr char kMimeTypeTextUriList[] = "text/uri-list";

storage::FileSystemContext* GetFileSystemContext() {
  Profile* primary_profile = ProfileManager::GetPrimaryUserProfile();
  if (!primary_profile)
    return nullptr;

  return file_manager::util::GetFileManagerFileSystemContext(primary_profile);
}

}  // namespace

std::vector<storage::FileSystemURL> GetFileSystemUrlsFromPickle(
    const base::Pickle& pickle) {
  std::vector<storage::FileSystemURL> file_system_urls;

  storage::FileSystemContext* file_system_context = GetFileSystemContext();
  if (!file_system_context) {
    return file_system_urls;
  }

  std::vector<content::DropData::FileSystemFileInfo> file_system_files;
  if (!content::DropData::FileSystemFileInfo::ReadFileSystemFilesFromPickle(
          pickle, &file_system_files)) {
    return file_system_urls;
  }

  for (const auto& file_system_file : file_system_files) {
    const storage::FileSystemURL file_system_url =
        file_system_context->CrackURLInFirstPartyContext(file_system_file.url);
    if (file_system_url.is_valid()) {
      file_system_urls.push_back(std::move(file_system_url));
    }
  }

  return file_system_urls;
}

ChromeDataExchangeDelegate::ChromeDataExchangeDelegate() = default;

ChromeDataExchangeDelegate::~ChromeDataExchangeDelegate() = default;

ui::EndpointType ChromeDataExchangeDelegate::GetDataTransferEndpointType(
    aura::Window* target) const {
  auto* top_level_window = target->GetToplevelWindow();

  if (IsArcWindow(top_level_window))
    return ui::EndpointType::kArc;

  if (ash::borealis::IsBorealisWindow(top_level_window)) {
    return ui::EndpointType::kBorealis;
  }

  if (crosapi::browser_util::IsLacrosWindow(top_level_window))
    return ui::EndpointType::kLacros;

  if (crostini::IsCrostiniWindow(top_level_window))
    return ui::EndpointType::kCrostini;

  if (plugin_vm::IsPluginVmAppWindow(top_level_window))
    return ui::EndpointType::kPluginVm;

  return ui::EndpointType::kUnknownVm;
}

std::string ChromeDataExchangeDelegate::GetMimeTypeForUriList(
    ui::EndpointType target) const {
  return target == ui::EndpointType::kArc ? kMimeTypeArcUriList
                                          : kMimeTypeTextUriList;
}

bool ChromeDataExchangeDelegate::HasUrlsInPickle(
    const base::Pickle& pickle) const {
  return !GetFileSystemUrlsFromPickle(pickle).empty();
}

std::vector<ui::FileInfo> ChromeDataExchangeDelegate::ParseFileSystemSources(
    const ui::DataTransferEndpoint* source,
    const base::Pickle& pickle) const {
  return file_manager::util::ParseFileSystemSources(source, pickle);
}

}  // namespace ash
