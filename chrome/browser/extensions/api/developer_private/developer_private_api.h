// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_DEVELOPER_PRIVATE_DEVELOPER_PRIVATE_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_DEVELOPER_PRIVATE_DEVELOPER_PRIVATE_API_H_

#include <map>
#include <set>

#include "base/files/file.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/extensions/api/commands/command_service.h"
#include "chrome/browser/extensions/api/developer_private/entry_picker.h"
#include "chrome/browser/extensions/error_console/error_console.h"
#include "chrome/browser/extensions/extension_allowlist.h"
#include "chrome/browser/extensions/extension_management.h"
#include "chrome/browser/extensions/extension_uninstall_dialog.h"
#include "chrome/browser/extensions/load_error_reporter.h"
#include "chrome/browser/extensions/pack_extension_job.h"
#include "chrome/common/extensions/api/developer_private.h"
#include "chrome/common/extensions/webstore_install_result.h"
#include "components/prefs/pref_change_registrar.h"
#include "extensions/browser/api/file_system/file_system_api.h"
#include "extensions/browser/app_window/app_window_registry.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_function.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_prefs_observer.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_observer.h"
#include "extensions/browser/permissions_manager.h"
#include "extensions/browser/process_manager.h"
#include "extensions/browser/process_manager_observer.h"
#include "extensions/browser/warning_service.h"
#include "extensions/common/extension_id.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/file_system_operation.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/shell_dialogs/select_file_dialog.h"

class Profile;

