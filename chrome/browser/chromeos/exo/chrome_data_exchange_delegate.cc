// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/exo/chrome_data_exchange_delegate.h"

#include <string>
#include <vector>

#include "ash/public/cpp/app_types.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/memory/ref_counted_memory.h"
#include "base/strings/escape.h"
#include "base/strings/strcat.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/chromeos/borealis/borealis_window_manager.h"
#include "chrome/browser/chromeos/crostini/crostini_util.h"
#include "chrome/browser/chromeos/extensions/file_manager/event_router.h"
#include "chrome/browser/chromeos/extensions/file_manager/event_router_factory.h"
#include "chrome/browser/chromeos/file_manager/app_id.h"
#include "chrome/browser/chromeos/file_manager/fileapi_util.h"
#include "chrome/browser/chromeos/file_manager/path_util.h"
#include "chrome/browser/chromeos/guest_os/guest_os_share_path.h"
#include "chrome/browser/chromeos/plugin_vm/plugin_vm_files.h"
#include "chrome/browser/chromeos/plugin_vm/plugin_vm_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "content/public/common/drop_data.h"
#include "storage/browser/file_system/external_mount_points.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/file_system_url.h"
#include "ui/aura/window.h"
#include "ui/base/data_transfer_policy/data_transfer_endpoint.h"
#include "ui/base/dragdrop/file_info/file_info.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "url/gurl.h"

