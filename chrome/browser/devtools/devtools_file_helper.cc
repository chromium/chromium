// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/devtools_file_helper.h"

#include <set>
#include <vector>

#include "base/base64.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/json/values_util.h"
#include "base/no_destructor.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/devtools/devtools_file_watcher.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/browser/download_manager.h"
#include "content/public/common/content_client.h"
#include "crypto/obsolete/md5.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/shell_dialogs/selected_file_info.h"
#include "url/gurl.h"
#include "url/origin.h"

using content::BrowserThread;
using std::set;

namespace devtools {
std::string Md5OfUrlAsHexForDevTools(std::string_view url) {
  return base::HexEncodeLower(crypto::obsolete::Md5::Hash(url));
}
}  // namespace devtools

namespace {

static const char kAutomaticFileSystemType[] = "automatic";
static const char kDefaultFileSystemType[] = "";

static const char kIllegalPath[] = "<illegal path>";
static const char kIllegalType[] = "<illegal type>";
static const char kPermissionDenied[] = "<permission denied>";
static const char kSelectionCancelled[] = "<selection cancelled>";

base::FilePath& GetLastSavePath() {
  static base::NoDestructor<base::FilePath> last_save_path;
  return *last_save_path;
}

void WriteToFile(const base::FilePath& path,
                 const std::string& content,
                 bool is_base64) {
  DCHECK(!path.empty());

  std::optional<std::vector<uint8_t>> decoded_content;
  if (is_base64) {
    decoded_content = base::Base64Decode(content);
    if (!decoded_content) {
      LOG(ERROR) << "Invalid base64. Not writing " << path;
      return;
    }
  }
  base::span<const uint8_t> content_span =
      decoded_content ? *decoded_content : base::as_byte_span(content);

  base::File file(path,
                  base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE);
  if (!file.IsValid()) {
    LOG(ERROR) << "Failed to open file: " << path.value();
    return;
  }
  if (!file.WriteAndCheck(0, content_span)) {
    LOG(ERROR) << "Failed to write: " << path.value();
    return;
  }
}

void AppendToFile(const base::FilePath& path, const std::string& content) {
  DCHECK(!path.empty());

  base::File file(path, base::File::FLAG_OPEN_ALWAYS | base::File::FLAG_APPEND);
  if (!file.IsValid()) {
    LOG(ERROR) << "Failed to open file: " << path.value();
    return;
  }
  if (!file.WriteAtCurrentPosAndCheck(base::as_byte_span(content))) {
    LOG(ERROR) << "Failed to append: " << path.value();
    return;
  }
}

}  // namespace

DevToolsFileHelper::FileSystem::FileSystem() = default;

DevToolsFileHelper::FileSystem::~FileSystem() = default;

DevToolsFileHelper::FileSystem::FileSystem(const FileSystem& other) = default;

DevToolsFileHelper::FileSystem::FileSystem(const std::string& type,
                                           const std::string& file_system_name,
                                           const std::string& root_url,
                                           const std::string& file_system_path)
    : type(type),
      file_system_name(file_system_name),
      root_url(root_url),
      file_system_path(file_system_path) {}

DevToolsFileHelper::Storage::~Storage() = default;

DevToolsFileHelper::DevToolsFileHelper(Profile* profile,
                                       Delegate* delegate,
                                       Storage* storage)
    : profile_(profile),
      delegate_(delegate),
      storage_(storage),
      file_task_runner_(
          base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()})) {
  pref_change_registrar_.Init(profile_->GetPrefs());
}

DevToolsFileHelper::~DevToolsFileHelper() = default;

