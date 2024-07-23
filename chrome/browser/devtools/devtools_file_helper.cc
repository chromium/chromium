// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/devtools_file_helper.h"

#include <set>
#include <vector>

#include "base/base64.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/hash/md5.h"
#include "base/json/values_util.h"
#include "base/lazy_instance.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/devtools/devtools_file_watcher.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/chrome_select_file_policy.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_client.h"
#include "content/public/common/url_constants.h"
#include "storage/browser/file_system/file_system_url.h"
#include "storage/browser/file_system/isolated_context.h"
#include "storage/common/file_system/file_system_util.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/shell_dialogs/select_file_dialog.h"
#include "ui/shell_dialogs/selected_file_info.h"
#include "url/gurl.h"
#include "url/origin.h"

using content::BrowserContext;
using content::BrowserThread;
using content::DownloadManager;
using content::RenderViewHost;
using content::WebContents;
using std::set;

namespace {

static const char kRootName[] = "<root>";
static const char kPermissionDenied[] = "<permission denied>";
static const char kSelectionCancelled[] = "<selection cancelled>";

base::LazyInstance<base::FilePath>::Leaky
    g_last_save_path = LAZY_INSTANCE_INITIALIZER;

typedef base::OnceCallback<void(const base::FilePath&)> SelectedCallback;
typedef base::OnceCallback<void(void)> CanceledCallback;

class SelectFileDialog : public ui::SelectFileDialog::Listener {
 public:
  SelectFileDialog(const SelectFileDialog&) = delete;
  SelectFileDialog& operator=(const SelectFileDialog&) = delete;

  static void Show(SelectedCallback selected_callback,
                   CanceledCallback canceled_callback,
                   WebContents* web_contents,
                   ui::SelectFileDialog::Type type,
                   const base::FilePath& default_path) {
    // `dialog` is self-deleting.
    auto* dialog = new SelectFileDialog();
    dialog->ShowDialog(std::move(selected_callback),
                       std::move(canceled_callback), web_contents, type,
                       default_path);
  }

  // ui::SelectFileDialog::Listener implementation.
  void FileSelected(const ui::SelectedFileInfo& file, int index) override {
    std::move(selected_callback_).Run(file.path());
    delete this;
  }

  void FileSelectionCanceled() override {
    if (canceled_callback_) {
      std::move(canceled_callback_).Run();
    }
    delete this;
  }

 private:
  SelectFileDialog() = default;
  ~SelectFileDialog() override { select_file_dialog_->ListenerDestroyed(); }

  void ShowDialog(SelectedCallback selected_callback,
                  CanceledCallback canceled_callback,
                  WebContents* web_contents,
                  ui::SelectFileDialog::Type type,
                  const base::FilePath& default_path) {
    selected_callback_ = std::move(selected_callback);
    canceled_callback_ = std::move(canceled_callback);
    select_file_dialog_ = ui::SelectFileDialog::Create(
        this, std::make_unique<ChromeSelectFilePolicy>(web_contents));
    base::FilePath::StringType ext;
    ui::SelectFileDialog::FileTypeInfo file_type_info;
    if (type == ui::SelectFileDialog::SELECT_SAVEAS_FILE &&
        default_path.Extension().length() > 0) {
      ext = default_path.Extension().substr(1);
      file_type_info.extensions.resize(1);
      file_type_info.extensions[0].push_back(ext);
    }
    select_file_dialog_->SelectFile(
        type, std::u16string(), default_path, &file_type_info, 0, ext,
        platform_util::GetTopLevel(web_contents->GetNativeView()));
  }

  scoped_refptr<ui::SelectFileDialog> select_file_dialog_;
  SelectedCallback selected_callback_;
  CanceledCallback canceled_callback_;
};

void WriteToFile(const base::FilePath& path,
                 const std::string& content,
                 bool is_base64) {
  DCHECK(!path.empty());

  if (!is_base64) {
    base::WriteFile(path, content);
    return;
  }

  const std::optional<std::vector<uint8_t>> decoded_content =
      base::Base64Decode(content);
  if (decoded_content) {
    base::WriteFile(path, decoded_content.value());
  } else {
    LOG(ERROR) << "Invalid base64. Not writing " << path;
  }
}

void AppendToFile(const base::FilePath& path, const std::string& content) {
  DCHECK(!path.empty());

  base::AppendToFile(path, content);
}

storage::IsolatedContext* isolated_context() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  storage::IsolatedContext* isolated_context =
      storage::IsolatedContext::GetInstance();
  DCHECK(isolated_context);
  return isolated_context;
}

