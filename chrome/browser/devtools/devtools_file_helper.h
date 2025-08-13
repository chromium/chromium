// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVTOOLS_DEVTOOLS_FILE_HELPER_H_
#define CHROME_BROWSER_DEVTOOLS_DEVTOOLS_FILE_HELPER_H_

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/uuid.h"
#include "chrome/browser/devtools/devtools_file_watcher.h"
#include "chrome/browser/platform_util.h"
#include "components/prefs/pref_change_registrar.h"

class GURL;
class Profile;

namespace base {
class FilePath;
class SequencedTaskRunner;
}  // namespace base

namespace ui {
struct SelectedFileInfo;
}  // namespace ui

class DevToolsFileHelper {
 public:
  struct FileSystem {
    FileSystem();
    ~FileSystem();
    FileSystem(const FileSystem& other);
    FileSystem(const std::string& type,
               const std::string& file_system_name,
               const std::string& root_url,
               const std::string& file_system_path);

    friend constexpr bool operator==(const FileSystem&,
                                     const FileSystem&) = default;

    std::string type;
    std::string file_system_name;
    std::string root_url;
    std::string file_system_path;
  };

  class Delegate {
   public:
    virtual ~Delegate() = default;

    virtual void FileSystemAdded(const std::string& error,
                                 const FileSystem* file_system) = 0;
    virtual void FileSystemRemoved(const std::string& file_system_path) = 0;
    virtual void FilePathsChanged(
        const std::vector<std::string>& changed_paths,
        const std::vector<std::string>& added_paths,
        const std::vector<std::string>& removed_paths) = 0;
  };

  class Storage {
   public:
    virtual ~Storage();

    virtual FileSystem RegisterFileSystem(const base::FilePath& path,
                                          const std::string& type) = 0;
    virtual void UnregisterFileSystem(const base::FilePath& path) = 0;

    virtual std::vector<base::FilePath> GetDraggedFileSystemPaths(
        const GURL& file_system_url) = 0;
  };

  DevToolsFileHelper(Profile* profile, Delegate* delegate, Storage* storage);

  DevToolsFileHelper(const DevToolsFileHelper&) = delete;
  DevToolsFileHelper& operator=(const DevToolsFileHelper&) = delete;

  ~DevToolsFileHelper();

  using CanceledCallback = base::OnceClosure;
  using ConnectCallback = base::OnceCallback<void(bool)>;
  using SaveCallback = base::OnceCallback<void(const std::string&)>;
  using SelectedCallback =
      base::OnceCallback<void(const ui::SelectedFileInfo&)>;
  using SelectFileCallback =
      base::OnceCallback<void(SelectedCallback selected_callback,
                              CanceledCallback canceled_callback,
                              const base::FilePath& default_path)>;
  using HandlePermissionsCallback =
      base::RepeatingCallback<void(const std::string&,
                                   const std::u16string&,
                                   base::OnceCallback<void(bool)>)>;

  // Saves |content| to the file and associates its path with given |url|.
  // If client is calling this method with given |url| for the first time
  // or |save_as| is true, confirmation dialog is shown to the user.
  void Save(const std::string& url,
            const std::string& content,
            bool save_as,
            bool is_base64,
            SelectFileCallback select_file_callback,
            SaveCallback save_callback,
            CanceledCallback canceled_callback);

  // Append |content| to the file that has been associated with given |url|.
  // The |url| can be associated with a file via calling Save method.
  // If the Save method has not been called for this |url|, then
  // Append method does nothing.
  void Append(const std::string& url,
              const std::string& content,
              base::OnceClosure callback);

  // Opens a folder selector dialog, asking the user to select a folder
  // on the local file system to be added as file system with the given
  // |type|. Once the user selects a folder, it shows an infobar by means
  // of |show_info_bar_callback| to let the user decide whether to grant
  // security permissions or not. If user allows adding file system in
  // infobar, grants renderer read/write permissions and registers isolated
  // file system for it. Saves file system path to prefs.
  // If user denies adding file system in infobar, passes error string to
  // |callback|.
  // The filesystem is marked of |type|, which is an arbitrary string (with
  // the exception that it must not be the string "automatic" and it also
  // must not be a valid UUID).
  void AddFileSystem(const std::string& type,
                     SelectFileCallback select_file_callback,
                     const HandlePermissionsCallback& show_info_bar_callback);

