// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "apps/launcher.h"

#include <memory>
#include <set>
#include <utility>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/notreached.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/services/app_service/public/cpp/file_handler_info.h"
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
#include "extensions/browser/lazy_context_id.h"
#include "extensions/browser/lazy_context_task_queue.h"
#include "extensions/browser/process_manager.h"
#include "extensions/common/api/app_runtime.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/manifest_handlers/kiosk_mode_info.h"
#include "extensions/common/permissions/api_permission.h"
#include "extensions/common/permissions/permissions_data.h"
#include "net/base/filename_util.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "components/app_restore/app_launch_info.h"
#include "components/app_restore/full_restore_utils.h"
#include "components/user_manager/user_manager.h"
#endif

namespace app_runtime = extensions::api::app_runtime;

using content::BrowserThread;
using extensions::AppRuntimeEventRouter;
using extensions::EventRouter;
using extensions::Extension;
using extensions::ExtensionHost;
using extensions::GrantedFileEntry;
using extensions::app_file_handler_util::CreateEntryInfos;
using extensions::app_file_handler_util::CreateFileEntry;
using extensions::app_file_handler_util::FileHandlerCanHandleEntry;
using extensions::app_file_handler_util::FileHandlerForId;
using extensions::app_file_handler_util::HasFileSystemWritePermission;
using extensions::app_file_handler_util::PrepareFilesForWritableApp;

namespace apps {

namespace {

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
  PlatformAppPathLauncher(const PlatformAppPathLauncher&) = delete;
  PlatformAppPathLauncher& operator=(const PlatformAppPathLauncher&) = delete;

  void set_action_data(std::optional<app_runtime::ActionData> action_data) {
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
      LaunchWithBasicData();
      return;
    }

    for (size_t i = 0; i < entry_paths_.size(); ++i) {
      DCHECK(entry_paths_[i].IsAbsolute());
    }

    is_directory_collector_.CollectForEntriesPaths(
        entry_paths_,
        base::BindOnce(&PlatformAppPathLauncher::OnAreDirectoriesCollected,
                       this, HasFileSystemWritePermission(app)));
  }

  void LaunchWithHandler(const std::string& handler_id) {
    handler_id_ = handler_id;
    Launch();
  }

  void LaunchWithRelativePath(const base::FilePath& current_directory) {
    base::ThreadPool::PostTask(
        FROM_HERE,
        {base::TaskPriority::USER_VISIBLE, base::MayBlock(),
         base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
        base::BindOnce(&PlatformAppPathLauncher::MakePathAbsolute, this,
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
        content::GetUIThreadTaskRunner({})->PostTask(
            FROM_HERE,
            base::BindOnce(&PlatformAppPathLauncher::LaunchWithBasicData,
                           this));
        return;
      }
    }

    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(&PlatformAppPathLauncher::Launch, this));
  }

  void OnFilesValid(std::unique_ptr<std::set<base::FilePath>> directory_paths) {
    mime_type_collector_.CollectForLocalPaths(
        entry_paths_,
        base::BindOnce(
            &PlatformAppPathLauncher::OnAreDirectoriesAndMimeTypesCollected,
            this, std::move(directory_paths)));
  }

  void OnFilesInvalid(const base::FilePath& /* error_path */) {
    LaunchWithBasicData();
  }