std::string RegisterFileSystem(WebContents* web_contents,
                               const base::FilePath& path) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  CHECK(web_contents->GetLastCommittedURL().SchemeIs(
      content::kChromeDevToolsScheme));
  std::string root_name(kRootName);
  storage::IsolatedContext::ScopedFSHandle file_system =
      isolated_context()->RegisterFileSystemForPath(
          storage::kFileSystemTypeLocal, std::string(), path, &root_name);

  content::ChildProcessSecurityPolicy* policy =
      content::ChildProcessSecurityPolicy::GetInstance();
  RenderViewHost* render_view_host =
      web_contents->GetPrimaryMainFrame()->GetRenderViewHost();
  int renderer_id = render_view_host->GetProcess()->GetID();
  policy->GrantReadFileSystem(renderer_id, file_system.id());
  policy->GrantWriteFileSystem(renderer_id, file_system.id());
  policy->GrantCreateFileForFileSystem(renderer_id, file_system.id());
  policy->GrantDeleteFromFileSystem(renderer_id, file_system.id());

  // We only need file level access for reading FileEntries. Saving FileEntries
  // just needs the file system to have read/write access, which is granted
  // above if required.
  if (!policy->CanReadFile(renderer_id, path))
    policy->GrantReadFile(renderer_id, path);
  return file_system.id();
}

DevToolsFileHelper::FileSystem CreateFileSystemStruct(
    WebContents* web_contents,
    const std::string& type,
    const std::string& file_system_id,
    const std::string& file_system_path) {
  const GURL origin =
      web_contents->GetLastCommittedURL().DeprecatedGetOriginAsURL();
  std::string file_system_name =
      storage::GetIsolatedFileSystemName(origin, file_system_id);
  std::string root_url = storage::GetIsolatedFileSystemRootURIString(
      origin, file_system_id, kRootName);
  return DevToolsFileHelper::FileSystem(type, file_system_name, root_url,
                                        file_system_path);
}

using PathToType = std::map<std::string, std::string>;
PathToType GetAddedFileSystemPaths(Profile* profile) {
  const base::Value::Dict& file_systems_paths_value =
      profile->GetPrefs()->GetDict(prefs::kDevToolsFileSystemPaths);
  PathToType result;
  for (auto pair : file_systems_paths_value) {
    std::string type =
        pair.second.is_string() ? pair.second.GetString() : std::string();
    result[pair.first] = type;
  }
  return result;
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

DevToolsFileHelper::DevToolsFileHelper(WebContents* web_contents,
                                       Profile* profile,
                                       Delegate* delegate)
    : web_contents_(web_contents),
      profile_(profile),
      delegate_(delegate),
      file_task_runner_(
          base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()})) {
  pref_change_registrar_.Init(profile_->GetPrefs());
}

DevToolsFileHelper::~DevToolsFileHelper() = default;

void DevToolsFileHelper::Save(const std::string& url,
                              const std::string& content,
                              bool save_as,
                              bool is_base64,
                              SaveCallback saveCallback,
                              base::OnceClosure cancelCallback) {
  auto it = saved_files_.find(url);
  if (it != saved_files_.end() && !save_as) {
    SaveAsFileSelected(url, content, is_base64, std::move(saveCallback),
                       it->second);
    return;
  }

  const base::Value::Dict& file_map =
      profile_->GetPrefs()->GetDict(prefs::kDevToolsEditedFiles);
  base::FilePath initial_path;

  if (const base::Value* path_value = file_map.Find(base::MD5String(url))) {
    std::optional<base::FilePath> path = base::ValueToFilePath(*path_value);
    if (path)
      initial_path = std::move(*path);
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
    if (suggested_file_name.length() > 64)
      suggested_file_name = suggested_file_name.substr(0, 64);
    // TODO(crbug.com/40839171): Ensure suggested_file_name is an ASCII string
    if (!g_last_save_path.Pointer()->empty()) {
      initial_path = g_last_save_path.Pointer()->DirName().AppendASCII(
          suggested_file_name);
    } else {
      base::FilePath download_path =
          DownloadPrefs::FromDownloadManager(profile_->GetDownloadManager())
              ->DownloadPath();
      initial_path = download_path.AppendASCII(suggested_file_name);
    }
  }

  SelectFileDialog::Show(
      base::BindOnce(&DevToolsFileHelper::SaveAsFileSelected,
                     weak_factory_.GetWeakPtr(), url, content, is_base64,
                     std::move(saveCallback)),
      std::move(cancelCallback), web_contents_,
      ui::SelectFileDialog::SELECT_SAVEAS_FILE, initial_path);
}

