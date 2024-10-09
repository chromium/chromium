// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/exo/chrome_security_delegate.h"

#include <memory>
#include <string>

#include "ash/components/arc/arc_util.h"
#include "ash/public/cpp/app_types_util.h"
#include "base/memory/ref_counted_memory.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ash/arc/arc_util.h"
#include "chrome/browser/ash/crostini/crostini_util.h"
#include "chrome/browser/ash/exo/chrome_data_exchange_delegate.h"
#include "chrome/browser/ash/extensions/file_manager/event_router.h"
#include "chrome/browser/ash/extensions/file_manager/event_router_factory.h"
#include "chrome/browser/ash/file_manager/fileapi_util.h"
#include "chrome/browser/ash/file_manager/path_util.h"
#include "chrome/browser/ash/fusebox/fusebox_server.h"
#include "chrome/browser/ash/guest_os/guest_os_session_tracker.h"
#include "chrome/browser/ash/guest_os/guest_os_session_tracker_factory.h"
#include "chrome/browser/ash/guest_os/guest_os_share_path.h"
#include "chrome/browser/ash/guest_os/guest_os_share_path_factory.h"
#include "chrome/browser/ash/plugin_vm/plugin_vm_files.h"
#include "chrome/browser/ash/plugin_vm/plugin_vm_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/exo/shell_surface_util.h"
#include "content/public/common/drop_data.h"
#include "storage/browser/file_system/external_mount_points.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/file_system_url.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "ui/base/clipboard/file_info.h"
#include "ui/base/data_transfer_policy/data_transfer_endpoint.h"