void DevToolsFileHelper::Save(const std::string& url,
                              const std::string& content,
                              bool save_as,
                              bool is_base64,
                              SelectFileCallback select_file_callback,
                              SaveCallback save_callback,
                              CanceledCallback canceled_callback) {
  auto it = saved_files_.find(url);
  if (it != saved_files_.end() && !save_as) {
    SaveToFileSelected(url, content, is_base64, std::move(save_callback),
                       ui::SelectedFileInfo(it->second));
    return;
  }

  const base::Value::Dict& file_map =
      profile_->GetPrefs()->GetDict(prefs::kDevToolsEditedFiles);
  base::FilePath initial_path;

  if (const base::Value* path_value =
          file_map.Find(devtools::Md5OfUrlAsHexForDevTools(url))) {
    std::optional<base::FilePath> path = base::ValueToFilePath(*path_value);
    if (path) {
      initial_path = std::move(*path);
    }
  }

  if (initial_path.empty()) {
    GURL gurl(url);
    std::string suggested_file_name;
    if (gurl.is_valid()) {
      url::RawCanonOutputW<1024> unescaped_content;
      std::string escaped_content = gurl.ExtractFileName();
      url::DecodeURLEscapeSequences(escaped_content,
                                    url::DecodeURLMode::kUTF8OrIsomorphic,
                                    &unescaped_content);
      // TODO(crbug.com/40839171): Due to filename encoding on Windows we can't
      // expect to always be able to convert to UTF8 and back
      std::string unescaped_content_string =
          base::UTF16ToUTF8(unescaped_content.view());
      suggested_file_name = unescaped_content_string;
    } else {
      suggested_file_name = url;
    }
    // TODO(crbug.com/40839171): Truncate a UTF8 string in a better way
    if (suggested_file_name.length() > 64) {
      suggested_file_name = suggested_file_name.substr(0, 64);
    }
    // TODO(crbug.com/40839171): Ensure suggested_file_name is an ASCII string
    if (!GetLastSavePath().empty()) {
      initial_path =
          GetLastSavePath().DirName().AppendASCII(suggested_file_name);
    } else {
      base::FilePath download_path =
          DownloadPrefs::FromDownloadManager(profile_->GetDownloadManager())
              ->DownloadPath();
      initial_path = download_path.AppendASCII(suggested_file_name);
    }
  }

  std::move(select_file_callback)
      .Run(base::BindOnce(&DevToolsFileHelper::SaveToFileSelected,
                          weak_factory_.GetWeakPtr(), url, content, is_base64,
                          std::move(save_callback)),
           std::move(canceled_callback), initial_path);
}

void DevToolsFileHelper::Append(const std::string& url,
                                const std::string& content,
                                base::OnceClosure callback) {
  auto it = saved_files_.find(url);
  if (it == saved_files_.end()) {
    return;
  }
  file_task_runner_->PostTaskAndReply(
      FROM_HERE, BindOnce(&AppendToFile, it->second.path(), content),
      std::move(callback));
}

void DevToolsFileHelper::SaveToFileSelected(
    const std::string& url,
    const std::string& content,
    bool is_base64,
    SaveCallback callback,
    const ui::SelectedFileInfo& file_info) {
  GetLastSavePath() = file_info.path();
  saved_files_[url] = file_info;

  ScopedDictPrefUpdate update(profile_->GetPrefs(),
                              prefs::kDevToolsEditedFiles);
  base::Value::Dict& files_map = update.Get();

#if BUILDFLAG(IS_ANDROID)
  // On Android, the selected file path can be a content URL that isn't supposed
  // to be shown to the user. In that case, store the display name instead.
  base::FilePath path_in_prefs = file_info.display_name.empty()
                                     ? file_info.path()
                                     : base::FilePath(file_info.display_name);
#else
  base::FilePath path_in_prefs = file_info.path();
#endif  // BUILDFLAG(IS_ANDROID)
  files_map.Set(devtools::Md5OfUrlAsHexForDevTools(url),
                base::FilePathToValue(path_in_prefs));

  std::string file_system_path = file_info.path().AsUTF8Unsafe();
  // Run 'SaveCallback' only once we have actually written the file, but
  // run it on the current task runner.
  scoped_refptr<base::SequencedTaskRunner> current_task_runner =
      base::SequencedTaskRunner::GetCurrentDefault();
  file_task_runner_->PostTask(
      FROM_HERE, BindOnce(&WriteToFile, file_info.path(), content, is_base64)
                     .Then(base::BindPostTask(
                         current_task_runner,
                         BindOnce(std::move(callback), file_system_path))));
}

void DevToolsFileHelper::AddFileSystem(
    const std::string& type,
    SelectFileCallback select_file_callback,
    const HandlePermissionsCallback& handle_permissions_callback) {
  // Make sure the |type| is not a valid UUID. These are reserved for automatic
  // file systems.
  if (type == kAutomaticFileSystemType ||
      base::Uuid::ParseCaseInsensitive(type).is_valid()) {
    FailedToAddFileSystem(kIllegalType);
    return;
  }

  std::move(select_file_callback)
      .Run(base::BindOnce(&DevToolsFileHelper::InnerAddFileSystem,
                          weak_factory_.GetWeakPtr(),
                          handle_permissions_callback, type),
           base::BindOnce(&DevToolsFileHelper::FailedToAddFileSystem,
                          weak_factory_.GetWeakPtr(), kSelectionCancelled),
           base::FilePath());
}

