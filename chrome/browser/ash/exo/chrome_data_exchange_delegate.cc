// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/exo/chrome_data_exchange_delegate.h"

#include <string>
#include <vector>

#include "ash/public/cpp/app_types_util.h"
#include "base/check.h"
#include "chrome/browser/ash/crostini/crostini_util.h"
#include "chrome/browser/ash/file_manager/fileapi_util.h"
#include "chrome/browser/ash/file_manager/path_util.h"
#include "chromeos/ash/components/borealis/borealis_util.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "components/session_manager/core/session.h"
#include "components/session_manager/core/session_manager.h"
#include "content/public/browser/browser_context.h"
#include "content/public/common/drop_data.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/file_system_url.h"
#include "ui/aura/window.h"
#include "ui/base/data_transfer_policy/data_transfer_endpoint.h"

namespace ash {

namespace {

constexpr char kMimeTypeArcUriList[] = "application/x-arc-uri-list";
constexpr char kMimeTypeTextUriList[] = "text/uri-list";

}  // namespace

std::vector<storage::FileSystemURL> GetFileSystemUrlsFromPickle(
    content::BrowserContext* browser_context,
    const base::Pickle& pickle) {
  std::vector<storage::FileSystemURL> file_system_urls;

  storage::FileSystemContext* file_system_context =
      file_manager::util::GetFileManagerFileSystemContext(browser_context);
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

  if (crostini::IsCrostiniWindow(top_level_window))
    return ui::EndpointType::kCrostini;

  return ui::EndpointType::kUnknownVm;
}

std::string ChromeDataExchangeDelegate::GetMimeTypeForUriList(
    ui::EndpointType target) const {
  return target == ui::EndpointType::kArc ? kMimeTypeArcUriList
                                          : kMimeTypeTextUriList;
}

bool ChromeDataExchangeDelegate::HasUrlsInPickle(
    const base::Pickle& pickle) const {
  const session_manager::Session* primary_session =
      session_manager::SessionManager::Get()->GetPrimarySession();
  CHECK(primary_session);
  content::BrowserContext* primary_context =
      ash::BrowserContextHelper::Get()->GetBrowserContextByAccountId(
          primary_session->account_id());
  CHECK(primary_context);
  return !GetFileSystemUrlsFromPickle(primary_context, pickle).empty();
}

std::vector<ui::FileInfo> ChromeDataExchangeDelegate::ParseFileSystemSources(
    const ui::DataTransferEndpoint* source,
    const base::Pickle& pickle) const {
  return file_manager::util::ParseFileSystemSources(source, pickle);
}

}  // namespace ash
