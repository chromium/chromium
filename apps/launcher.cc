// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "apps/launcher.h"

#include <memory>
#include <set>
#include <utility>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/url_constants.h"
#include "extensions/browser/api/app_runtime/app_runtime_api.h"
#include "extensions/browser/api/file_handlers/app_file_handler_util.h"
#include "extensions/browser/api/file_handlers/directory_util.h"
#include "extensions/browser/api/file_handlers/mime_util.h"
#include "extensions/browser/entry_info.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/granted_file_entry.h"
#include "extensions/browser/lazy_background_task_queue.h"
#include "extensions/browser/process_manager.h"
#include "extensions/common/api/app_runtime.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_handlers/kiosk_mode_info.h"
#include "extensions/common/permissions/api_permission.h"
#include "extensions/common/permissions/permissions_data.h"
#include "net/base/filename_util.h"
#include "url/gurl.h"

#if defined(OS_CHROMEOS)
#include "components/user_manager/user_manager.h"
#endif

namespace app_runtime = extensions::api::app_runtime;

using content::BrowserThread;
using extensions::AppRuntimeEventRouter;
using extensions::api::app_runtime::PlayStoreStatus;
using extensions::app_file_handler_util::CreateFileEntry;
using extensions::app_file_handler_util::FileHandlerCanHandleEntry;
using extensions::app_file_handler_util::FileHandlerForId;
using extensions::app_file_handler_util::HasFileSystemWritePermission;
using extensions::app_file_handler_util::PrepareFilesForWritableApp;
using extensions::EventRouter;
using extensions::Extension;
using extensions::ExtensionHost;
using extensions::GrantedFileEntry;

namespace apps {

namespace {

const char kFallbackMimeType[] = "application/octet-stream";

bool DoMakePathAbsolute(const base::FilePath& current_directory,
                        base::FilePath* file_path) {
  DCHECK(file_path);
  if (file_path->IsAbsolute())
    return true;

  if (current_directory.empty()) {
    base::FilePath absolute_path = base::MakeAbsoluteFilePath(*file_path);
    if (absolute_path.empty())
      return false;
    *file_path = absolute_path;
    return true;
  }

  if (!current_directory.IsAbsolute())
    return false;

  *file_path = current_directory.Append(*file_path);
  return true;
}

// Class to handle launching of platform apps to open specific paths.
// An instance of this class is created for each launch. The lifetime of these
// instances is managed by reference counted pointers. As long as an instance
// has outstanding tasks on a message queue it will be retained; once all
// outstanding tasks are completed it will be deleted.
class PlatformAppPathLauncher
    : public base::RefCountedThreadSafe<PlatformAppPathLauncher> {
 public:
  PlatformAppPathLauncher(content::BrowserContext* context,
                          const Extension* app,
                          const std::vector<base::FilePath>& entry_paths)
      : context_(context),
        extension_id(app->id()),
        entry_paths_(entry_paths),
        mime_type_collector_(context),
        is_directory_collector_(context) {}

  PlatformAppPathLauncher(content::BrowserContext* context,
                          const Extension* app,
                          const base::FilePath& file_path)
      : context_(context),
        extension_id(app->id()),
        mime_type_collector_(context),
        is_directory_collector_(context) {
    if (!file_path.empty())
      entry_paths_.push_back(file_path);
  }

  void set_action_data(std::unique_ptr<app_runtime::ActionData> action_data) {
    action_data_ = std::move(action_data);
  }

  void set_launch_source(extensions::AppLaunchSource launch_source) {
    launch_source_ = launch_source;
  }

  void Launch() {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);

    const Extension* app = GetExtension();
    if (!app)
      return;

    if (entry_paths_.empty()) {
      LaunchWithNoLaunchData();
      return;
    }

    for (size_t i = 0; i < entry_paths_.size(); ++i) {
      DCHECK(entry_paths_[i].IsAbsolute());
    }

    is_directory_collector_.CollectForEntriesPaths(
        entry_paths_,
        base::Bind(&PlatformAppPathLauncher::OnAreDirectoriesCollected, this,
                   HasFileSystemWritePermission(app)));
  }

  void LaunchWithHandler(const std::string& handler_id) {
    handler_id_ = handler_id;
    Launch();
  }