void DevToolsFileHelper::UpgradeDraggedFileSystemPermissions(
    const std::string& file_system_url,
    const HandlePermissionsCallback& handle_permissions_callback) {
  auto file_system_paths =
      storage_->GetDraggedFileSystemPaths(GURL(file_system_url));
  for (const auto& file_system_path : file_system_paths) {
    InnerAddFileSystem(handle_permissions_callback, kDefaultFileSystemType,
                       ui::SelectedFileInfo(file_system_path));
  }
}

void DevToolsFileHelper::ConnectAutomaticFileSystem(
    const std::string& file_system_path,
    const base::Uuid& file_system_uuid,
    bool add_if_missing,
    const HandlePermissionsCallback& handle_permissions_callback,
    ConnectCallback connect_callback) {
  DCHECK(file_system_uuid.is_valid());

  // Make sure that |file_system_path| is a valid absolute path.
  base::FilePath path = base::FilePath::FromUTF8Unsafe(file_system_path);
  if (!path.IsAbsolute()) {
    LOG(ERROR) << "Rejected automatic file system " << file_system_path
               << " with UUID " << file_system_uuid << " because it's not"
               << " a valid absolute path.";
    std::move(connect_callback).Run(false);
    FailedToAddFileSystem(kIllegalPath);
    return;
  }

  // Check if the automatic file system is already known, and potentially
  // already connected (in this session).
  if (IsUserConfirmedAutomaticFileSystem(file_system_path, file_system_uuid)) {
    if (connected_automatic_file_systems_.emplace(file_system_path).second) {
      // The |file_system_path| is known, but was just connected now
      // (within this session).
      VLOG(1) << "Automatic file system " << file_system_path << " with UUID "
              << file_system_uuid << " was found in the profile, and will be "
              << "automatically connected now.";
      connected_automatic_file_systems_.emplace(file_system_path);
      UpdateFileSystemPathsOnUI();
    } else {
      // The |file_system_path| was already connected.
      VLOG(1) << "Automatic file system " << file_system_path << " with UUID "
              << file_system_uuid << " was already connected.";
    }
    std::move(connect_callback).Run(true);
    return;
  }

  if (!add_if_missing) {
    VLOG(1) << "Not adding automatic file system " << file_system_path
            << " with UUID " << file_system_uuid << ".";
    std::move(connect_callback).Run(false);
    return;
  }

  // Ensure that the |path| refers to an existing directory first (since this
  // is a blocking call, we need to perform this operation asynchronously).
  file_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE, BindOnce(&base::DirectoryExists, path),
      BindOnce(&DevToolsFileHelper::ConnectMissingAutomaticFileSystem,
               weak_factory_.GetWeakPtr(), std::move(file_system_path),
               std::move(file_system_uuid),
               std::move(handle_permissions_callback),
               std::move(connect_callback)));
}

void DevToolsFileHelper::ConnectMissingAutomaticFileSystem(
    const std::string& file_system_path,
    const base::Uuid& file_system_uuid,
    const HandlePermissionsCallback& handle_permissions_callback,
    ConnectCallback connect_callback,
    bool directory_exists) {
  if (!directory_exists) {
    LOG(ERROR) << "Rejected automatic file system " << file_system_path
               << " with UUID " << file_system_uuid << " because that"
               << "directory does not exist.";
    std::move(connect_callback).Run(false);
    FailedToAddFileSystem(kIllegalPath);
    return;
  }

  if (IsFileSystemAdded(file_system_path)) {
    RemoveFileSystem(file_system_path);
  }

  std::u16string message =
      l10n_util::GetStringFUTF16(IDS_DEV_TOOLS_CONFIRM_ADD_FILE_SYSTEM_MESSAGE,
                                 base::UTF8ToUTF16(file_system_path));
  handle_permissions_callback.Run(
      file_system_path, message,
      BindOnce(&DevToolsFileHelper::ConnectUserConfirmedAutomaticFileSystem,
               weak_factory_.GetWeakPtr(), std::move(connect_callback),
               file_system_path, file_system_uuid));
}

