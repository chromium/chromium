// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/exo/chrome_data_exchange_delegate.h"

#include <string>
#include <vector>

#include "ash/components/arc/arc_util.h"
#include "ash/public/cpp/app_types_util.h"
#include "base/logging.h"
#include "base/memory/ref_counted_memory.h"
#include "base/strings/escape.h"
#include "base/strings/strcat.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ash/arc/arc_util.h"
#include "chrome/browser/ash/borealis/borealis_window_manager.h"
#include "chrome/browser/ash/crosapi/browser_util.h"
#include "chrome/browser/ash/crostini/crostini_util.h"
#include "chrome/browser/ash/extensions/file_manager/event_router.h"
#include "chrome/browser/ash/extensions/file_manager/event_router_factory.h"
#include "chrome/browser/ash/file_manager/app_id.h"
#include "chrome/browser/ash/file_manager/fileapi_util.h"
#include "chrome/browser/ash/file_manager/path_util.h"
#include "chrome/browser/ash/guest_os/guest_os_session_tracker.h"
#include "chrome/browser/ash/guest_os/guest_os_share_path.h"
#include "chrome/browser/ash/plugin_vm/plugin_vm_files.h"
#include "chrome/browser/ash/plugin_vm/plugin_vm_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "content/public/common/drop_data.h"
#include "storage/browser/file_system/external_mount_points.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/file_system_url.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "ui/aura/window.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/clipboard/clipboard_buffer.h"
#include "ui/base/clipboard/custom_data_helper.h"
#include "ui/base/clipboard/file_info.h"
#include "ui/base/data_transfer_policy/data_transfer_endpoint.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "url/gurl.h"