namespace extensions {

class EventRouter;
class ExtensionError;
class ExtensionInfoGenerator;

namespace api {

namespace developer_private {

struct ProfileInfo;

}

class EntryPickerClient;

}  // namespace api

class DeveloperPrivateEventRouter : public ExtensionRegistryObserver,
                                    public ErrorConsole::Observer,
                                    public ProcessManagerObserver,
                                    public AppWindowRegistry::Observer,
                                    public CommandService::Observer,
                                    public ExtensionPrefsObserver,
                                    public ExtensionAllowlist::Observer,
                                    public ExtensionManagement::Observer,
                                    public WarningService::Observer,
                                    public PermissionsManager::Observer {
 public:
  explicit DeveloperPrivateEventRouter(Profile* profile);

  DeveloperPrivateEventRouter(const DeveloperPrivateEventRouter&) = delete;
  DeveloperPrivateEventRouter& operator=(const DeveloperPrivateEventRouter&) =
      delete;

  ~DeveloperPrivateEventRouter() override;

  // Add or remove an ID to the list of extensions subscribed to events.
  void AddExtensionId(const std::string& extension_id);
  void RemoveExtensionId(const std::string& extension_id);

 private:
  // ExtensionRegistryObserver:
  void OnExtensionLoaded(content::BrowserContext* browser_context,
                         const Extension* extension) override;
  void OnExtensionUnloaded(content::BrowserContext* browser_context,
                           const Extension* extension,
                           UnloadedExtensionReason reason) override;
  void OnExtensionInstalled(content::BrowserContext* browser_context,
                            const Extension* extension,
                            bool is_update) override;
  void OnExtensionUninstalled(content::BrowserContext* browser_context,
                              const Extension* extension,
                              extensions::UninstallReason reason) override;

  // ErrorConsole::Observer:
  void OnErrorAdded(const ExtensionError* error) override;
  void OnErrorsRemoved(const std::set<std::string>& extension_ids) override;

  // ProcessManagerObserver:
  void OnExtensionFrameRegistered(
      const std::string& extension_id,
      content::RenderFrameHost* render_frame_host) override;
  void OnExtensionFrameUnregistered(
      const std::string& extension_id,
      content::RenderFrameHost* render_frame_host) override;
  void OnServiceWorkerRegistered(const WorkerId& worker_id) override;
  void OnServiceWorkerUnregistered(const WorkerId& worker_id) override;

  // AppWindowRegistry::Observer:
  void OnAppWindowAdded(AppWindow* window) override;
  void OnAppWindowRemoved(AppWindow* window) override;

  // CommandService::Observer:
  void OnExtensionCommandAdded(const std::string& extension_id,
                               const Command& added_command) override;
  void OnExtensionCommandRemoved(const std::string& extension_id,
                                 const Command& removed_command) override;

  // ExtensionPrefsObserver:
  void OnExtensionDisableReasonsChanged(const std::string& extension_id,
                                        int disable_reasons) override;
  void OnExtensionRuntimePermissionsChanged(
      const std::string& extension_id) override;

  // ExtensionAllowlist::Observer
  void OnExtensionAllowlistWarningStateChanged(const std::string& extension_id,
                                               bool show_warning) override;

  // ExtensionManagement::Observer:
  void OnExtensionManagementSettingsChanged() override;

  // WarningService::Observer:
  void ExtensionWarningsChanged(
      const ExtensionIdSet& affected_extensions) override;

  // PermissionsManager::Observer:
  void OnUserPermissionsSettingsChanged(
      const PermissionsManager::UserPermissionsSettings& settings) override;
  void OnExtensionPermissionsUpdated(
      const Extension& extension,
      const PermissionSet& permissions,
      PermissionsManager::UpdateReason reason) override;

  // Handles a profile preference change.
  void OnProfilePrefChanged();

  // Broadcasts an event to all listeners.
  void BroadcastItemStateChanged(api::developer_private::EventType event_type,
                                 const std::string& id);
  void BroadcastItemStateChangedHelper(
      api::developer_private::EventType event_type,
      const std::string& extension_id,
      std::unique_ptr<ExtensionInfoGenerator> info_generator,
      std::vector<api::developer_private::ExtensionInfo> infos);

  base::ScopedObservation<ExtensionRegistry, ExtensionRegistryObserver>
      extension_registry_observation_{this};
  base::ScopedObservation<ErrorConsole, ErrorConsole::Observer>
      error_console_observation_{this};
  base::ScopedObservation<ProcessManager, ProcessManagerObserver>
      process_manager_observation_{this};
  base::ScopedObservation<AppWindowRegistry, AppWindowRegistry::Observer>
      app_window_registry_observation_{this};
  base::ScopedObservation<WarningService, WarningService::Observer>
      warning_service_observation_{this};
  base::ScopedObservation<ExtensionPrefs, ExtensionPrefsObserver>
      extension_prefs_observation_{this};
  base::ScopedObservation<ExtensionManagement, ExtensionManagement::Observer>
      extension_management_observation_{this};
  base::ScopedObservation<CommandService, CommandService::Observer>
      command_service_observation_{this};
  base::ScopedObservation<ExtensionAllowlist, ExtensionAllowlist::Observer>
      extension_allowlist_observer_{this};
  base::ScopedObservation<PermissionsManager, PermissionsManager::Observer>
      permissions_manager_observation_{this};

  raw_ptr<Profile> profile_;

  raw_ptr<EventRouter> event_router_;

  // The set of IDs of the Extensions that have subscribed to DeveloperPrivate
  // events. Since the only consumer of the DeveloperPrivate API is currently
  // the Apps Developer Tool (which replaces the chrome://extensions page), we
  // don't want to send information about the subscribing extension in an
  // update. In particular, we want to avoid entering a loop, which could happen
  // when, e.g., the Apps Developer Tool throws an error.
  std::set<std::string> extension_ids_;

  PrefChangeRegistrar pref_change_registrar_;

  base::WeakPtrFactory<DeveloperPrivateEventRouter> weak_factory_{this};
};

// The profile-keyed service that manages the DeveloperPrivate API.
class DeveloperPrivateAPI : public BrowserContextKeyedAPI,
                            public EventRouter::Observer {
 public:
  using UnpackedRetryId = std::string;

  static BrowserContextKeyedAPIFactory<DeveloperPrivateAPI>*
  GetFactoryInstance();

  static std::unique_ptr<api::developer_private::ProfileInfo> CreateProfileInfo(
      Profile* profile);

  // Convenience method to get the DeveloperPrivateAPI for a profile.
  static DeveloperPrivateAPI* Get(content::BrowserContext* context);

  explicit DeveloperPrivateAPI(content::BrowserContext* context);

  DeveloperPrivateAPI(const DeveloperPrivateAPI&) = delete;
  DeveloperPrivateAPI& operator=(const DeveloperPrivateAPI&) = delete;

  ~DeveloperPrivateAPI() override;

  // Adds a path to the list of allowed unpacked paths for the given
  // |web_contents|. Returns a unique identifier to retry that path. Safe to
  // call multiple times for the same <web_contents, path> pair; each call will
  // return the same identifier.
  UnpackedRetryId AddUnpackedPath(content::WebContents* web_contents,
                                  const base::FilePath& path);

  // Returns the FilePath associated with the given |id| and |web_contents|, if
  // one exists.
  base::FilePath GetUnpackedPath(content::WebContents* web_contents,
                                 const UnpackedRetryId& id) const;

  // Sets the dragged path for the given |web_contents|.
  void SetDraggedPath(content::WebContents* web_contents,
                      const base::FilePath& path);

  // Returns the dragged path for the given |web_contents|, if one exists.
  base::FilePath GetDraggedPath(content::WebContents* web_contents) const;

  // KeyedService implementation
  void Shutdown() override;

  // EventRouter::Observer implementation.
  void OnListenerAdded(const EventListenerInfo& details) override;
  void OnListenerRemoved(const EventListenerInfo& details) override;

  DeveloperPrivateEventRouter* developer_private_event_router() {
    return developer_private_event_router_.get();
  }
  const base::FilePath& last_unpacked_directory() const {
    return last_unpacked_directory_;
  }

 private:
  class WebContentsTracker;

  using IdToPathMap = std::map<UnpackedRetryId, base::FilePath>;
  // Data specific to a given WebContents.
  struct WebContentsData {
    WebContentsData();

    WebContentsData(const WebContentsData&) = delete;
    WebContentsData& operator=(const WebContentsData&) = delete;

    ~WebContentsData();
    WebContentsData(WebContentsData&& other);

    // A set of unpacked paths that we are allowed to load for different
    // WebContents. For security reasons, we don't let JavaScript arbitrarily
    // pass us a path and load the extension at that location; instead, the user
    // has to explicitly select the path through a native dialog first, and then
    // we will allow JavaScript to request we reload that same selected path.
    // Additionally, these are segmented by WebContents; this is primarily to
    // allow collection (removing old paths when the WebContents closes) but has
    // the effect that WebContents A cannot retry a path selected in
    // WebContents B.
    IdToPathMap allowed_unpacked_paths;

    // The last dragged path for the WebContents.
    base::FilePath dragged_path;
  };

  friend class BrowserContextKeyedAPIFactory<DeveloperPrivateAPI>;

  // BrowserContextKeyedAPI implementation.
  static const char* service_name() { return "DeveloperPrivateAPI"; }
  static const bool kServiceRedirectedInIncognito = true;
  static const bool kServiceIsNULLWhileTesting = true;

  void RegisterNotifications();

  const WebContentsData* GetWebContentsData(
      content::WebContents* web_contents) const;
  WebContentsData* GetOrCreateWebContentsData(
      content::WebContents* web_contents);

  raw_ptr<Profile> profile_;

  // Used to start the load |load_extension_dialog_| in the last directory that
  // was loaded.
  base::FilePath last_unpacked_directory_;

  std::map<content::WebContents*, WebContentsData> web_contents_data_;

  // Created lazily upon OnListenerAdded.
  std::unique_ptr<DeveloperPrivateEventRouter> developer_private_event_router_;

  base::WeakPtrFactory<DeveloperPrivateAPI> weak_factory_{this};
};

template <>
void BrowserContextKeyedAPIFactory<
    DeveloperPrivateAPI>::DeclareFactoryDependencies();

namespace api {

class DeveloperPrivateAPIFunction : public ExtensionFunction {
 protected:
  ~DeveloperPrivateAPIFunction() override;