  void LaunchWithRelativePath(const base::FilePath& current_directory) {
    base::PostTaskWithTraits(
        FROM_HERE,
        {base::TaskPriority::USER_VISIBLE, base::MayBlock(),
         base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
        base::Bind(&PlatformAppPathLauncher::MakePathAbsolute, this,
                   current_directory));
  }

 private:
  friend class base::RefCountedThreadSafe<PlatformAppPathLauncher>;

  virtual ~PlatformAppPathLauncher() = default;

  void MakePathAbsolute(const base::FilePath& current_directory) {
    for (std::vector<base::FilePath>::iterator it = entry_paths_.begin();
         it != entry_paths_.end(); ++it) {
      if (!DoMakePathAbsolute(current_directory, &*it)) {
        LOG(WARNING) << "Cannot make absolute path from " << it->value();
        base::PostTaskWithTraits(
            FROM_HERE, {BrowserThread::UI},
            base::Bind(&PlatformAppPathLauncher::LaunchWithNoLaunchData, this));
        return;
      }
    }

    base::PostTaskWithTraits(
        FROM_HERE, {BrowserThread::UI},
        base::Bind(&PlatformAppPathLauncher::Launch, this));
  }

  void OnFilesValid(std::unique_ptr<std::set<base::FilePath>> directory_paths) {
    mime_type_collector_.CollectForLocalPaths(
        entry_paths_,
        base::Bind(
            &PlatformAppPathLauncher::OnAreDirectoriesAndMimeTypesCollected,
            this, base::Passed(std::move(directory_paths))));
  }

  void OnFilesInvalid(const base::FilePath& /* error_path */) {
    LaunchWithNoLaunchData();
  }

  void LaunchWithNoLaunchData() {
    // This method is required as an entry point on the UI thread.
    DCHECK_CURRENTLY_ON(BrowserThread::UI);

    const Extension* app = GetExtension();
    if (!app)
      return;

    std::unique_ptr<app_runtime::LaunchData> launch_data =
        std::make_unique<app_runtime::LaunchData>();
    launch_data->action_data = std::move(action_data_);

    AppRuntimeEventRouter::DispatchOnLaunchedEvent(
        context_, app, launch_source_, std::move(launch_data));
  }

  void OnAreDirectoriesCollected(
      bool has_file_system_write_permission,
      std::unique_ptr<std::set<base::FilePath>> directory_paths) {
    if (has_file_system_write_permission) {
      std::set<base::FilePath>* const directory_paths_ptr =
          directory_paths.get();
      PrepareFilesForWritableApp(
          entry_paths_, context_, *directory_paths_ptr,
          base::Bind(&PlatformAppPathLauncher::OnFilesValid, this,
                     base::Passed(std::move(directory_paths))),
          base::Bind(&PlatformAppPathLauncher::OnFilesInvalid, this));
      return;
    }

    OnFilesValid(std::move(directory_paths));
  }

  void OnAreDirectoriesAndMimeTypesCollected(
      std::unique_ptr<std::set<base::FilePath>> directory_paths,
      std::unique_ptr<std::vector<std::string>> mime_types) {
    DCHECK(entry_paths_.size() == mime_types->size());
    // If fetching a mime type failed, then use a fallback one.
    for (size_t i = 0; i < entry_paths_.size(); ++i) {
      const std::string mime_type =
          !(*mime_types)[i].empty() ? (*mime_types)[i] : kFallbackMimeType;
      bool is_directory =
          directory_paths->find(entry_paths_[i]) != directory_paths->end();
      entries_.push_back(
          extensions::EntryInfo(entry_paths_[i], mime_type, is_directory));
    }

    const Extension* app = GetExtension();
    if (!app)
      return;

    // Find file handler from the platform app for the file being opened.
    const extensions::FileHandlerInfo* handler = NULL;
    if (!handler_id_.empty()) {
      handler = FileHandlerForId(*app, handler_id_);
      if (handler) {
        for (size_t i = 0; i < entry_paths_.size(); ++i) {
          if (!FileHandlerCanHandleEntry(*handler, entries_[i])) {
            LOG(WARNING)
                << "Extension does not provide a valid file handler for "
                << entry_paths_[i].value();
            handler = NULL;
            break;
          }
        }
      }
    } else {
      const std::vector<const extensions::FileHandlerInfo*>& handlers =
          extensions::app_file_handler_util::FindFileHandlersForEntries(
              *app, entries_);
      if (!handlers.empty())
        handler = handlers[0];
    }

    // If this app doesn't have a file handler that supports the file, launch
    // with no launch data.
    if (!handler) {
      LOG(WARNING) << "Extension does not provide a valid file handler.";
      LaunchWithNoLaunchData();
      return;
    }

    if (handler_id_.empty())
      handler_id_ = handler->id;

    // Access needs to be granted to the file for the process associated with
    // the extension. To do this the ExtensionHost is needed. This might not be
    // available, or it might be in the process of being unloaded, in which case
    // the lazy background task queue is used to load the extension and then
    // call back to us.
    extensions::LazyBackgroundTaskQueue* const queue =
        extensions::LazyBackgroundTaskQueue::Get(context_);
    if (queue->ShouldEnqueueTask(context_, app)) {
      queue->AddPendingTask(
          context_, extension_id,
          base::Bind(&PlatformAppPathLauncher::GrantAccessToFilesAndLaunch,
                     this));
      return;
    }

    extensions::ProcessManager* const process_manager =
        extensions::ProcessManager::Get(context_);
    ExtensionHost* const host =
        process_manager->GetBackgroundHostForExtension(extension_id);
    DCHECK(host);
    GrantAccessToFilesAndLaunch(host);
  }

  void GrantAccessToFilesAndLaunch(ExtensionHost* host) {
    const Extension* app = GetExtension();
    if (!app)
      return;

    // If there was an error loading the app page, |host| will be NULL.
    if (!host) {
      LOG(ERROR) << "Could not load app page for " << extension_id;
      return;
    }

    std::vector<GrantedFileEntry> granted_entries;
    for (size_t i = 0; i < entry_paths_.size(); ++i) {
      granted_entries.push_back(
          CreateFileEntry(context_, app, host->render_process_host()->GetID(),
                          entries_[i].path, entries_[i].is_directory));
    }

    AppRuntimeEventRouter::DispatchOnLaunchedEventWithFileEntries(
        context_, app, launch_source_, handler_id_, entries_, granted_entries,
        std::move(action_data_));
  }

  const Extension* GetExtension() const {
    return extensions::ExtensionRegistry::Get(context_)->GetExtensionById(
        extension_id, extensions::ExtensionRegistry::EVERYTHING);
  }

  // The browser context the app should be run in.
  content::BrowserContext* context_;
  // The id of the extension providing the app. A pointer to the extension is
  // not kept as the extension may be unloaded and deleted during the course of
  // the launch.
  const std::string extension_id;
  extensions::AppLaunchSource launch_source_ = extensions::SOURCE_FILE_HANDLER;
  std::unique_ptr<app_runtime::ActionData> action_data_;
  // A list of files and directories to be passed through to the app.
  std::vector<base::FilePath> entry_paths_;
  // A corresponding list with EntryInfo for every base::FilePath in
  // entry_paths_.
  std::vector<extensions::EntryInfo> entries_;
  // The ID of the file handler used to launch the app.
  std::string handler_id_;
  extensions::app_file_handler_util::MimeTypeCollector mime_type_collector_;
  extensions::app_file_handler_util::IsDirectoryCollector
      is_directory_collector_;

  DISALLOW_COPY_AND_ASSIGN(PlatformAppPathLauncher);
};

}  // namespace

void LaunchPlatformAppWithCommandLine(content::BrowserContext* context,
                                      const extensions::Extension* app,
                                      const base::CommandLine& command_line,
                                      const base::FilePath& current_directory,
                                      extensions::AppLaunchSource source,
                                      PlayStoreStatus play_store_status) {
  LaunchPlatformAppWithCommandLineAndLaunchId(context, app, "", command_line,
                                              current_directory, source,
                                              play_store_status);
}

void LaunchPlatformAppWithCommandLineAndLaunchId(
    content::BrowserContext* context,
    const extensions::Extension* app,
    const std::string& launch_id,
    const base::CommandLine& command_line,
    const base::FilePath& current_directory,
    extensions::AppLaunchSource source,
    PlayStoreStatus play_store_status) {
  // An app with "kiosk_only" should not be installed and launched
  // outside of ChromeOS kiosk mode in the first place. This is a defensive
  // check in case this scenario does occur.
  if (extensions::KioskModeInfo::IsKioskOnly(app)) {
    bool in_kiosk_mode = false;
#if defined(OS_CHROMEOS)
    user_manager::UserManager* user_manager = user_manager::UserManager::Get();
    in_kiosk_mode = user_manager && user_manager->IsLoggedInAsKioskApp();
#endif
    if (!in_kiosk_mode) {
      LOG(ERROR) << "App with 'kiosk_only' attribute must be run in "
                 << " ChromeOS kiosk mode.";
      NOTREACHED();
      return;
    }
  }

#if defined(OS_WIN)
  base::CommandLine::StringType about_blank_url(
      base::ASCIIToUTF16(url::kAboutBlankURL));
#else
  base::CommandLine::StringType about_blank_url(url::kAboutBlankURL);
#endif
  base::CommandLine::StringVector args = command_line.GetArgs();
  // Browser tests will add about:blank to the command line. This should
  // never be interpreted as a file to open, as doing so with an app that
  // has write access will result in a file 'about' being created, which
  // causes problems on the bots.
  if (args.empty() || (command_line.HasSwitch(switches::kTestType) &&
                       args[0] == about_blank_url)) {
    std::unique_ptr<app_runtime::LaunchData> launch_data =
        std::make_unique<app_runtime::LaunchData>();
    if (play_store_status != PlayStoreStatus::PLAY_STORE_STATUS_UNKNOWN)
      launch_data->play_store_status = play_store_status;
    if (!launch_id.empty())
      launch_data->id.reset(new std::string(launch_id));
    AppRuntimeEventRouter::DispatchOnLaunchedEvent(context, app, source,
                                                   std::move(launch_data));
    return;
  }

  base::FilePath file_path(command_line.GetArgs()[0]);
  scoped_refptr<PlatformAppPathLauncher> launcher =
      new PlatformAppPathLauncher(context, app, file_path);
  launcher->LaunchWithRelativePath(current_directory);
}

void LaunchPlatformAppWithPath(content::BrowserContext* context,
                               const Extension* app,
                               const base::FilePath& file_path) {
  scoped_refptr<PlatformAppPathLauncher> launcher =
      new PlatformAppPathLauncher(context, app, file_path);
  launcher->Launch();
}

void LaunchPlatformAppWithAction(
    content::BrowserContext* context,
    const extensions::Extension* app,
    std::unique_ptr<app_runtime::ActionData> action_data,
    const base::FilePath& file_path) {
  CHECK(!action_data || !action_data->is_lock_screen_action ||
        !*action_data->is_lock_screen_action ||
        app->permissions_data()->HasAPIPermission(
            extensions::APIPermission::kLockScreen))
      << "Launching lock screen action handler requires lockScreen permission.";

  scoped_refptr<PlatformAppPathLauncher> launcher =
      new PlatformAppPathLauncher(context, app, file_path);
  launcher->set_action_data(std::move(action_data));
  launcher->set_launch_source(extensions::AppLaunchSource::SOURCE_UNTRACKED);
  launcher->Launch();
}

void LaunchPlatformApp(content::BrowserContext* context,
                       const Extension* app,
                       extensions::AppLaunchSource source) {
  LaunchPlatformAppWithCommandLine(
      context, app, base::CommandLine(base::CommandLine::NO_PROGRAM),
      base::FilePath(), source);
}

void LaunchPlatformAppWithFileHandler(
    content::BrowserContext* context,
    const Extension* app,
    const std::string& handler_id,
    const std::vector<base::FilePath>& entry_paths) {
  scoped_refptr<PlatformAppPathLauncher> launcher =
      new PlatformAppPathLauncher(context, app, entry_paths);
  launcher->LaunchWithHandler(handler_id);
}

void RestartPlatformApp(content::BrowserContext* context,
                        const Extension* app) {
  EventRouter* event_router = EventRouter::Get(context);
  bool listening_to_restart = event_router->ExtensionHasEventListener(
      app->id(), app_runtime::OnRestarted::kEventName);

  if (listening_to_restart) {
    AppRuntimeEventRouter::DispatchOnRestartedEvent(context, app);
    return;
  }

  extensions::ExtensionPrefs* extension_prefs =
      extensions::ExtensionPrefs::Get(context);
  bool had_windows = extension_prefs->IsActive(app->id());
  extension_prefs->SetIsActive(app->id(), false);
  bool listening_to_launch = event_router->ExtensionHasEventListener(
      app->id(), app_runtime::OnLaunched::kEventName);

  if (listening_to_launch && had_windows) {
    AppRuntimeEventRouter::DispatchOnLaunchedEvent(
        context, app, extensions::SOURCE_RESTART, nullptr);
  }
}

void LaunchPlatformAppWithUrl(content::BrowserContext* context,
                              const Extension* app,
                              const std::string& handler_id,
                              const GURL& url,
                              const GURL& referrer_url) {
  AppRuntimeEventRouter::DispatchOnLaunchedEventWithUrl(
      context, app, handler_id, url, referrer_url);
}

}  // namespace apps