namespace ash {

namespace {

constexpr char kUriListSeparator[] = "\r\n";
constexpr char kVmFileScheme[] = "vmfile";
constexpr char kDefaultVmMount[] = "/mnt/shared";

void SendArcUrls(exo::SecurityDelegate::SendDataCallback callback,
                 const std::vector<GURL>& urls) {
  std::vector<std::string> lines;
  for (const GURL& url : urls) {
    if (!url.is_valid()) {
      continue;
    }
    lines.push_back(url.spec());
  }
  // Arc requires UTF16 for data.
  std::move(callback).Run(base::MakeRefCounted<base::RefCountedString16>(
      base::UTF8ToUTF16(base::JoinString(lines, kUriListSeparator))));
}

void SendAfterShare(ui::EndpointType target,
                    exo::SecurityDelegate::SendDataCallback callback,
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
  const base::FilePath path;
  const storage::FileSystemURL url;
};

// Returns true if path is shared with the specified VM, or for crostini if path
// is homedir or dir within.
bool IsPathShared(Profile* profile,
                  std::string vm_name,
                  bool is_crostini,
                  base::FilePath path) {
  auto* share_path = guest_os::GuestOsSharePathFactory::GetForProfile(profile);
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

// Return VM mount path for specified `vm_name`.
base::FilePath GetVmMount(const std::string& vm_name) {
  if (vm_name == crostini::kCrostiniDefaultVmName) {
    return crostini::ContainerChromeOSBaseDirectory();
  }
  if (vm_name == plugin_vm::kPluginVmName) {
    return plugin_vm::ChromeOSBaseDirectory();
  }
  return base::FilePath(std::string(kDefaultVmMount));
}

// Translate |vm_paths| from |source| VM to host paths.
std::vector<FileInfo> TranslateVMToHost(const std::string vm_name,
                                        std::vector<ui::FileInfo> vm_paths) {
  std::vector<FileInfo> file_infos;
  Profile* primary_profile = ProfileManager::GetPrimaryUserProfile();
  bool is_crostini = vm_name == crostini::kCrostiniDefaultVmName;

  for (ui::FileInfo& info : vm_paths) {
    base::FilePath path = std::move(info.path);
    storage::FileSystemURL url;

    // Convert the VM path to a path in the host if possible (in homedir or
    // /mnt/chromeos for crostini; in //ChromeOS for Plugin VM), otherwise
    // prefix with 'vmfile:<vm_name>:' to avoid VMs spoofing host paths.
    // E.g. crostini /etc/mime.types => vmfile:termina:/etc/mime.types.
    if (!vm_name.empty() && vm_name != arc::kArcVmName) {
      if (file_manager::util::ConvertPathInsideVMToFileSystemURL(
              primary_profile, path, GetVmMount(vm_name),
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
    file_infos.push_back({std::move(path), std::move(url)});
  }
  return file_infos;
}

// Crack paths and get FileSystemURL.
std::vector<FileInfo> CrackPaths(std::vector<base::FilePath> paths) {
  storage::ExternalMountPoints* mount_points =
      storage::ExternalMountPoints::GetSystemInstance();
  base::FilePath virtual_path;
  std::vector<FileInfo> file_infos;

  for (auto& path : paths) {
    // Convert absolute host path to FileSystemURL if possible.
    storage::FileSystemURL url;
    if (mount_points->GetVirtualPath(path, &virtual_path)) {
      url = mount_points->CreateCrackedFileSystemURL(
          blink::StorageKey(), storage::kFileSystemTypeExternal, virtual_path);
    }
    file_infos.push_back({std::move(path), std::move(url)});
  }
  return file_infos;
}

// Share |files| with |target| VM and invoke |callback| with translated file:
// URLs.
void ShareAndTranslateHostToVM(
    const std::string& vm_name,
    const std::vector<FileInfo>& file_infos,
    base::OnceCallback<void(std::vector<std::string>)> callback) {
  Profile* primary_profile = ProfileManager::GetPrimaryUserProfile();
  bool is_crostini = vm_name == crostini::kCrostiniDefaultVmName;

  const std::string vm_prefix =
      base::StrCat({kVmFileScheme, ":", vm_name, ":"});
  std::vector<std::string> file_urls;
  auto* share_path =
      guest_os::GuestOsSharePathFactory::GetForProfile(primary_profile);
  std::vector<base::FilePath> paths_to_share;

  for (auto& info : file_infos) {
    std::string file_url;
    bool share_required = false;
    if (vm_name == arc::kArcVmName) {
      GURL arc_url;
      if (!file_manager::util::ConvertPathToArcUrl(info.path, &arc_url,
                                                   &share_required)) {
        LOG(WARNING) << "Could not convert arc path " << info.path;
        continue;
      }
      file_url = arc_url.spec();
    } else if (!vm_name.empty()) {
      base::FilePath path;
      // Check if it is a path inside the VM: 'vmfile:<vm_name>:'.
      if (base::StartsWith(info.path.value(), vm_prefix,
                           base::CompareCase::SENSITIVE)) {
        file_url = ui::FilePathToFileURL(
            base::FilePath(info.path.value().substr(vm_prefix.size())));
      } else if (file_manager::util::ConvertFileSystemURLToPathInsideVM(
                     primary_profile, info.url, GetVmMount(vm_name),
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
    if (share_required && !share_path->IsPathShared(vm_name, info.path)) {
      paths_to_share.push_back(std::move(info.path));
    }
  }

  if (!paths_to_share.empty()) {
    if (vm_name != plugin_vm::kPluginVmName) {
      auto vm_info =
          guest_os::GuestOsSessionTrackerFactory::GetForProfile(primary_profile)
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
                if (!success) {
                  LOG(ERROR) << "Error sharing paths: " << failure_reason;
                }

                // Still send the data, even if sharing failed.
                std::move(callback).Run(std::move(file_urls));
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

// static
std::vector<base::FilePath> TranslateVMPathsToHost(
    const std::string& vm_name,
    const std::vector<ui::FileInfo>& vm_paths) {
  std::vector<FileInfo> translated = TranslateVMToHost(vm_name, vm_paths);
  std::vector<base::FilePath> result;
  for (auto& info : translated) {
    result.push_back(std::move(info.path));
  }
  return result;
}

// static
void ShareWithVMAndTranslateToFileUrls(
    const std::string& vm_name,
    const std::vector<base::FilePath>& files,
    base::OnceCallback<void(std::vector<std::string>)> callback) {
  ShareAndTranslateHostToVM(vm_name, CrackPaths(files), std::move(callback));
}

ChromeSecurityDelegate::ChromeSecurityDelegate() = default;

ChromeSecurityDelegate::~ChromeSecurityDelegate() = default;

bool ChromeSecurityDelegate::CanSelfActivate(aura::Window* window) const {
  // TODO(b/233691818): The default should be "false", and clients should
  // override that if they need to self-activate.
  //
  // Unfortunately, several clients don't have their own SecurityDelegate yet,
  // so we will continue to use the old exo::Permissions stuff until they do.
  return exo::HasPermissionToActivate(window);
}

bool ChromeSecurityDelegate::CanLockPointer(aura::Window* window) const {
  // TODO(b/200896773): Move this out from exo's default security delegate
  // define in client's security delegates.
  return ash::IsArcWindow(window) || ash::IsLacrosWindow(window);
}

ChromeSecurityDelegate::SetBoundsPolicy ChromeSecurityDelegate::CanSetBounds(
    aura::Window* window) const {
  // TODO(b/200896773): Move into LacrosSecurityDelegate when it exists.
  if (ash::IsLacrosWindow(window)) {
    return SetBoundsPolicy::DCHECK_IF_DECORATED;
  } else if (ash::IsArcWindow(window)) {
    // TODO(b/285252684): Move into ArcSecurityDelegate when it exists.
    return SetBoundsPolicy::ADJUST_IF_DECORATED;
  } else {
    return SetBoundsPolicy::IGNORE;
  }
}

std::vector<ui::FileInfo> ChromeSecurityDelegate::GetFilenames(
    ui::EndpointType source,
    const std::vector<uint8_t>& data) const {
  std::vector<ui::FileInfo> result;
  std::vector<FileInfo> file_infos = TranslateVMToHost(
      GetVmName(source),
      ui::URIListToFileInfos(std::string(data.begin(), data.end())));
  for (auto& info : file_infos) {
    result.push_back(ui::FileInfo(std::move(info.path), base::FilePath()));
  }
  return result;
}

void ChromeSecurityDelegate::SendFileInfo(
    ui::EndpointType target,
    const std::vector<ui::FileInfo>& files,
    SendDataCallback callback) const {
  std::vector<base::FilePath> paths;
  for (const auto& file : files) {
    paths.push_back(file.path);
  }

  ShareAndTranslateHostToVM(
      GetVmName(target), CrackPaths(std::move(paths)),
      base::BindOnce(&SendAfterShare, target, std::move(callback)));
}

void ChromeSecurityDelegate::SendPickle(ui::EndpointType target,
                                        const base::Pickle& pickle,
                                        SendDataCallback callback) {
  std::vector<storage::FileSystemURL> file_system_urls =
      GetFileSystemUrlsFromPickle(pickle);
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

  std::vector<FileInfo> file_infos;
  for (auto& url : file_system_urls) {
    if (url.TypeImpliesPathIsReal()) {
      base::FilePath path = url.path();
      file_infos.emplace_back(std::move(path), std::move(url));
    } else if (base::FilePath path =
                   fusebox::Server::SubstituteFuseboxFilePath(url);
               !path.empty()) {
      file_infos.emplace_back(std::move(path), std::move(url));
    }
  }

  ShareAndTranslateHostToVM(
      GetVmName(target), std::move(file_infos),
      base::BindOnce(&SendAfterShare, target, std::move(callback)));
}

std::string ChromeSecurityDelegate::GetVmName(ui::EndpointType target) const {
  if (target == ui::EndpointType::kArc) {
    return arc::kArcVmName;
  } else if (target == ui::EndpointType::kPluginVm) {
    return plugin_vm::kPluginVmName;
  }
  return std::string();
}

}  // namespace ash