namespace chromeos {

namespace {

constexpr char kMimeTypeArcUriList[] = "application/x-arc-uri-list";
constexpr char kMimeTypeTextUriList[] = "text/uri-list";
constexpr char kFileSchemePrefix[] = "file:";
constexpr char kUriListSeparator[] = "\r\n";
constexpr char kVmFileScheme[] = "vmfile";

// We implement our own URLToPath() and PathToURL() rather than use
// net::FileUrlToFilePath() or net::FilePathToFileURL() since //net code does
// not support Windows network paths such as //ChromeOS/MyFiles on OS_CHROMEOS.
bool URLToPath(const base::StringPiece& url, std::string* path) {
  if (!base::StartsWith(url, kFileSchemePrefix, base::CompareCase::SENSITIVE))
    return false;

  // Skip slashes after 'file:' if needed:
  //  file://host/path => //host/path
  //  file:///path     => /path
  int path_start = sizeof(kFileSchemePrefix) - 1;
  if (url.size() > path_start + 2 && url[path_start] == '/' &&
      url[path_start + 1] == '/' && url[path_start + 2] == '/') {
    path_start += 2;
  }

  *path = base::UnescapeBinaryURLComponent(url.substr(path_start));
  return true;
}

std::string PathToURL(const std::string& path) {
  std::string url;
  url.reserve(sizeof(kFileSchemePrefix) + 3 + (3 * path.size()));
  url += kFileSchemePrefix;

  // Add slashes after 'file:' if needed:
  //  //host/path    => file://host/path
  //  /absolute/path => file:///absolute/path
  //  relative/path  => file:///relative/path
  if (path.size() > 1 && path[0] == '/' && path[1] == '/') {
    // Do nothing.
  } else if (path.size() > 0 && path[0] == '/') {
    url += "//";
  } else {
    url += "///";
  }

  // Escape special characters `%;#?\ `
  for (auto c : path) {
    if (c == '%' || c == ';' || c == '#' || c == '?' || c == '\\' || c <= ' ') {
      static const char kHexChars[] = "0123456789ABCDEF";
      url += '%';
      url += kHexChars[(c >> 4) & 0xf];
      url += kHexChars[c & 0xf];
    } else {
      url += c;
    }
  }

  return url;
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

void SendArcUrls(exo::DataExchangeDelegate::SendDataCallback callback,
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

void SendAfterShare(exo::DataExchangeDelegate::SendDataCallback callback,
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
                  exo::DataExchangeDelegate::SendDataCallback callback) {
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
    lines_to_send.emplace_back(PathToURL(path_to_send.value()));
  }

  std::string joined = base::JoinString(lines_to_send, kUriListSeparator);
  auto data = base::RefCountedString::TakeString(&joined);
  if (!paths_to_share.empty()) {
    if (!is_plugin_vm) {
      share_path->SharePaths(
          vm_name, std::move(paths_to_share),
          /*persist=*/false,
          base::BindOnce(&SendAfterShare, std::move(callback),
                         std::move(data)));
      return;
    }

    // Show FilesApp move-to-windows-files dialog when Plugin VM is not shared.
    if (auto* event_router =
            file_manager::EventRouterFactory::GetForProfile(primary_profile)) {
      event_router->DropFailedPluginVmDirectoryNotShared();
    }
    std::string empty;
    data = base::RefCountedString::TakeString(&empty);
  }

  std::move(callback).Run(std::move(data));
}

}  // namespace

ChromeDataExchangeDelegate::ChromeDataExchangeDelegate() = default;

ChromeDataExchangeDelegate::~ChromeDataExchangeDelegate() = default;

ui::EndpointType ChromeDataExchangeDelegate::GetDataTransferEndpointType(
    aura::Window* target) const {
  auto* top_level_window = target->GetToplevelWindow();

  if (ash::IsArcWindow(top_level_window))
    return ui::EndpointType::kArc;

  if (borealis::BorealisWindowManager::IsBorealisWindow(top_level_window))
    return ui::EndpointType::kBorealis;

  if (crostini::IsCrostiniWindow(top_level_window))
    return ui::EndpointType::kCrostini;

  if (plugin_vm::IsPluginVmAppWindow(top_level_window))
    return ui::EndpointType::kPluginVm;

  return ui::EndpointType::kUnknownVm;
}

void ChromeDataExchangeDelegate::SetSourceOnOSExchangeData(
    aura::Window* target,
    ui::OSExchangeData* os_exchange_data) const {
  DCHECK(os_exchange_data);

  os_exchange_data->SetSource(std::make_unique<ui::DataTransferEndpoint>(
      GetDataTransferEndpointType(target)));
}

std::vector<ui::FileInfo> ChromeDataExchangeDelegate::GetFilenames(
    aura::Window* source,
    const std::vector<uint8_t>& data) const {
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
  for (const base::StringPiece& line :
       base::SplitStringPiece(lines, kUriListSeparator, base::TRIM_WHITESPACE,
                              base::SPLIT_WANT_NONEMPTY)) {
    if (line.empty() || line[0] == '#')
      continue;
    std::string path_str;
    if (!URLToPath(line, &path_str)) {
      LOG(WARNING) << "Invalid drop file path: " << line;
      continue;
    }
    path = base::FilePath(path_str);

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

std::string ChromeDataExchangeDelegate::GetMimeTypeForUriList(
    aura::Window* target) const {
  return ash::IsArcWindow(target->GetToplevelWindow()) ? kMimeTypeArcUriList
                                                       : kMimeTypeTextUriList;
}

void ChromeDataExchangeDelegate::SendFileInfo(
    aura::Window* target,
    const std::vector<ui::FileInfo>& files,
    SendDataCallback callback) const {
  // ARC converts to ArcUrl and uses utf-16.
  if (ash::IsArcWindow(target->GetToplevelWindow())) {
    std::vector<std::string> lines;
    GURL url;
    for (const auto& info : files) {
      if (file_manager::util::ConvertPathToArcUrl(info.path, &url))
        lines.emplace_back(url.spec());
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

bool ChromeDataExchangeDelegate::HasUrlsInPickle(
    const base::Pickle& pickle) const {
  std::vector<storage::FileSystemURL> file_system_urls;
  GetFileSystemUrlsFromPickle(pickle, &file_system_urls);
  return !file_system_urls.empty();
}

void ChromeDataExchangeDelegate::SendPickle(aura::Window* target,
                                            const base::Pickle& pickle,
                                            SendDataCallback callback) {
  std::vector<storage::FileSystemURL> file_system_urls;
  GetFileSystemUrlsFromPickle(pickle, &file_system_urls);

  // ARC FileSystemURLs are converted to Content URLs.
  if (ash::IsArcWindow(target->GetToplevelWindow())) {
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

}  // namespace chromeos