namespace ash {

namespace {

constexpr char kMimeTypeArcUriList[] = "application/x-arc-uri-list";
constexpr char kMimeTypeTextUriList[] = "text/uri-list";
constexpr char kUriListSeparator[] = "\r\n";
constexpr char kVmFileScheme[] = "vmfile";

// Mime types used in FilesApp to copy/paste files to clipboard.
constexpr char16_t kFilesAppMimeSources[] = u"fs/sources";
constexpr char16_t kFilesAppSeparator16[] = u"\n";

storage::FileSystemContext* GetFileSystemContext() {
  Profile* primary_profile = ProfileManager::GetPrimaryUserProfile();
  if (!primary_profile)
    return nullptr;

  return file_manager::util::GetFileManagerFileSystemContext(primary_profile);
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
        file_system_context->CrackURLInFirstPartyContext(file_system_file.url);
    if (file_system_url.is_valid())
      file_system_urls->push_back(std::move(file_system_url));
  }
}

void SendArcUrls(exo::DataExchangeDelegate::SendDataCallback callback,
                 const std::vector<GURL>& urls) {
  std::vector<std::string> lines;
  for (const GURL& url : urls) {
    if (!url.is_valid())
      continue;
    lines.push_back(url.spec());
  }
  // Arc requires UTF16 for data.
  std::move(callback).Run(base::MakeRefCounted<base::RefCountedString16>(
      base::UTF8ToUTF16(base::JoinString(lines, kUriListSeparator))));
}

void SendAfterShare(ui::EndpointType target,
                    exo::DataExchangeDelegate::SendDataCallback callback,
                    std::vector<std::string> file_urls) {
  std::string joined = base::JoinString(file_urls, kUriListSeparator);
  scoped_refptr<base::RefCountedMemory> data;
  if (target == ui::EndpointType::kArc) {
    // Arc uses utf-16 data.
    data = base::MakeRefCounted<base::RefCountedString16>(
        base::UTF8ToUTF16(joined));
  } else {
    data = base::MakeRefCounted<base::RefCountedString>(std::move(joined));
  }

  std::move(callback).Run(data);
}

struct FileInfo {
  base::FilePath path;
  storage::FileSystemURL url;
};

// Returns true if path is shared with the specified VM, or for crostini if path
// is homedir or dir within.
bool IsPathShared(Profile* profile,
                  std::string vm_name,
                  bool is_crostini,
                  base::FilePath path) {
  auto* share_path = guest_os::GuestOsSharePath::GetForProfile(profile);
  if (share_path->IsPathShared(vm_name, path)) {
    return true;
  }
  if (is_crostini) {
    base::FilePath mount =
        file_manager::util::GetCrostiniMountDirectory(profile);
    return path == mount || mount.IsParent(path);
  }
  return false;
}

// Translate |vm_paths| from |source| VM to host paths.
std::vector<FileInfo> TranslateVMToHost(ui::EndpointType source,
                                        std::vector<ui::FileInfo> vm_paths) {
  std::vector<FileInfo> file_info;
  Profile* primary_profile = ProfileManager::GetPrimaryUserProfile();
  bool is_crostini = source == ui::EndpointType::kCrostini;
  bool is_plugin_vm = source == ui::EndpointType::kPluginVm;

  base::FilePath vm_mount;
  std::string vm_name;
  if (is_crostini) {
    vm_mount = crostini::ContainerChromeOSBaseDirectory();
    vm_name = crostini::kCrostiniDefaultVmName;
  } else if (is_plugin_vm) {
    vm_mount = plugin_vm::ChromeOSBaseDirectory();
    vm_name = plugin_vm::kPluginVmName;
  }

  for (ui::FileInfo& info : vm_paths) {
    base::FilePath path = std::move(info.path);
    storage::FileSystemURL url;

    // Convert the VM path to a path in the host if possible (in homedir or
    // /mnt/chromeos for crostini; in //ChromeOS for Plugin VM), otherwise
    // prefix with 'vmfile:<vm_name>:' to avoid VMs spoofing host paths.
    // E.g. crostini /etc/mime.types => vmfile:termina:/etc/mime.types.
    if (is_crostini || is_plugin_vm) {
      if (file_manager::util::ConvertPathInsideVMToFileSystemURL(
              primary_profile, path, vm_mount,
              /*map_crostini_home=*/is_crostini, &url)) {
        path = url.path();
        // Only allow write to clipboard for paths that are shared.
        if (!IsPathShared(primary_profile, vm_name, is_crostini, path)) {
          LOG(WARNING) << "Unshared file path: " << path;
          continue;
        }
      } else {
        path = base::FilePath(
            base::StrCat({kVmFileScheme, ":", vm_name, ":", path.value()}));
      }
    }
    file_info.push_back({std::move(path), std::move(url)});
  }
  return file_info;
}

// Crack paths and get FileSystemURL.
std::vector<FileInfo> CrackPaths(std::vector<base::FilePath> paths) {
  storage::ExternalMountPoints* mount_points =
      storage::ExternalMountPoints::GetSystemInstance();
  base::FilePath virtual_path;
  std::vector<FileInfo> file_info;

  for (auto& path : paths) {
    // Convert absolute host path to FileSystemURL if possible.
    storage::FileSystemURL url;
    if (mount_points->GetVirtualPath(path, &virtual_path)) {
      url = mount_points->CreateCrackedFileSystemURL(
          blink::StorageKey(), storage::kFileSystemTypeExternal, virtual_path);
    }
    file_info.push_back({std::move(path), std::move(url)});
  }
  return file_info;
}

// Share |files| with |target| VM and invoke |callback| with translated file:
// URLs.
void ShareAndTranslateHostToVM(
    ui::EndpointType target,
    std::vector<FileInfo> files,
    base::OnceCallback<void(std::vector<std::string>)> callback) {
  Profile* primary_profile = ProfileManager::GetPrimaryUserProfile();
  bool is_arc = target == ui::EndpointType::kArc;
  bool is_crostini = target == ui::EndpointType::kCrostini;
  bool is_plugin_vm = target == ui::EndpointType::kPluginVm;

  base::FilePath vm_mount;
  std::string vm_name;
  if (is_arc) {
    // For ARC, |share_required| below will always be false and |vm_name| will
    // not be used. Setting it to arc::kArcVmName has no effect.
    vm_name = arc::kArcVmName;
  } else if (is_crostini) {
    vm_mount = crostini::ContainerChromeOSBaseDirectory();
    vm_name = crostini::kCrostiniDefaultVmName;
  } else if (is_plugin_vm) {
    vm_mount = plugin_vm::ChromeOSBaseDirectory();
    vm_name = plugin_vm::kPluginVmName;
  }

  const std::string vm_prefix =
      base::StrCat({kVmFileScheme, ":", vm_name, ":"});
  std::vector<std::string> file_urls;
  auto* share_path = guest_os::GuestOsSharePath::GetForProfile(primary_profile);
  std::vector<base::FilePath> paths_to_share;

  for (auto& info : files) {
    std::string file_url;
    bool share_required = false;
    if (is_arc) {
      GURL arc_url;
      if (!file_manager::util::ConvertPathToArcUrl(info.path, &arc_url,
                                                   &share_required)) {
        LOG(WARNING) << "Could not convert arc path " << info.path;
        continue;
      }
      file_url = arc_url.spec();
    } else if (is_crostini || is_plugin_vm) {
      base::FilePath path;
      // Check if it is a path inside the VM: 'vmfile:<vm_name>:'.
      if (base::StartsWith(info.path.value(), vm_prefix,
                           base::CompareCase::SENSITIVE)) {
        file_url = ui::FilePathToFileURL(
            base::FilePath(info.path.value().substr(vm_prefix.size())));
      } else if (file_manager::util::ConvertFileSystemURLToPathInsideVM(
                     primary_profile, info.url, vm_mount,
                     /*map_crostini_home=*/is_crostini, &path)) {
        // Convert to path inside the VM.
        file_url = ui::FilePathToFileURL(path);
        share_required = true;
      } else {
        LOG(WARNING) << "Could not convert into VM path " << info.path;
        continue;
      }
    } else {
      // Use path without conversion as default.
      file_url = ui::FilePathToFileURL(info.path);
    }
    file_urls.push_back(std::move(file_url));
    if (share_required && !share_path->IsPathShared(vm_name, info.path))
      paths_to_share.push_back(std::move(info.path));
  }

  if (!paths_to_share.empty()) {
    if (!is_plugin_vm) {
      auto vm_info =
          guest_os::GuestOsSessionTracker::GetForProfile(primary_profile)
              ->GetVmInfo(vm_name);
      if (!vm_info) {
        // VM must be running for copy-paste or drag-drop to be happening so
        // something's gone wrong, skip trying to share and just send the data.
        std::move(callback).Run(std::move(file_urls));
        return;
      }
      share_path->SharePaths(
          vm_name, vm_info->seneschal_server_handle(),
          std::move(paths_to_share),
          base::BindOnce(
              [](base::OnceCallback<void(std::vector<std::string>)> callback,
                 std::vector<std::string> file_urls, bool success,
                 const std::string& failure_reason) {
                if (!success)
                  LOG(ERROR) << "Error sharing paths: " << failure_reason;

                // Still send the data, even if sharing failed.
                std::move(callback).Run(file_urls);
              },
              std::move(callback), std::move(file_urls)));
      return;
    }

    // Show FilesApp move-to-windows-files dialog when Plugin VM is not shared.
    if (auto* event_router =
            file_manager::EventRouterFactory::GetForProfile(primary_profile)) {
      event_router->DropFailedPluginVmDirectoryNotShared();
    }
    file_urls.clear();
  }

  std::move(callback).Run(std::move(file_urls));
}

}  // namespace