  // Returns the extension with the given |id| from the registry, including
  // all possible extensions (enabled, disabled, terminated, etc).
  const Extension* GetExtensionById(const std::string& id);

  // Returns the extension with the given |id| from the registry, only checking
  // enabled extensions.
  const Extension* GetEnabledExtensionById(const std::string& id);
};

class DeveloperPrivateAutoUpdateFunction : public DeveloperPrivateAPIFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("developerPrivate.autoUpdate",
                             DEVELOPERPRIVATE_AUTOUPDATE)

 protected:
  ~DeveloperPrivateAutoUpdateFunction() override;
  ResponseAction Run() override;

 private:
  void OnComplete();
};

class DeveloperPrivateGetItemsInfoFunction
    : public DeveloperPrivateAPIFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("developerPrivate.getItemsInfo",
                             DEVELOPERPRIVATE_GETITEMSINFO)
  DeveloperPrivateGetItemsInfoFunction();

  DeveloperPrivateGetItemsInfoFunction(
      const DeveloperPrivateGetItemsInfoFunction&) = delete;
  DeveloperPrivateGetItemsInfoFunction& operator=(
      const DeveloperPrivateGetItemsInfoFunction&) = delete;

 private:
  ~DeveloperPrivateGetItemsInfoFunction() override;
  ResponseAction Run() override;

