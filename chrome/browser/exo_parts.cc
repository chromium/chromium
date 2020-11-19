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
#include "base/strings/strcat.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/chromeos/crostini/crostini_shelf_utils.h"
#include "chrome/browser/chromeos/crostini/crostini_util.h"
#include "chrome/browser/chromeos/file_manager/app_id.h"
#include "chrome/browser/chromeos/file_manager/fileapi_util.h"
#include "chrome/browser/chromeos/file_manager/path_util.h"
#include "chrome/browser/chromeos/guest_os/guest_os_share_path.h"
#include "chrome/browser/chromeos/plugin_vm/plugin_vm_files.h"
#include "chrome/browser/chromeos/plugin_vm/plugin_vm_util.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/chrome_switches.h"
#include "components/exo/file_helper.h"
#include "components/exo/server/wayland_server_controller.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/drop_data.h"
#include "net/base/filename_util.h"
#include "storage/browser/file_system/external_mount_points.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/file_system_url.h"
#include "storage/common/file_system/file_system_types.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/base/dragdrop/file_info/file_info.h"

namespace {

constexpr char kMimeTypeArcUriList[] = "application/x-arc-uri-list";
constexpr char kMimeTypeTextUriList[] = "text/uri-list";
constexpr char kUriListSeparator[] = "\r\n";
constexpr char kVmFileScheme[] = "vmfile";

bool IsArcWindow(const aura::Window* window) {
  return static_cast<ash::AppType>(window->GetProperty(
             aura::client::kAppType)) == ash::AppType::ARC_APP;
}

storage::FileSystemContext* GetFileSystemContext() {
  Profile* primary_profile = ProfileManager::GetPrimaryUserProfile();
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

void SendAfterShare(exo::FileHelper::SendDataCallback callback,
                    scoped_refptr<base::RefCountedMemory> data,
                    bool success,
                    const std::string& failure_reason) {
  if (!success)
    LOG(ERROR) << "Error sharing paths for drag and drop: " << failure_reason;

  // Still send the data, even if sharing failed.
  std::move(callback).Run(data);
}

struct FileInfo {
  base::FilePath path;
  storage::FileSystemURL url;
};

void ShareAndSend(aura::Window* target,
                  std::vector<FileInfo> files,
                  exo::FileHelper::SendDataCallback callback) {
  Profile* primary_profile = ProfileManager::GetPrimaryUserProfile();

  aura::Window* toplevel = target->GetToplevelWindow();
  bool is_crostini = crostini::IsCrostiniWindow(toplevel);
  bool is_plugin_vm = plugin_vm::IsPluginVmAppWindow(toplevel);

  base::FilePath vm_mount;
  std::string vm_name;
  if (is_crostini) {
    vm_mount = crostini::ContainerChromeOSBaseDirectory();
    vm_name = crostini::kCrostiniDefaultVmName;
  } else if (is_plugin_vm) {
    vm_mount = plugin_vm::ChromeOSBaseDirectory();
    vm_name = plugin_vm::kPluginVmName;
  }

  const std::string vm_prefix =
      base::StrCat({kVmFileScheme, ":", vm_name, ":"});
  std::vector<std::string> lines_to_send;
  auto* share_path = guest_os::GuestOsSharePath::GetForProfile(primary_profile);
  std::vector<base::FilePath> paths_to_share;

  for (auto& info : files) {
    base::FilePath path_to_send = info.path;
    if (is_crostini || is_plugin_vm) {
      // Check if it is a path inside the VM: 'vmfile:<vm_name>:'.
      if (base::StartsWith(info.path.value(), vm_prefix,
                           base::CompareCase::SENSITIVE)) {
        path_to_send =
            base::FilePath(info.path.value().substr(vm_prefix.size()));
      } else if (file_manager::util::ConvertFileSystemURLToPathInsideVM(
                     primary_profile, info.url, vm_mount,
                     /*map_crostini_home=*/is_crostini, &path_to_send)) {
        // Convert to path inside the VM and check if the path needs sharing.
        if (!share_path->IsPathShared(vm_name, info.path))
          paths_to_share.emplace_back(info.path);
      } else {
        LOG(WARNING) << "Could not convert path " << info.path;
        continue;
      }
    }
    lines_to_send.emplace_back(net::FilePathToFileURL(path_to_send).spec());
  }

  std::string joined = base::JoinString(lines_to_send, kUriListSeparator);
  auto data = base::RefCountedString::TakeString(&joined);
  if (!paths_to_share.empty()) {
    share_path->SharePaths(
        vm_name, std::move(paths_to_share),
        /*persist=*/false,
        base::BindOnce(&SendAfterShare, std::move(callback), std::move(data)));
  } else {
    std::move(callback).Run(std::move(data));
  }
}

class ChromeFileHelper : public exo::FileHelper {
 public:
  ChromeFileHelper() = default;
  ~ChromeFileHelper() override = default;

  // exo::FileHelper:
  std::vector<ui::FileInfo> GetFilenames(
      aura::Window* source,
      const std::vector<uint8_t>& data) const override {
    Profile* primary_profile = ProfileManager::GetPrimaryUserProfile();
    aura::Window* toplevel = source->GetToplevelWindow();
    bool is_crostini = crostini::IsCrostiniWindow(toplevel);
    bool is_plugin_vm = plugin_vm::IsPluginVmAppWindow(toplevel);

    base::FilePath vm_mount;
    std::string vm_name;
    if (is_crostini) {
      vm_mount = crostini::ContainerChromeOSBaseDirectory();
      vm_name = crostini::kCrostiniDefaultVmName;
    } else if (is_plugin_vm) {
      vm_mount = plugin_vm::ChromeOSBaseDirectory();
      vm_name = plugin_vm::kPluginVmName;
    }

    std::string lines(data.begin(), data.end());
    std::vector<ui::FileInfo> filenames;

    base::FilePath path;
    storage::FileSystemURL url;
    for (const base::StringPiece& line : base::SplitStringPiece(
             lines, "\n", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY)) {
      if (!net::FileURLToFilePath(GURL(line), &path))
        continue;

      // Convert the VM path to a path in the host if possible (in homedir or
      // /mnt/chromeos for crostini; in //ChromeOS for Plugin VM), otherwise
      // prefix with 'vmfile:<vm_name>:' to avoid VMs spoofing host paths.
      // E.g. crostini /etc/mime.types => vmfile:termina:/etc/mime.types.
      if (is_crostini || is_plugin_vm) {
        if (file_manager::util::ConvertPathInsideVMToFileSystemURL(
                primary_profile, path, vm_mount,
                /*map_crostini_home=*/is_crostini, &url)) {
          path = url.path();
        } else {
          path = base::FilePath(
              base::StrCat({kVmFileScheme, ":", vm_name, ":", path.value()}));
        }
      }
      filenames.emplace_back(ui::FileInfo(path, base::FilePath()));
    }
    return filenames;
  }

  std::string GetMimeTypeForUriList(aura::Window* target) const override {
    return IsArcWindow(target->GetToplevelWindow()) ? kMimeTypeArcUriList
                                                    : kMimeTypeTextUriList;
  }

  void SendFileInfo(aura::Window* target,
                    const std::vector<ui::FileInfo>& files,
                    exo::FileHelper::SendDataCallback callback) const override {
    // ARC converts to ArcUrl and uses utf-16.
    if (IsArcWindow(target->GetToplevelWindow())) {
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
      return;
    }

    storage::ExternalMountPoints* mount_points =
        storage::ExternalMountPoints::GetSystemInstance();
    base::FilePath virtual_path;
    std::vector<FileInfo> list;

    for (const auto& info : files) {
      // Convert absolute host path to FileSystemURL if possible.
      storage::FileSystemURL url;
      if (mount_points->GetVirtualPath(info.path, &virtual_path)) {
        url = mount_points->CreateCrackedFileSystemURL(
            url::Origin(), storage::kFileSystemTypeExternal, virtual_path);
      }
      list.push_back({info.path, std::move(url)});
    }

    ShareAndSend(target, std::move(list), std::move(callback));
  }

  bool HasUrlsInPickle(const base::Pickle& pickle) const override {
    std::vector<storage::FileSystemURL> file_system_urls;
    GetFileSystemUrlsFromPickle(pickle, &file_system_urls);
    return !file_system_urls.empty();
  }

  void SendPickle(aura::Window* target,
                  const base::Pickle& pickle,
                  exo::FileHelper::SendDataCallback callback) override {
    std::vector<storage::FileSystemURL> file_system_urls;
    GetFileSystemUrlsFromPickle(pickle, &file_system_urls);

    // ARC FileSystemURLs are converted to Content URLs.
    if (IsArcWindow(target->GetToplevelWindow())) {
      if (file_system_urls.empty()) {
        std::move(callback).Run(nullptr);
        return;
      }
      file_manager::util::ConvertToContentUrls(
          file_system_urls, base::BindOnce(&SendArcUrls, std::move(callback)));
      return;
    }

    std::vector<FileInfo> list;
    for (const auto& url : file_system_urls)
      list.push_back({url.path(), url});

    ShareAndSend(target, std::move(list), std::move(callback));
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