std::vector<base::FilePath> TranslateVMPathsToHost(
    ui::EndpointType source,
    const std::vector<ui::FileInfo>& vm_paths) {
  std::vector<FileInfo> translated = TranslateVMToHost(source, vm_paths);
  std::vector<base::FilePath> result;
  for (auto& info : translated)
    result.push_back(std::move(info.path));
  return result;
}

void ShareWithVMAndTranslateToFileUrls(
    ui::EndpointType target,
    const std::vector<base::FilePath>& files,
    base::OnceCallback<void(std::vector<std::string>)> callback) {
  ShareAndTranslateHostToVM(target, CrackPaths(files), std::move(callback));
}

ChromeDataExchangeDelegate::ChromeDataExchangeDelegate() = default;

ChromeDataExchangeDelegate::~ChromeDataExchangeDelegate() = default;

ui::EndpointType ChromeDataExchangeDelegate::GetDataTransferEndpointType(
    aura::Window* target) const {
  auto* top_level_window = target->GetToplevelWindow();

  if (IsArcWindow(top_level_window))
    return ui::EndpointType::kArc;

  if (borealis::BorealisWindowManager::IsBorealisWindow(top_level_window))
    return ui::EndpointType::kBorealis;

  if (crosapi::browser_util::IsLacrosWindow(top_level_window))
    return ui::EndpointType::kLacros;

  if (crostini::IsCrostiniWindow(top_level_window))
    return ui::EndpointType::kCrostini;

  if (plugin_vm::IsPluginVmAppWindow(top_level_window))
    return ui::EndpointType::kPluginVm;

  return ui::EndpointType::kUnknownVm;
}