  void OnInfosGenerated(
      std::vector<api::developer_private::ExtensionInfo> infos);

  std::unique_ptr<ExtensionInfoGenerator> info_generator_;
};

class DeveloperPrivateGetExtensionsInfoFunction
    : public DeveloperPrivateAPIFunction {
 public:
  DeveloperPrivateGetExtensionsInfoFunction();

  DeveloperPrivateGetExtensionsInfoFunction(
      const DeveloperPrivateGetExtensionsInfoFunction&) = delete;
  DeveloperPrivateGetExtensionsInfoFunction& operator=(
      const DeveloperPrivateGetExtensionsInfoFunction&) = delete;

  DECLARE_EXTENSION_FUNCTION("developerPrivate.getExtensionsInfo",
                             DEVELOPERPRIVATE_GETEXTENSIONSINFO)

 private:
  ~DeveloperPrivateGetExtensionsInfoFunction() override;
  ResponseAction Run() override;

  void OnInfosGenerated(
      std::vector<api::developer_private::ExtensionInfo> infos);

  std::unique_ptr<ExtensionInfoGenerator> info_generator_;
};

class DeveloperPrivateGetExtensionInfoFunction
    : public DeveloperPrivateAPIFunction {
 public:
  DeveloperPrivateGetExtensionInfoFunction();

  DeveloperPrivateGetExtensionInfoFunction(
      const DeveloperPrivateGetExtensionInfoFunction&) = delete;
  DeveloperPrivateGetExtensionInfoFunction& operator=(
      const DeveloperPrivateGetExtensionInfoFunction&) = delete;

  DECLARE_EXTENSION_FUNCTION("developerPrivate.getExtensionInfo",
                             DEVELOPERPRIVATE_GETEXTENSIONINFO)

 private:
  ~DeveloperPrivateGetExtensionInfoFunction() override;
  ResponseAction Run() override;

  void OnInfosGenerated(
      std::vector<api::developer_private::ExtensionInfo> infos);

  std::unique_ptr<ExtensionInfoGenerator> info_generator_;
};

class DeveloperPrivateGetExtensionSizeFunction
    : public DeveloperPrivateAPIFunction {
 public:
  DeveloperPrivateGetExtensionSizeFunction();

  DeveloperPrivateGetExtensionSizeFunction(
      const DeveloperPrivateGetExtensionSizeFunction&) = delete;
  DeveloperPrivateGetExtensionSizeFunction& operator=(
      const DeveloperPrivateGetExtensionSizeFunction&) = delete;

  DECLARE_EXTENSION_FUNCTION("developerPrivate.getExtensionSize",
                             DEVELOPERPRIVATE_GETEXTENSIONSIZE)

 private:
  ~DeveloperPrivateGetExtensionSizeFunction() override;
  ResponseAction Run() override;

  void OnSizeCalculated(const std::u16string& size);
};

class DeveloperPrivateGetProfileConfigurationFunction
    : public DeveloperPrivateAPIFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("developerPrivate.getProfileConfiguration",
                             DEVELOPERPRIVATE_GETPROFILECONFIGURATION)

 private:
  ~DeveloperPrivateGetProfileConfigurationFunction() override;
  ResponseAction Run() override;
};

class DeveloperPrivateUpdateProfileConfigurationFunction
    : public DeveloperPrivateAPIFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("developerPrivate.updateProfileConfiguration",
                             DEVELOPERPRIVATE_UPDATEPROFILECONFIGURATION)

 private:
  ~DeveloperPrivateUpdateProfileConfigurationFunction() override;
  ResponseAction Run() override;
};

class DeveloperPrivateUpdateExtensionConfigurationFunction
    : public DeveloperPrivateAPIFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("developerPrivate.updateExtensionConfiguration",
                             DEVELOPERPRIVATE_UPDATEEXTENSIONCONFIGURATION)

 protected:
  ~DeveloperPrivateUpdateExtensionConfigurationFunction() override;
  ResponseAction Run() override;
};