  void LaunchWithBasicData() {
    // This method is required as an entry point on the UI thread.
    DCHECK_CURRENTLY_ON(BrowserThread::UI);

    const Extension* app = GetExtension();
    if (!app)
      return;

    app_runtime::LaunchData launch_data;

    // TODO(crbug.com/40235429): This conditional block is being added here
    // temporarily, and should be removed once the underlying type of
    // |launch_data.action_data| is wrapped with std::optional<T>.
    if (action_data_) {
      launch_data.action_data = std::move(*action_data_);
      action_data_.reset();
    }
    if (!handler_id_.empty())
      launch_data.id = handler_id_;

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
          base::BindOnce(&PlatformAppPathLauncher::OnFilesValid, this,
                         std::move(directory_paths)),
          base::BindOnce(&PlatformAppPathLauncher::OnFilesInvalid, this));
      return;
    }

    OnFilesValid(std::move(directory_paths));
  }

  void OnAreDirectoriesAndMimeTypesCollected(
      std::unique_ptr<std::set<base::FilePath>> directory_paths,
      std::unique_ptr<std::vector<std::string>> mime_types) {
    // If mime type fetch fails then the following provides a fallback.
    entries_ = CreateEntryInfos(entry_paths_, *mime_types, *directory_paths);

    const Extension* app = GetExtension();
    if (!app)
      return;

    // Find file handler from the platform app for the file being opened.
    const FileHandlerInfo* handler = nullptr;
    if (!handler_id_.empty()) {
      handler = FileHandlerForId(*app, handler_id_);
      if (handler) {
        for (size_t i = 0; i < entry_paths_.size(); ++i) {
          if (!FileHandlerCanHandleEntry(*handler, entries_[i])) {
            LOG(WARNING)
                << "Extension does not provide a valid file handler for "
                << entry_paths_[i].value();
            handler = nullptr;
            break;
          }
        }
      }
    } else {
      const std::vector<extensions::FileHandlerMatch> handlers =
          extensions::app_file_handler_util::FindFileHandlerMatchesForEntries(
              *app, entries_);
      if (!handlers.empty())
        handler = handlers[0].handler;
    }

    // If this app doesn't have a file handler that supports the file, launch
    // with no launch data.
    if (!handler) {
      LOG(WARNING) << "Extension does not provide a valid file handler.";
      LaunchWithBasicData();
      return;
    }

    if (handler_id_.empty())
      handler_id_ = handler->id;

    // Access needs to be granted to the file for the process associated with
    // the extension. To do this the ExtensionHost is needed. This might not be
    // available, or it might be in the process of being unloaded, in which case
    // the lazy background task queue is used to load the extension and then
    // call back to us.
    const auto context_id =
        extensions::LazyContextId::ForExtension(context_, app);
    CHECK(context_id.IsForBackgroundPage());
    extensions::LazyContextTaskQueue* const queue = context_id.GetTaskQueue();
    if (queue->ShouldEnqueueTask(context_, app)) {
      queue->AddPendingTask(
          context_id,
          base::BindOnce(&PlatformAppPathLauncher::GrantAccessToFilesAndLaunch,
                         this));
      return;
    }

    extensions::ProcessManager* const process_manager =
        extensions::ProcessManager::Get(context_);
    ExtensionHost* const host =
        process_manager->GetBackgroundHostForExtension(extension_id);
    DCHECK(host);
    GrantAccessToFilesAndLaunch(
        std::make_unique<extensions::LazyContextTaskQueue::ContextInfo>(host));
  }

  void GrantAccessToFilesAndLaunch(
      std::unique_ptr<extensions::LazyContextTaskQueue::ContextInfo>
          context_info) {
    const Extension* app = GetExtension();
    if (!app)
      return;

    // If there was an error loading the app page, |context_info| will be NULL.
    if (!context_info) {
      LOG(ERROR) << "Could not load app page for " << extension_id;
      return;
    }

    std::vector<GrantedFileEntry> granted_entries;
    for (size_t i = 0; i < entry_paths_.size(); ++i) {
      granted_entries.push_back(CreateFileEntry(
          context_, app, context_info->render_process_host->GetID(),
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
  raw_ptr<content::BrowserContext> context_;
  // The id of the extension providing the app. A pointer to the extension is
  // not kept as the extension may be unloaded and deleted during the course of
  // the launch.
  const extensions::ExtensionId extension_id;
  extensions::AppLaunchSource launch_source_ =
      extensions::AppLaunchSource::kSourceFileHandler;
  std::optional<app_runtime::ActionData> action_data_;
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
};

}  // namespace

void LaunchPlatformAppWithCommandLine(content::BrowserContext* context,
                                      const extensions::Extension* app,
                                      const base::CommandLine& command_line,
                                      const base::FilePath& current_directory,
                                      extensions::AppLaunchSource source) {
  LaunchPlatformAppWithCommandLineAndLaunchId(context, app, "", command_line,
                                              current_directory, source);
}

void LaunchPlatformAppWithCommandLineAndLaunchId(
    content::BrowserContext* context,
    const extensions::Extension* app,
    const std::string& launch_id,
    const base::CommandLine& command_line,
    const base::FilePath& current_directory,
    extensions::AppLaunchSource source) {
  // An app with "kiosk_only" should not be installed and launched
  // outside of ChromeOS kiosk mode in the first place. This is a defensive
  // check in case this scenario does occur.
  if (extensions::KioskModeInfo::IsKioskOnly(app)) {
    bool in_kiosk_mode = false;
#if BUILDFLAG(IS_CHROMEOS_ASH)
    user_manager::UserManager* user_manager = user_manager::UserManager::Get();
    in_kiosk_mode = user_manager && user_manager->IsLoggedInAsKioskApp();
#endif
    if (!in_kiosk_mode) {
      NOTREACHED() << "App with 'kiosk_only' attribute must be run in "
                   << " ChromeOS kiosk mode.";
    }
  }

#if BUILDFLAG(IS_WIN)
  base::CommandLine::StringType about_blank_url(
      base::ASCIIToWide(url::kAboutBlankURL));
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
    app_runtime::LaunchData launch_data;
    if (!launch_id.empty())
      launch_data.id = launch_id;
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
  auto launcher =
      base::MakeRefCounted<PlatformAppPathLauncher>(context, app, file_path);
  launcher->Launch();
}

void LaunchPlatformAppWithFilePaths(
    content::BrowserContext* context,
    const extensions::Extension* app,
    const std::vector<base::FilePath>& file_paths) {
  auto launcher =
      base::MakeRefCounted<PlatformAppPathLauncher>(context, app, file_paths);
  launcher->Launch();
}

void LaunchPlatformAppWithAction(content::BrowserContext* context,
                                 const extensions::Extension* app,
                                 app_runtime::ActionData action_data) {
  CHECK(!action_data.is_lock_screen_action ||
        !*action_data.is_lock_screen_action ||
        app->permissions_data()->HasAPIPermission(
            extensions::mojom::APIPermissionID::kLockScreen))
      << "Launching lock screen action handler requires lockScreen permission.";

  scoped_refptr<PlatformAppPathLauncher> launcher =
      new PlatformAppPathLauncher(context, app, base::FilePath());
  launcher->set_action_data(std::move(action_data));
  launcher->set_launch_source(extensions::AppLaunchSource::kSourceUntracked);
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
#if BUILDFLAG(IS_CHROMEOS_ASH)
  auto launch_info = std::make_unique<app_restore::AppLaunchInfo>(
      app->id(), handler_id, entry_paths);
  full_restore::SaveAppLaunchInfo(context->GetPath(), std::move(launch_info));
#endif

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
        context, app, extensions::AppLaunchSource::kSourceRestart,
        std::nullopt);
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