std::vector<ui::FileInfo> ChromeDataExchangeDelegate::GetFilenames(
    ui::EndpointType source,
    const std::vector<uint8_t>& data) const {
  std::vector<ui::FileInfo> result;
  std::vector<FileInfo> file_info = TranslateVMToHost(
      source, ui::URIListToFileInfos(std::string(data.begin(), data.end())));
  for (auto& info : file_info)
    result.push_back(ui::FileInfo(std::move(info.path), base::FilePath()));
  return result;
}

std::string ChromeDataExchangeDelegate::GetMimeTypeForUriList(
    ui::EndpointType target) const {
  return target == ui::EndpointType::kArc ? kMimeTypeArcUriList
                                          : kMimeTypeTextUriList;
}

void ChromeDataExchangeDelegate::SendFileInfo(
    ui::EndpointType target,
    const std::vector<ui::FileInfo>& files,
    SendDataCallback callback) const {
  std::vector<base::FilePath> paths;
  for (const auto& file : files)
    paths.push_back(file.path);

  ShareAndTranslateHostToVM(
      target, CrackPaths(std::move(paths)),
      base::BindOnce(&SendAfterShare, target, std::move(callback)));
}

bool ChromeDataExchangeDelegate::HasUrlsInPickle(
    const base::Pickle& pickle) const {
  std::vector<storage::FileSystemURL> file_system_urls;
  GetFileSystemUrlsFromPickle(pickle, &file_system_urls);
  return !file_system_urls.empty();
}

void ChromeDataExchangeDelegate::SendPickle(ui::EndpointType target,
                                            const base::Pickle& pickle,
                                            SendDataCallback callback) {
  std::vector<storage::FileSystemURL> file_system_urls;
  GetFileSystemUrlsFromPickle(pickle, &file_system_urls);

  // ARC FileSystemURLs are converted to Content URLs.
  if (target == ui::EndpointType::kArc) {
    if (file_system_urls.empty()) {
      std::move(callback).Run(nullptr);
      return;
    }
    arc::ConvertToContentUrlsAndShare(
        ProfileManager::GetPrimaryUserProfile(), file_system_urls,
        base::BindOnce(&SendArcUrls, std::move(callback)));
    return;
  }

  std::vector<FileInfo> list;
  for (auto& url : file_system_urls) {
    base::FilePath path = url.path();
    list.push_back({std::move(path), std::move(url)});
  }

  ShareAndTranslateHostToVM(
      target, std::move(list),
      base::BindOnce(&SendAfterShare, target, std::move(callback)));
}

std::vector<ui::FileInfo> ChromeDataExchangeDelegate::ParseFileSystemSources(
    const ui::DataTransferEndpoint* source,
    const base::Pickle& pickle) const {
  std::vector<ui::FileInfo> file_info;
  // We only promote 'fs/sources' custom data pickle to be filenames which can
  // be shared and read by clients if it came from the trusted FilesApp.
  if (!source || !source->GetURL() ||
      !file_manager::util::IsFileManagerURL(*source->GetURL())) {
    return file_info;
  }

  std::u16string file_system_url_list;
  ui::ReadCustomDataForType(pickle.data(), pickle.size(), kFilesAppMimeSources,
                            &file_system_url_list);
  if (file_system_url_list.empty())
    return file_info;

  storage::ExternalMountPoints* mount_points =
      storage::ExternalMountPoints::GetSystemInstance();

  for (const base::StringPiece16& line : base::SplitStringPiece(
           file_system_url_list, kFilesAppSeparator16, base::TRIM_WHITESPACE,
           base::SPLIT_WANT_NONEMPTY)) {
    if (line.empty() || line[0] == '#')
      continue;
    const GURL gurl(line);
    storage::FileSystemURL url = mount_points->CrackURL(
        gurl, blink::StorageKey(url::Origin::Create(gurl)));
    if (!url.is_valid()) {
      LOG(WARNING) << "Invalid clipboard FileSystemURL: " << line;
      continue;
    }
    file_info.push_back(ui::FileInfo(std::move(url.path()), base::FilePath()));
  }
  return file_info;
}

}  // namespace ash