class DeveloperPrivateReloadFunction : public DeveloperPrivateAPIFunction,
                                       public ExtensionRegistryObserver,
                                       public LoadErrorReporter::Observer {
 public:
  DECLARE_EXTENSION_FUNCTION("developerPrivate.reload", DEVELOPERPRIVATE_RELOAD)

  DeveloperPrivateReloadFunction();

  DeveloperPrivateReloadFunction(const DeveloperPrivateReloadFunction&) =
      delete;
  DeveloperPrivateReloadFunction& operator=(
      const DeveloperPrivateReloadFunction&) = delete;

  // ExtensionRegistryObserver:
  void OnExtensionLoaded(content::BrowserContext* browser_context,
                         const Extension* extension) override;
  void OnShutdown(ExtensionRegistry* registry) override;

  // LoadErrorReporter::Observer:
  void OnLoadFailure(content::BrowserContext* browser_context,
                     const base::FilePath& file_path,
                     const std::string& error) override;

 protected:
  ~DeveloperPrivateReloadFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;

 private:
  // Callback once we parse a manifest error from a failed reload.
  void OnGotManifestError(const base::FilePath& file_path,
                          const std::string& error,
                          size_t line_number,
                          const std::string& manifest);

  // Clears the scoped observers.
  void ClearObservers();

  // The file path of the extension that's reloading.
  base::FilePath reloading_extension_path_;

  base::ScopedObservation<ExtensionRegistry, ExtensionRegistryObserver>
      registry_observation_{this};
  base::ScopedObservation<LoadErrorReporter, LoadErrorReporter::Observer>
      error_reporter_observation_{this};
};

class DeveloperPrivateShowPermissionsDialogFunction
    : public DeveloperPrivateAPIFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("developerPrivate.showPermissionsDialog",
                             DEVELOPERPRIVATE_PERMISSIONS)
  DeveloperPrivateShowPermissionsDialogFunction();

  DeveloperPrivateShowPermissionsDialogFunction(
      const DeveloperPrivateShowPermissionsDialogFunction&) = delete;
  DeveloperPrivateShowPermissionsDialogFunction& operator=(
      const DeveloperPrivateShowPermissionsDialogFunction&) = delete;

 protected:
  // DeveloperPrivateAPIFunction:
  ~DeveloperPrivateShowPermissionsDialogFunction() override;
  ResponseAction Run() override;

  void Finish();
};

class DeveloperPrivateChooseEntryFunction : public ExtensionFunction,
                                            public EntryPickerClient {
 protected:
  ~DeveloperPrivateChooseEntryFunction() override;
  bool ShowPicker(ui::SelectFileDialog::Type picker_type,
                  const std::u16string& select_title,
                  const ui::SelectFileDialog::FileTypeInfo& info,
                  int file_type_index);
};

class DeveloperPrivateLoadUnpackedFunction
    : public DeveloperPrivateChooseEntryFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("developerPrivate.loadUnpacked",
                             DEVELOPERPRIVATE_LOADUNPACKED)
  DeveloperPrivateLoadUnpackedFunction();

 protected:
  ~DeveloperPrivateLoadUnpackedFunction() override;
  ResponseAction Run() override;

  // EntryPickerClient:
  void FileSelected(const base::FilePath& path) override;
  void FileSelectionCanceled() override;

  // Callback for the UnpackedLoader.
  void OnLoadComplete(const Extension* extension,
                      const base::FilePath& file_path,
                      const std::string& error);

 private:
  void OnGotManifestError(const base::FilePath& file_path,
                          const std::string& error,
                          size_t line_number,
                          const std::string& manifest);

  // Whether or not we should fail quietly in the event of a load error.
  bool fail_quietly_ = false;

  // Whether we populate a developer_private::LoadError on load failure, as
  // opposed to simply passing the message in lastError.
  bool populate_error_ = false;

  // The identifier for the selected path when retrying an unpacked load.
  DeveloperPrivateAPI::UnpackedRetryId retry_guid_;
};

class DeveloperPrivateInstallDroppedFileFunction
    : public DeveloperPrivateAPIFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("developerPrivate.installDroppedFile",
                             DEVELOPERPRIVATE_INSTALLDROPPEDFILE)
  DeveloperPrivateInstallDroppedFileFunction();

  DeveloperPrivateInstallDroppedFileFunction(
      const DeveloperPrivateInstallDroppedFileFunction&) = delete;
  DeveloperPrivateInstallDroppedFileFunction& operator=(
      const DeveloperPrivateInstallDroppedFileFunction&) = delete;

 private:
  ~DeveloperPrivateInstallDroppedFileFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;
};

