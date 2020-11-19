// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/exo_parts.h"

#include <string>
#include <vector>

#include "ash/public/cpp/app_types.h"
#include "ash/public/cpp/ash_switches.h"
#include "ash/public/cpp/external_arc/keyboard/arc_input_method_surface_manager.h"
#include "ash/public/cpp/external_arc/message_center/arc_notification_surface_manager_impl.h"
#include "ash/public/cpp/external_arc/overlay/arc_overlay_manager.h"
#include "ash/public/cpp/external_arc/toast/arc_toast_surface_manager.h"
#include "ash/shell.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted_memory.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/chromeos/crostini/crostini_shelf_utils.h"
#include "chrome/browser/chromeos/crostini/crostini_util.h"
#include "chrome/browser/chromeos/file_manager/app_id.h"
#include "chrome/browser/chromeos/file_manager/fileapi_util.h"
#include "chrome/browser/chromeos/file_manager/path_util.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/common/chrome_switches.h"
#include "components/exo/file_helper.h"
#include "components/exo/server/wayland_server_controller.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/drop_data.h"
#include "net/base/filename_util.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/file_system_url.h"
#include "storage/common/file_system/file_system_types.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/base/dragdrop/file_info/file_info.h"

namespace {

constexpr char kMimeTypeArcUriList[] = "application/x-arc-uri-list";
constexpr char kUriListSeparator[] = "\r\n";

bool IsArcWindow(const aura::Window* window) {
  return static_cast<ash::AppType>(window->GetProperty(
             aura::client::kAppType)) == ash::AppType::ARC_APP;
}

storage::FileSystemContext* GetFileSystemContext() {
  // Obtains the primary profile.
  if (!user_manager::UserManager::IsInitialized())
    return nullptr;
  const user_manager::User* primary_user =
      user_manager::UserManager::Get()->GetPrimaryUser();
  if (!primary_user)
    return nullptr;
  Profile* primary_profile =
      chromeos::ProfileHelper::Get()->GetProfileByUser(primary_user);
  if (!primary_profile)
    return nullptr;

  return file_manager::util::GetFileSystemContextForExtensionId(
      primary_profile, file_manager::kFileManagerAppId);
}

void GetFileSystemUrlsFromPickle(
    const base::Pickle& pickle,
    std::vector<storage::FileSystemURL>* file_system_urls) {
  storage::FileSystemContext* file_system_context = GetFileSystemContext();
  if (!file_system_context)
    return;

  std::vector<content::DropData::FileSystemFileInfo> file_system_files;
  if (!content::DropData::FileSystemFileInfo::ReadFileSystemFilesFromPickle(
          pickle, &file_system_files))
    return;

  for (const auto& file_system_file : file_system_files) {
    const storage::FileSystemURL file_system_url =
        file_system_context->CrackURL(file_system_file.url);
    if (file_system_url.is_valid())
      file_system_urls->push_back(file_system_url);
  }
}

void SendArcUrls(exo::FileHelper::SendDataCallback callback,
                 const std::vector<GURL>& urls) {
  std::vector<std::string> lines;
  for (const GURL& url : urls) {
    if (!url.is_valid())
      continue;
    lines.emplace_back(url.spec());
  }
  // Arc requires UTF16 for data.
  base::string16 data =
      base::UTF8ToUTF16(base::JoinString(lines, kUriListSeparator));
  std::move(callback).Run(base::RefCountedString16::TakeString(&data));
}

class ChromeFileHelper : public exo::FileHelper {
 public:
  ChromeFileHelper() = default;
  ~ChromeFileHelper() override = default;

  // exo::FileHelper:
  std::vector<ui::FileInfo> GetFilenames(
      aura::Window* source,
      const std::vector<uint8_t>& data) const override {
    // TODO(crbug.com/1144138): We must translate the path if this was received
    // from a VM. E.g. if this was from crostini as
    // file:///home/username/file.txt, we translate to
    // file:///media/fuse/crostini_<hash>_termina_penguin/file.txt.
    std::string lines(data.begin(), data.end());
    std::vector<ui::FileInfo> filenames;
    // Until we have mapping for other VMs, only accept from arc.
    if (!IsArcWindow(source))
      return filenames;

    base::FilePath path;
    storage::FileSystemURL url;
    for (const base::StringPiece& line : base::SplitStringPiece(
             lines, "\n", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY)) {
      if (!net::FileURLToFilePath(GURL(line), &path))
        continue;
      filenames.emplace_back(ui::FileInfo(path, base::FilePath()));
    }
    return filenames;
  }

  std::string GetMimeTypeForUriList(aura::Window* target) const override {
    return kMimeTypeArcUriList;
  }

  void SendFileInfo(aura::Window* target,
                    const std::vector<ui::FileInfo>& files,
                    exo::FileHelper::SendDataCallback callback) const override {
    // TODO(crbug.com/1144138): Translate path and possibly share files with VM.
    std::vector<std::string> lines;
    GURL url;
    for (const auto& info : files) {
      if (file_manager::util::ConvertPathToArcUrl(info.path, &url)) {
        lines.emplace_back(url.spec());
      }
    }
    base::string16 data =
        base::UTF8ToUTF16(base::JoinString(lines, kUriListSeparator));
    std::move(callback).Run(base::RefCountedString16::TakeString(&data));
  }

  bool HasUrlsInPickle(const base::Pickle& pickle) const override {
    std::vector<storage::FileSystemURL> file_system_urls;
    GetFileSystemUrlsFromPickle(pickle, &file_system_urls);
    return !file_system_urls.empty();
  }

  void SendPickle(aura::Window* target,
                  const base::Pickle& pickle,
                  exo::FileHelper::SendDataCallback callback) override {
    // TODO(crbug.com/1144138): Translate path and possibly share files with VM.
    std::vector<storage::FileSystemURL> file_system_urls;
    GetFileSystemUrlsFromPickle(pickle, &file_system_urls);
    if (file_system_urls.empty()) {
      std::move(callback).Run(nullptr);
      return;
    }
    file_manager::util::ConvertToContentUrls(
        file_system_urls, base::BindOnce(&SendArcUrls, std::move(callback)));
  }
};

}  // namespace

// static
std::unique_ptr<ExoParts> ExoParts::CreateIfNecessary() {
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          ash::switches::kAshEnableWaylandServer)) {
    return nullptr;
  }

  return base::WrapUnique(new ExoParts());
}

ExoParts::~ExoParts() {
  ash::Shell::Get()->UntrackTrackInputMethodBounds(
      ash::ArcInputMethodBoundsTracker::Get());
  wayland_server_.reset();
}

ExoParts::ExoParts()
    : arc_overlay_manager_(std::make_unique<ash::ArcOverlayManager>()) {
  wayland_server_ = exo::WaylandServerController::CreateIfNecessary(
      std::make_unique<ChromeFileHelper>(),
      std::make_unique<ash::ArcNotificationSurfaceManagerImpl>(),
      std::make_unique<ash::ArcInputMethodSurfaceManager>(),
      std::make_unique<ash::ArcToastSurfaceManager>());
  ash::Shell::Get()->TrackInputMethodBounds(
      ash::ArcInputMethodBoundsTracker::Get());
}