void DevToolsFileHelper::ConnectUserConfirmedAutomaticFileSystem(
    ConnectCallback connect_callback,
    const std::string& file_system_path,
    const base::Uuid& file_system_uuid,
    bool allowed) {
  VLOG(1) << "User " << (allowed ? "allowed" : "denied")
          << " adding automatic file system " << file_system_path
          << " with UUID " << file_system_uuid << ".";
  if (allowed) {
    connected_automatic_file_systems_.emplace(file_system_path);
  }

  auto path = base::FilePath::FromUTF8Unsafe(file_system_path);
  auto type = file_system_uuid.AsLowercaseString();
  AddUserConfirmedFileSystem(type, path, allowed);

  std::move(connect_callback).Run(allowed);
}

bool DevToolsFileHelper::IsUserConfirmedAutomaticFileSystem(
    const std::string& file_system_path,
    const base::Uuid& file_system_uuid) const {
  DCHECK(file_system_uuid.is_valid());
  const base::Value::Dict& file_system_paths_value =
      profile_->GetPrefs()->GetDict(prefs::kDevToolsFileSystemPaths);
  const base::Value* value = file_system_paths_value.Find(file_system_path);
  if (value == nullptr || !value->is_string()) {
    return false;
  }
  return value->GetString() == file_system_uuid.AsLowercaseString();
}

void DevToolsFileHelper::DisconnectAutomaticFileSystem(
    const std::string& file_system_path) {
  if (connected_automatic_file_systems_.erase(file_system_path) == 1) {
    VLOG(1) << "Disconnected automatic file system " << file_system_path << ".";
    UpdateFileSystemPathsOnUI();
  }
}

void DevToolsFileHelper::InnerAddFileSystem(
    const HandlePermissionsCallback& handle_permissions_callback,
    const std::string& type,
    const ui::SelectedFileInfo& file_info) {
  base::FilePath path = file_info.path();
  std::string file_system_path = path.AsUTF8Unsafe();

  if (IsFileSystemAdded(file_system_path)) {
    RemoveFileSystem(file_system_path);
  }

  std::string path_display_name = path.AsEndingWithSeparator().AsUTF8Unsafe();
  std::u16string message =
      l10n_util::GetStringFUTF16(IDS_DEV_TOOLS_CONFIRM_ADD_FILE_SYSTEM_MESSAGE,
                                 base::UTF8ToUTF16(path_display_name));
  handle_permissions_callback.Run(
      file_system_path, message,
      BindOnce(&DevToolsFileHelper::AddUserConfirmedFileSystem,
               weak_factory_.GetWeakPtr(), type, path));
}

void DevToolsFileHelper::AddUserConfirmedFileSystem(const std::string& type,
                                                    const base::FilePath& path,
                                                    bool allowed) {
  if (!allowed) {
    FailedToAddFileSystem(kPermissionDenied);
    return;
  }

  ScopedDictPrefUpdate update(profile_->GetPrefs(),
                              prefs::kDevToolsFileSystemPaths);
  base::Value::Dict& file_systems_paths_value = update.Get();
  file_systems_paths_value.Set(path.AsUTF8Unsafe(), type);
}

void DevToolsFileHelper::FailedToAddFileSystem(const std::string& error) {
  delegate_->FileSystemAdded(error, nullptr);
}

namespace {

void RunOnUIThread(base::OnceClosure callback) {
  content::GetUIThreadTaskRunner({})->PostTask(FROM_HERE, std::move(callback));
}

}  // namespace

std::vector<DevToolsFileHelper::FileSystem>
DevToolsFileHelper::GetFileSystems() {
  file_system_paths_ = GetActiveFileSystemPaths();
  std::vector<FileSystem> file_systems;
  if (!file_watcher_) {
    file_watcher_.reset(new DevToolsFileWatcher(
        base::BindRepeating(&DevToolsFileHelper::FilePathsChanged,
                            weak_factory_.GetWeakPtr()),
        base::SequencedTaskRunner::GetCurrentDefault()));
    auto change_handler_on_ui =
        base::BindRepeating(&DevToolsFileHelper::UpdateFileSystemPathsOnUI,
                            weak_factory_.GetWeakPtr());
    pref_change_registrar_.Add(
        prefs::kDevToolsFileSystemPaths,
        base::BindRepeating(RunOnUIThread, change_handler_on_ui));
  }
  for (const auto& file_system_path : file_system_paths_) {
    auto path = base::FilePath::FromUTF8Unsafe(file_system_path.first);
    auto file_system =
        storage_->RegisterFileSystem(path, file_system_path.second);
    file_systems.push_back(file_system);
    file_watcher_->AddWatch(std::move(path));
  }
  return file_systems;
}