class DeveloperPrivateNotifyDragInstallInProgressFunction
    : public DeveloperPrivateAPIFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("developerPrivate.notifyDragInstallInProgress",
                             DEVELOPERPRIVATE_NOTIFYDRAGINSTALLINPROGRESS)

  DeveloperPrivateNotifyDragInstallInProgressFunction();

  DeveloperPrivateNotifyDragInstallInProgressFunction(
      const DeveloperPrivateNotifyDragInstallInProgressFunction&) = delete;
  DeveloperPrivateNotifyDragInstallInProgressFunction& operator=(
      const DeveloperPrivateNotifyDragInstallInProgressFunction&) = delete;

  ResponseAction Run() override;

  static void SetDropPathForTesting(base::FilePath* file_path);

 private:
  ~DeveloperPrivateNotifyDragInstallInProgressFunction() override;
};

class DeveloperPrivateChoosePathFunction
    : public DeveloperPrivateChooseEntryFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("developerPrivate.choosePath",
                             DEVELOPERPRIVATE_CHOOSEPATH)

 protected:
  ~DeveloperPrivateChoosePathFunction() override;
  ResponseAction Run() override;

  // EntryPickerClient:
  void FileSelected(const base::FilePath& path) override;
  void FileSelectionCanceled() override;
};

class DeveloperPrivatePackDirectoryFunction
    : public DeveloperPrivateAPIFunction,
      public PackExtensionJob::Client {
 public:
  DECLARE_EXTENSION_FUNCTION("developerPrivate.packDirectory",
                             DEVELOPERPRIVATE_PACKDIRECTORY)

  DeveloperPrivatePackDirectoryFunction();

  // ExtensionPackJob::Client implementation.
  void OnPackSuccess(const base::FilePath& crx_file,
                     const base::FilePath& key_file) override;
  void OnPackFailure(const std::string& error,
                     ExtensionCreator::ErrorType error_type) override;

 protected:
  ~DeveloperPrivatePackDirectoryFunction() override;
  ResponseAction Run() override;

 private:
  std::unique_ptr<PackExtensionJob> pack_job_;
  std::string item_path_str_;
  std::string key_path_str_;
};

class DeveloperPrivateIsProfileManagedFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("developerPrivate.isProfileManaged",
                             DEVELOPERPRIVATE_ISPROFILEMANAGED)

 protected:
  ~DeveloperPrivateIsProfileManagedFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;
};

class DeveloperPrivateLoadDirectoryFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("developerPrivate.loadDirectory",
                             DEVELOPERPRIVATE_LOADUNPACKEDCROS)

  DeveloperPrivateLoadDirectoryFunction();

 protected:
  ~DeveloperPrivateLoadDirectoryFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;

  ResponseAction LoadByFileSystemAPI(
      const ::storage::FileSystemURL& directory_url);

  void ClearExistingDirectoryContent(const base::FilePath& project_path);

  void ReadDirectoryByFileSystemAPI(const base::FilePath& project_path,
                                    const base::FilePath& destination_path);

  void ReadDirectoryByFileSystemAPICb(
      const base::FilePath& project_path,
      const base::FilePath& destination_path,
      base::File::Error result,
      ::storage::FileSystemOperation::FileEntryList file_list,
      bool has_more);

  void SnapshotFileCallback(
      const base::FilePath& target_path,
      base::File::Error result,
      const base::File::Info& file_info,
      const base::FilePath& platform_path,
      scoped_refptr<::storage::ShareableFileReference> file_ref);

  void CopyFile(const base::FilePath& src_path,
                const base::FilePath& dest_path);

  void Load();

  scoped_refptr<::storage::FileSystemContext> context_;

  // syncfs url representing the root of the folder to be copied.
  std::string project_base_url_;

  // physical path on disc of the folder to be copied.
  base::FilePath project_base_path_;

 private:
  int pending_copy_operations_count_;

  // This is set to false if any of the copyFile operations fail on
  // call of the API. It is returned as a response of the API call.
  bool success_;

  // Error string if |success_| is false.
  std::string error_;
};

class DeveloperPrivateRequestFileSourceFunction
    : public DeveloperPrivateAPIFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("developerPrivate.requestFileSource",
                             DEVELOPERPRIVATE_REQUESTFILESOURCE)
  DeveloperPrivateRequestFileSourceFunction();

 protected:
  ~DeveloperPrivateRequestFileSourceFunction() override;
  ResponseAction Run() override;

 private:
  void Finish(const std::string& file_contents);

  absl::optional<api::developer_private::RequestFileSource::Params> params_;
};