void DevToolsFileHelper::Append(const std::string& url,
                                const std::string& content,
                                base::OnceClosure callback) {
  auto it = saved_files_.find(url);
  if (it == saved_files_.end())
    return;
  std::move(callback).Run();
  file_task_runner_->PostTask(FROM_HERE,
                              BindOnce(&AppendToFile, it->second, content));
}

void DevToolsFileHelper::SaveAsFileSelected(const std::string& url,
                                            const std::string& content,
                                            bool is_base64,
                                            SaveCallback callback,
                                            const base::FilePath& path) {
  *g_last_save_path.Pointer() = path;
  saved_files_[url] = path;

  ScopedDictPrefUpdate update(profile_->GetPrefs(),
                              prefs::kDevToolsEditedFiles);
  base::Value::Dict& files_map = update.Get();
  files_map.Set(base::MD5String(url), base::FilePathToValue(path));

  std::string file_system_path = path.AsUTF8Unsafe();
  // Run 'SaveCallback' only once we have actually written the file, but
  // run it on the current task runner.
  scoped_refptr<base::SequencedTaskRunner> current_task_runner =
      base::SequencedTaskRunner::GetCurrentDefault();
  file_task_runner_->PostTask(
      FROM_HERE, BindOnce(&WriteToFile, path, content, is_base64)
                     .Then(base::BindPostTask(
                         current_task_runner,
                         BindOnce(std::move(callback), file_system_path))));
}

void DevToolsFileHelper::AddFileSystem(
    const std::string& type,
    const ShowInfoBarCallback& show_info_bar_callback) {
  SelectFileDialog::Show(
      base::BindOnce(&DevToolsFileHelper::InnerAddFileSystem,
                     weak_factory_.GetWeakPtr(), show_info_bar_callback, type),
      base::BindOnce(&DevToolsFileHelper::FailedToAddFileSystem,
                     weak_factory_.GetWeakPtr(), kSelectionCancelled),
      web_contents_, ui::SelectFileDialog::SELECT_FOLDER, base::FilePath());
}

void DevToolsFileHelper::UpgradeDraggedFileSystemPermissions(
    const std::string& file_system_url,
    const ShowInfoBarCallback& show_info_bar_callback) {
  const GURL gurl(file_system_url);
  storage::FileSystemURL root_url = isolated_context()->CrackURL(
      gurl, blink::StorageKey::CreateFirstParty(url::Origin::Create(gurl)));
  if (!root_url.is_valid() || !root_url.path().empty())
    return;

  std::vector<storage::MountPoints::MountPointInfo> mount_points;
  isolated_context()->GetDraggedFileInfo(root_url.filesystem_id(),
                                         &mount_points);

  std::vector<storage::MountPoints::MountPointInfo>::const_iterator it =
      mount_points.begin();
  for (; it != mount_points.end(); ++it)
    InnerAddFileSystem(show_info_bar_callback, std::string(), it->path);
}

void DevToolsFileHelper::InnerAddFileSystem(
    const ShowInfoBarCallback& show_info_bar_callback,
    const std::string& type,
    const base::FilePath& path) {
  std::string file_system_path = path.AsUTF8Unsafe();

  if (IsFileSystemAdded(file_system_path))
    RemoveFileSystem(file_system_path);

  std::string path_display_name = path.AsEndingWithSeparator().AsUTF8Unsafe();
  std::u16string message =
      l10n_util::GetStringFUTF16(IDS_DEV_TOOLS_CONFIRM_ADD_FILE_SYSTEM_MESSAGE,
                                 base::UTF8ToUTF16(path_display_name));
  show_info_bar_callback.Run(
      message, BindOnce(&DevToolsFileHelper::AddUserConfirmedFileSystem,
                        weak_factory_.GetWeakPtr(), type, path));
}