  // Upgrades dragged file system permissions to a read-write access.
  // Shows infobar by means of |show_info_bar_callback| to let the user decide
  // whether to grant security permissions or not.
  // If user allows adding file system in infobar, grants renderer read/write
  // permissions, registers isolated file system for it and passes FileSystem
  // struct to |callback|. Saves file system path to prefs.
  // If user denies adding file system in infobar, passes error string to
  // |callback|.
  void UpgradeDraggedFileSystemPermissions(
      const std::string& file_system_url,
      const HandlePermissionsCallback& show_info_bar_callback);

  // Attempts to automatically connect to the |file_system_path| (identified
  // by path and |file_system_uuid|). If this is the first time that the
  // |file_system_path| is being connected and |add_if_missing| is true, the
  // user will be asked to grant permission to do so, and upon confirming,
  // the |file_system_path| will be remembered in the profile.
  // Invokes |connect_callback| with the result.
  void ConnectAutomaticFileSystem(
      const std::string& file_system_path,
      const base::Uuid& file_system_uuid,
      bool add_if_missing,
      const HandlePermissionsCallback& show_info_bar_callback,
      ConnectCallback connect_callback);

  // Disconnects the automatically connected |file_system_path|.
  void DisconnectAutomaticFileSystem(const std::string& file_system_path);

  // Loads file system paths from prefs, grants permissions and registers
  // isolated file system for those of them that contain magic file and passes
  // FileSystem structs for registered file systems to |callback|.
  std::vector<FileSystem> GetFileSystems();

  // Removes isolated file system for given |file_system_path|.
  void RemoveFileSystem(const std::string& file_system_path);

  // Returns whether access to the folder on given |file_system_path| was
  // granted.
  bool IsFileSystemAdded(const std::string& file_system_path);

  // Opens and reveals file in OS's default file manager.
  void ShowItemInFolder(const std::string& file_system_path);

 private:
  void OnOpenItemComplete(const base::FilePath& path,
                          platform_util::OpenOperationResult result);
  void SaveToFileSelected(const std::string& url,
                          const std::string& content,
                          bool is_base64,
                          SaveCallback callback,
                          const ui::SelectedFileInfo& file_info);
  void InnerAddFileSystem(
      const HandlePermissionsCallback& show_info_bar_callback,
      const std::string& type,
      const ui::SelectedFileInfo& file_info);
  void AddUserConfirmedFileSystem(const std::string& type,
                                  const base::FilePath& path,
                                  bool allowed);
  void ConnectMissingAutomaticFileSystem(
      const std::string& file_system_path,
      const base::Uuid& file_system_uuid,
      const HandlePermissionsCallback& handle_permissions_callback,
      ConnectCallback connect_callback,
      bool directory_exists);
  void ConnectUserConfirmedAutomaticFileSystem(
      ConnectCallback connect_callback,
      const std::string& file_system_path,
      const base::Uuid& file_system_uuid,
      bool allowed);
  bool IsUserConfirmedAutomaticFileSystem(
      const std::string& file_system_path,
      const base::Uuid& file_system_uuid) const;
  void FailedToAddFileSystem(const std::string& error);
  void UpdateFileSystemPathsOnUI();
  void FilePathsChanged(const std::vector<std::string>& changed_paths,
                        const std::vector<std::string>& added_paths,
                        const std::vector<std::string>& removed_paths);

  using PathToType = std::map<std::string, std::string>;
  PathToType GetActiveFileSystemPaths();

  raw_ptr<Profile> profile_;
  raw_ptr<DevToolsFileHelper::Delegate> delegate_;
  raw_ptr<DevToolsFileHelper::Storage> storage_;
  typedef std::map<std::string, ui::SelectedFileInfo> SelectedFileInfoMap;
  SelectedFileInfoMap saved_files_;
  PrefChangeRegistrar pref_change_registrar_;
  PathToType file_system_paths_;
  std::set<std::string> connected_automatic_file_systems_;
  std::unique_ptr<DevToolsFileWatcher, DevToolsFileWatcher::Deleter>
      file_watcher_;
  scoped_refptr<base::SequencedTaskRunner> file_task_runner_;
  base::WeakPtrFactory<DevToolsFileHelper> weak_factory_{this};
};

#endif  // CHROME_BROWSER_DEVTOOLS_DEVTOOLS_FILE_HELPER_H_