class DeveloperPrivateOpenDevToolsFunction
    : public DeveloperPrivateAPIFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("developerPrivate.openDevTools",
                             DEVELOPERPRIVATE_OPENDEVTOOLS)
  DeveloperPrivateOpenDevToolsFunction();

 protected:
  ~DeveloperPrivateOpenDevToolsFunction() override;
  ResponseAction Run() override;
};

class DeveloperPrivateDeleteExtensionErrorsFunction
    : public DeveloperPrivateAPIFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("developerPrivate.deleteExtensionErrors",
                             DEVELOPERPRIVATE_DELETEEXTENSIONERRORS)

 protected:
  ~DeveloperPrivateDeleteExtensionErrorsFunction() override;
  ResponseAction Run() override;
};

class DeveloperPrivateRepairExtensionFunction
    : public DeveloperPrivateAPIFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("developerPrivate.repairExtension",
                             DEVELOPERPRIVATE_REPAIREXTENSION)

 protected:
  ~DeveloperPrivateRepairExtensionFunction() override;
  ResponseAction Run() override;

  void OnReinstallComplete(bool success,
                           const std::string& error,
                           webstore_install::Result result);
};

class DeveloperPrivateShowOptionsFunction : public DeveloperPrivateAPIFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("developerPrivate.showOptions",
                             DEVELOPERPRIVATE_SHOWOPTIONS)

 protected:
  ~DeveloperPrivateShowOptionsFunction() override;
  ResponseAction Run() override;
};

class DeveloperPrivateShowPathFunction : public DeveloperPrivateAPIFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("developerPrivate.showPath",
                             DEVELOPERPRIVATE_SHOWPATH)

 protected:
  ~DeveloperPrivateShowPathFunction() override;
  ResponseAction Run() override;
};

class DeveloperPrivateSetShortcutHandlingSuspendedFunction
    : public DeveloperPrivateAPIFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("developerPrivate.setShortcutHandlingSuspended",
                             DEVELOPERPRIVATE_SETSHORTCUTHANDLINGSUSPENDED)

 protected:
  ~DeveloperPrivateSetShortcutHandlingSuspendedFunction() override;
  ResponseAction Run() override;
};

class DeveloperPrivateUpdateExtensionCommandFunction
    : public DeveloperPrivateAPIFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("developerPrivate.updateExtensionCommand",
                             DEVELOPERPRIVATE_UPDATEEXTENSIONCOMMAND)

 protected:
  ~DeveloperPrivateUpdateExtensionCommandFunction() override;
  ResponseAction Run() override;
};

class DeveloperPrivateAddHostPermissionFunction
    : public DeveloperPrivateAPIFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("developerPrivate.addHostPermission",
                             DEVELOPERPRIVATE_ADDHOSTPERMISSION)
  DeveloperPrivateAddHostPermissionFunction();

  DeveloperPrivateAddHostPermissionFunction(
      const DeveloperPrivateAddHostPermissionFunction&) = delete;
  DeveloperPrivateAddHostPermissionFunction& operator=(
      const DeveloperPrivateAddHostPermissionFunction&) = delete;

 private:
  ~DeveloperPrivateAddHostPermissionFunction() override;

  ResponseAction Run() override;

  void OnRuntimePermissionsGranted();
};

class DeveloperPrivateRemoveHostPermissionFunction
    : public DeveloperPrivateAPIFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("developerPrivate.removeHostPermission",
                             DEVELOPERPRIVATE_REMOVEHOSTPERMISSION)
  DeveloperPrivateRemoveHostPermissionFunction();

  DeveloperPrivateRemoveHostPermissionFunction(
      const DeveloperPrivateRemoveHostPermissionFunction&) = delete;
  DeveloperPrivateRemoveHostPermissionFunction& operator=(
      const DeveloperPrivateRemoveHostPermissionFunction&) = delete;

 private:
  ~DeveloperPrivateRemoveHostPermissionFunction() override;

  ResponseAction Run() override;

  void OnRuntimePermissionsRevoked();
};

class DeveloperPrivateGetUserSiteSettingsFunction
    : public DeveloperPrivateAPIFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("developerPrivate.getUserSiteSettings",
                             DEVELOPERPRIVATE_GETUSERSITESETTINGS)
  DeveloperPrivateGetUserSiteSettingsFunction();

  DeveloperPrivateGetUserSiteSettingsFunction(
      const DeveloperPrivateGetUserSiteSettingsFunction&) = delete;
  DeveloperPrivateGetUserSiteSettingsFunction& operator=(
      const DeveloperPrivateGetUserSiteSettingsFunction&) = delete;

 private:
  ~DeveloperPrivateGetUserSiteSettingsFunction() override;

  ResponseAction Run() override;
};