void DevToolsFileHelper::RemoveFileSystem(const std::string& file_system_path) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto path = base::FilePath::FromUTF8Unsafe(file_system_path);

  if (connected_automatic_file_systems_.erase(file_system_path) == 1) {
    VLOG(1) << "Disconnected automatic file system " << file_system_path
            << " (prior to removal).";
  }

  ScopedDictPrefUpdate update(profile_->GetPrefs(),
                              prefs::kDevToolsFileSystemPaths);
  base::Value::Dict& file_systems_paths_value = update.Get();
  file_systems_paths_value.Remove(file_system_path);
}

bool DevToolsFileHelper::IsFileSystemAdded(
    const std::string& file_system_path) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return file_system_paths_.contains(file_system_path);
}

void DevToolsFileHelper::OnOpenItemComplete(
    const base::FilePath& path,
    platform_util::OpenOperationResult result) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (result == platform_util::OPEN_FAILED_INVALID_TYPE) {
    platform_util::ShowItemInFolder(profile_, path);
  }
}

void DevToolsFileHelper::ShowItemInFolder(const std::string& file_system_path) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (file_system_path.empty()) {
    return;
  }
  base::FilePath path = base::FilePath::FromUTF8Unsafe(file_system_path);
  platform_util::OpenItem(
      profile_, path, platform_util::OPEN_FOLDER,
      base::BindOnce(&DevToolsFileHelper::OnOpenItemComplete,
                     weak_factory_.GetWeakPtr(), path));
}

void DevToolsFileHelper::UpdateFileSystemPathsOnUI() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  PathToType remaining;
  remaining.swap(file_system_paths_);
  DCHECK(file_watcher_.get());

  for (const auto& file_system_path : GetActiveFileSystemPaths()) {
    if (remaining.find(file_system_path.first) == remaining.end()) {
      auto path = base::FilePath::FromUTF8Unsafe(file_system_path.first);
      auto file_system =
          storage_->RegisterFileSystem(path, file_system_path.second);
      delegate_->FileSystemAdded(std::string(), &file_system);
      file_watcher_->AddWatch(std::move(path));
    } else {
      remaining.erase(file_system_path.first);
    }
    file_system_paths_[file_system_path.first] = file_system_path.second;
  }

  for (const auto& file_system : remaining) {
    delegate_->FileSystemRemoved(file_system.first);
    base::FilePath path = base::FilePath::FromUTF8Unsafe(file_system.first);
    storage_->UnregisterFileSystem(path);
    file_watcher_->RemoveWatch(std::move(path));
  }
}

void DevToolsFileHelper::FilePathsChanged(
    const std::vector<std::string>& changed_paths,
    const std::vector<std::string>& added_paths,
    const std::vector<std::string>& removed_paths) {
  delegate_->FilePathsChanged(changed_paths, added_paths, removed_paths);
}

DevToolsFileHelper::PathToType DevToolsFileHelper::GetActiveFileSystemPaths() {
  const base::Value::Dict& file_systems_paths_value =
      profile_->GetPrefs()->GetDict(prefs::kDevToolsFileSystemPaths);
  PathToType result;
  for (auto pair : file_systems_paths_value) {
    const std::string& path = pair.first;
    std::string type;
    if (pair.second.is_string()) {
      type = pair.second.GetString();
    }
    // If the |type| is a valid UUID, it signifies an automatic file
    // system. We only report the subset of automatic file systems that
    // are (currently) connected (within this session). And we report them
    // with type "automatic".
    if (base::Uuid::ParseLowercase(type).is_valid()) {
      if (!connected_automatic_file_systems_.contains(path)) {
        continue;
      }
      type = kAutomaticFileSystemType;
    }
    result[path] = type;
  }
  return result;
}