void DevToolsFileHelper::AddUserConfirmedFileSystem(const std::string& type,
                                                    const base::FilePath& path,
                                                    bool allowed) {
  if (!allowed) {
    FailedToAddFileSystem(kPermissionDenied);
    return;
  }

  std::string file_system_id = RegisterFileSystem(web_contents_, path);
  std::string file_system_path = path.AsUTF8Unsafe();

  ScopedDictPrefUpdate update(profile_->GetPrefs(),
                              prefs::kDevToolsFileSystemPaths);
  base::Value::Dict& file_systems_paths_value = update.Get();
  file_systems_paths_value.Set(file_system_path, type);
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
  file_system_paths_ = GetAddedFileSystemPaths(profile_);
  std::vector<FileSystem> file_systems;
  if (!file_watcher_) {
    file_watcher_.reset(new DevToolsFileWatcher(
        base::BindRepeating(&DevToolsFileHelper::FilePathsChanged,
                            weak_factory_.GetWeakPtr()),
        base::SequencedTaskRunner::GetCurrentDefault()));
    auto change_handler_on_ui = base::BindRepeating(
        &DevToolsFileHelper::FileSystemPathsSettingChangedOnUI,
        weak_factory_.GetWeakPtr());
    pref_change_registrar_.Add(
        prefs::kDevToolsFileSystemPaths,
        base::BindRepeating(RunOnUIThread, change_handler_on_ui));
  }
  for (auto file_system_path : file_system_paths_) {
    base::FilePath path =
        base::FilePath::FromUTF8Unsafe(file_system_path.first);
    std::string file_system_id = RegisterFileSystem(web_contents_, path);
    FileSystem filesystem =
        CreateFileSystemStruct(web_contents_, file_system_path.second,
                               file_system_id, file_system_path.first);
    file_systems.push_back(filesystem);
    file_watcher_->AddWatch(std::move(path));
  }
  return file_systems;
}

void DevToolsFileHelper::RemoveFileSystem(const std::string& file_system_path) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  base::FilePath path = base::FilePath::FromUTF8Unsafe(file_system_path);
  isolated_context()->RevokeFileSystemByPath(path);

  ScopedDictPrefUpdate update(profile_->GetPrefs(),
                              prefs::kDevToolsFileSystemPaths);
  base::Value::Dict& file_systems_paths_value = update.Get();
  file_systems_paths_value.Remove(file_system_path);
}

bool DevToolsFileHelper::IsFileSystemAdded(
    const std::string& file_system_path) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  const base::Value::Dict& file_systems_paths_value =
      profile_->GetPrefs()->GetDict(prefs::kDevToolsFileSystemPaths);
  return file_systems_paths_value.Find(file_system_path);
}

void DevToolsFileHelper::OnOpenItemComplete(
    const base::FilePath& path,
    platform_util::OpenOperationResult result) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (result == platform_util::OPEN_FAILED_INVALID_TYPE)
    platform_util::ShowItemInFolder(profile_, path);
}

void DevToolsFileHelper::ShowItemInFolder(const std::string& file_system_path) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (file_system_path.empty())
    return;
  base::FilePath path = base::FilePath::FromUTF8Unsafe(file_system_path);
  platform_util::OpenItem(
      profile_, path, platform_util::OPEN_FOLDER,
      base::BindOnce(&DevToolsFileHelper::OnOpenItemComplete,
                     weak_factory_.GetWeakPtr(), path));
}

void DevToolsFileHelper::FileSystemPathsSettingChangedOnUI() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  PathToType remaining;
  remaining.swap(file_system_paths_);
  DCHECK(file_watcher_.get());

  for (auto file_system : GetAddedFileSystemPaths(profile_)) {
    if (remaining.find(file_system.first) == remaining.end()) {
      base::FilePath path = base::FilePath::FromUTF8Unsafe(file_system.first);
      std::string file_system_id = RegisterFileSystem(web_contents_, path);
      FileSystem filesystem = CreateFileSystemStruct(
          web_contents_, file_system.second, file_system_id, file_system.first);
      delegate_->FileSystemAdded(std::string(), &filesystem);
      file_watcher_->AddWatch(std::move(path));
    } else {
      remaining.erase(file_system.first);
    }
    file_system_paths_[file_system.first] = file_system.second;
  }

  for (auto file_system : remaining) {
    delegate_->FileSystemRemoved(file_system.first);
    base::FilePath path = base::FilePath::FromUTF8Unsafe(file_system.first);
    file_watcher_->RemoveWatch(std::move(path));
  }
}

void DevToolsFileHelper::FilePathsChanged(
    const std::vector<std::string>& changed_paths,
    const std::vector<std::string>& added_paths,
    const std::vector<std::string>& removed_paths) {
  delegate_->FilePathsChanged(changed_paths, added_paths, removed_paths);
}