class DeveloperPrivateAddUserSpecifiedSitesFunction
    : public DeveloperPrivateAPIFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("developerPrivate.addUserSpecifiedSites",
                             DEVELOPERPRIVATE_ADDUSERSPECIFIEDSITES)
  DeveloperPrivateAddUserSpecifiedSitesFunction();

  DeveloperPrivateAddUserSpecifiedSitesFunction(
      const DeveloperPrivateAddUserSpecifiedSitesFunction&) = delete;
  DeveloperPrivateAddUserSpecifiedSitesFunction& operator=(
      const DeveloperPrivateAddUserSpecifiedSitesFunction&) = delete;

 private:
  ~DeveloperPrivateAddUserSpecifiedSitesFunction() override;

  ResponseAction Run() override;
};

class DeveloperPrivateRemoveUserSpecifiedSitesFunction
    : public DeveloperPrivateAPIFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("developerPrivate.removeUserSpecifiedSites",
                             DEVELOPERPRIVATE_REMOVEUSERSPECIFIEDSITES)
  DeveloperPrivateRemoveUserSpecifiedSitesFunction();

  DeveloperPrivateRemoveUserSpecifiedSitesFunction(
      const DeveloperPrivateRemoveUserSpecifiedSitesFunction&) = delete;
  DeveloperPrivateRemoveUserSpecifiedSitesFunction& operator=(
      const DeveloperPrivateRemoveUserSpecifiedSitesFunction&) = delete;

 private:
  ~DeveloperPrivateRemoveUserSpecifiedSitesFunction() override;

  ResponseAction Run() override;
};

class DeveloperPrivateGetUserAndExtensionSitesByEtldFunction
    : public DeveloperPrivateAPIFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("developerPrivate.getUserAndExtensionSitesByEtld",
                             DEVELOPERPRIVATE_GETUSERANDEXTENSIONSITESBYETLD)
  DeveloperPrivateGetUserAndExtensionSitesByEtldFunction();

  DeveloperPrivateGetUserAndExtensionSitesByEtldFunction(
      const DeveloperPrivateGetUserAndExtensionSitesByEtldFunction&) = delete;
  DeveloperPrivateGetUserAndExtensionSitesByEtldFunction& operator=(
      const DeveloperPrivateGetUserAndExtensionSitesByEtldFunction&) = delete;

 private:
  ~DeveloperPrivateGetUserAndExtensionSitesByEtldFunction() override;

  ResponseAction Run() override;
};

class DeveloperPrivateGetMatchingExtensionsForSiteFunction
    : public DeveloperPrivateAPIFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("developerPrivate.getMatchingExtensionsForSite",
                             DEVELOPERPRIVATE_GETMATCHINGEXTENSIONSFORSITE)
  DeveloperPrivateGetMatchingExtensionsForSiteFunction();

  DeveloperPrivateGetMatchingExtensionsForSiteFunction(
      const DeveloperPrivateGetMatchingExtensionsForSiteFunction&) = delete;
  DeveloperPrivateGetMatchingExtensionsForSiteFunction& operator=(
      const DeveloperPrivateGetMatchingExtensionsForSiteFunction&) = delete;

 private:
  ~DeveloperPrivateGetMatchingExtensionsForSiteFunction() override;

  ResponseAction Run() override;
};

class DeveloperPrivateUpdateSiteAccessFunction
    : public DeveloperPrivateAPIFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("developerPrivate.updateSiteAccess",
                             DEVELOPERPRIVATE_UPDATESITEACCESS)
  DeveloperPrivateUpdateSiteAccessFunction();

  DeveloperPrivateUpdateSiteAccessFunction(
      const DeveloperPrivateUpdateSiteAccessFunction&) = delete;
  DeveloperPrivateUpdateSiteAccessFunction& operator=(
      const DeveloperPrivateUpdateSiteAccessFunction&) = delete;

 private:
  ~DeveloperPrivateUpdateSiteAccessFunction() override;

  ResponseAction Run() override;

  void OnSiteSettingsUpdated();
};

}  // namespace api

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_DEVELOPER_PRIVATE_DEVELOPER_PRIVATE_API_H_
