// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/platform_util.h"

#include <fcntl.h>

#include <string>
#include <vector>

#include "base/callback_list.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/nix/xdg_util.h"
#include "base/no_destructor.h"
#include "base/posix/eintr_wrapper.h"
#include "base/process/kill.h"
#include "base/process/launch.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/scoped_blocking_call.h"
#include "chrome/browser/lifetime/termination_notification.h"
#include "chrome/browser/platform_util_internal.h"
#include "chrome/browser/profiles/profile.h"
// This file gets pulled in in Chromecast builds, which causes "gn check" to
// complain as Chromecast doesn't use (or depend on) //components/dbus.
// TODO(crbug.com/1215474): Eliminate //chrome being visible in the GN structure
// on Chromecast and remove the nogncheck below.
#include <optional>

#include "components/dbus/thread_linux/dbus_thread_linux.h"  // nogncheck
#include "content/public/browser/browser_thread.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_proxy.h"
#include "url/gurl.h"

using content::BrowserThread;

namespace platform_util {

namespace {

const char kMethodListActivatableNames[] = "ListActivatableNames";
const char kMethodNameHasOwner[] = "NameHasOwner";

const char kFreedesktopFileManagerName[] = "org.freedesktop.FileManager1";
const char kFreedesktopFileManagerPath[] = "/org/freedesktop/FileManager1";

const char kMethodShowItems[] = "ShowItems";

const char kFreedesktopPortalName[] = "org.freedesktop.portal.Desktop";
const char kFreedesktopPortalPath[] = "/org/freedesktop/portal/desktop";
const char kFreedesktopPortalOpenURI[] = "org.freedesktop.portal.OpenURI";

const char kMethodOpenDirectory[] = "OpenDirectory";

class ShowItemHelper {
 public:
  static ShowItemHelper& GetInstance() {
    static base::NoDestructor<ShowItemHelper> instance;
    return *instance;
  }

  ShowItemHelper()
      : browser_shutdown_subscription_(
            browser_shutdown::AddAppTerminatingCallback(
                base::BindOnce(&ShowItemHelper::OnAppTerminating,
                               base::Unretained(this)))) {}

  ShowItemHelper(const ShowItemHelper&) = delete;
  ShowItemHelper& operator=(const ShowItemHelper&) = delete;

  void ShowItemInFolder(Profile* profile, const base::FilePath& full_path) {
    if (!bus_) {
      // Sets up the D-Bus connection.
      dbus::Bus::Options bus_options;
      bus_options.bus_type = dbus::Bus::SESSION;
      bus_options.connection_type = dbus::Bus::PRIVATE;
      bus_options.dbus_task_runner = dbus_thread_linux::GetTaskRunner();
      bus_ = base::MakeRefCounted<dbus::Bus>(bus_options);
    }

    if (!dbus_proxy_) {
      dbus_proxy_ = bus_->GetObjectProxy(DBUS_SERVICE_DBUS,
                                         dbus::ObjectPath(DBUS_PATH_DBUS));
    }

    if (prefer_filemanager_interface_.has_value()) {
      if (prefer_filemanager_interface_.value()) {
        VLOG(1) << "Using FileManager1 to show folder";
        ShowItemUsingFileManager(profile, full_path);
      } else {
        VLOG(1) << "Using OpenURI to show folder";
        ShowItemUsingFreedesktopPortal(profile, full_path);
      }
    } else {
      CheckFileManagerRunning(profile, full_path);
    }
  }

 private:
  void OnAppTerminating() {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    // The browser process is about to exit. Clean up while we still can.
    object_proxy_ = nullptr;
    dbus_proxy_ = nullptr;
    if (bus_)
      bus_->ShutdownOnDBusThreadAndBlock();
    bus_.reset();
  }

  void CheckFileManagerRunning(Profile* profile,
                               const base::FilePath& full_path) {
    dbus::MethodCall method_call(DBUS_INTERFACE_DBUS, kMethodNameHasOwner);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(kFreedesktopFileManagerName);

    dbus_proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&ShowItemHelper::CheckFileManagerRunningResponse,
                       weak_ptr_factory_.GetWeakPtr(), profile, full_path));
  }

  void CheckFileManagerRunningResponse(Profile* profile,
                                       const base::FilePath& full_path,
                                       dbus::Response* response) {
    if (prefer_filemanager_interface_.has_value()) {
      ShowItemInFolder(profile, full_path);
      return;
    }

    bool is_running = false;

    if (!response) {
      LOG(ERROR) << "Failed to call " << kMethodNameHasOwner;
    } else {
      dbus::MessageReader reader(response);
      bool owned = false;

      if (!reader.PopBool(&owned)) {
        LOG(ERROR) << "Failed to read " << kMethodNameHasOwner << " resposne";
      } else if (owned) {
        is_running = true;
      }
    }

    if (is_running) {
      prefer_filemanager_interface_ = true;
      ShowItemInFolder(profile, full_path);
    } else {
      CheckFileManagerActivatable(profile, full_path);
    }
  }

  void CheckFileManagerActivatable(Profile* profile,
                                   const base::FilePath& full_path) {
    dbus::MethodCall method_call(DBUS_INTERFACE_DBUS,
                                 kMethodListActivatableNames);
    dbus_proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&ShowItemHelper::CheckFileManagerActivatableResponse,
                       weak_ptr_factory_.GetWeakPtr(), profile, full_path));
  }

  void CheckFileManagerActivatableResponse(Profile* profile,
                                           const base::FilePath& full_path,
                                           dbus::Response* response) {
    if (prefer_filemanager_interface_.has_value()) {
      ShowItemInFolder(profile, full_path);
      return;
    }

    bool is_activatable = false;

    if (!response) {
      LOG(ERROR) << "Failed to call " << kMethodListActivatableNames;
    } else {
      dbus::MessageReader reader(response);
      std::vector<std::string> names;
      if (!reader.PopArrayOfStrings(&names)) {
        LOG(ERROR) << "Failed to read " << kMethodListActivatableNames
                   << " response";
      } else if (base::Contains(names, kFreedesktopFileManagerName)) {
        is_activatable = true;
      }
    }

    prefer_filemanager_interface_ = is_activatable;
    ShowItemInFolder(profile, full_path);
  }

  void ShowItemUsingFreedesktopPortal(Profile* profile,
                                      const base::FilePath& full_path) {
    if (!object_proxy_) {
      object_proxy_ = bus_->GetObjectProxy(
          kFreedesktopPortalName, dbus::ObjectPath(kFreedesktopPortalPath));
    }

    base::ScopedFD fd(
        HANDLE_EINTR(open(full_path.value().c_str(), O_RDONLY | O_CLOEXEC)));
    if (!fd.is_valid()) {
      PLOG(ERROR) << "Failed to open " << full_path << " for URI portal";

      // At least open the parent folder, as long as we're not in the unit
      // tests.
      if (internal::AreShellOperationsAllowed()) {
        OpenItem(profile, full_path.DirName(), OPEN_FOLDER,
                 OpenOperationCallback());
      }

      return;
    }

    dbus::MethodCall open_directory_call(kFreedesktopPortalOpenURI,
                                         kMethodOpenDirectory);
    dbus::MessageWriter writer(&open_directory_call);

    writer.AppendString("");

    // Note that AppendFileDescriptor() duplicates the fd, so we shouldn't
    // release ownership of it here.
    writer.AppendFileDescriptor(fd.get());

    dbus::MessageWriter options_writer(nullptr);
    writer.OpenArray("{sv}", &options_writer);
    writer.CloseContainer(&options_writer);

    ShowItemUsingBusCall(&open_directory_call, profile, full_path);
  }

  void ShowItemUsingFileManager(Profile* profile,
                                const base::FilePath& full_path) {
    if (!object_proxy_) {
      object_proxy_ =
          bus_->GetObjectProxy(kFreedesktopFileManagerName,
                               dbus::ObjectPath(kFreedesktopFileManagerPath));
    }

    dbus::MethodCall show_items_call(kFreedesktopFileManagerName,
                                     kMethodShowItems);
    dbus::MessageWriter writer(&show_items_call);

    writer.AppendArrayOfStrings(
        {"file://" + full_path.value()});  // List of file(s) to highlight.
    writer.AppendString({});               // startup-id

    ShowItemUsingBusCall(&show_items_call, profile, full_path);
  }

  void ShowItemUsingBusCall(dbus::MethodCall* call,
                            Profile* profile,
                            const base::FilePath& full_path) {
    // Skip opening the folder during browser tests, to avoid leaving an open
    // file explorer window behind.
    if (!internal::AreShellOperationsAllowed())
      return;

    object_proxy_->CallMethod(
        call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&ShowItemHelper::ShowItemInFolderResponse,
                       weak_ptr_factory_.GetWeakPtr(), profile, full_path,
                       call->GetMember()));
  }

  void ShowItemInFolderResponse(Profile* profile,
                                const base::FilePath& full_path,
                                const std::string& method,
                                dbus::Response* response) {
    if (response)
      return;

    LOG(ERROR) << "Error calling " << method;
    // If the bus call fails, at least open the parent folder.
    OpenItem(profile, full_path.DirName(), OPEN_FOLDER,
             OpenOperationCallback());
  }

  scoped_refptr<dbus::Bus> bus_;

  // These proxy objects are owned by `bus_`.
  raw_ptr<dbus::ObjectProxy> dbus_proxy_ = nullptr;
  raw_ptr<dbus::ObjectProxy> object_proxy_ = nullptr;

  std::optional<bool> prefer_filemanager_interface_;

  base::CallbackListSubscription browser_shutdown_subscription_;
  base::WeakPtrFactory<ShowItemHelper> weak_ptr_factory_{this};
};

void OnLaunchOptionsCreated(const std::string& command,
                            const base::FilePath& working_directory,
                            const std::string& arg,
                            base::LaunchOptions options) {
  std::vector<std::string> argv;
  argv.push_back(command);
  argv.push_back(arg);
  options.current_directory = working_directory;
  options.allow_new_privs = true;
  // xdg-open can fall back on mailcap which eventually might plumb through
  // to a command that needs a terminal.  Set the environment variable telling
  // it that we definitely don't have a terminal available and that it should
  // bring up a new terminal if necessary.  See "man mailcap".
  options.environment["MM_NOTTTY"] = "1";

  // In Google Chrome, we do not let GNOME's bug-buddy intercept our crashes.
  // However, we do not want this environment variable to propagate to external
  // applications. See http://crbug.com/24120
  char* disable_gnome_bug_buddy = getenv("GNOME_DISABLE_CRASH_DIALOG");
  if (disable_gnome_bug_buddy &&
      disable_gnome_bug_buddy == std::string("SET_BY_GOOGLE_CHROME"))
    options.environment["GNOME_DISABLE_CRASH_DIALOG"] = std::string();

  base::Process process = base::LaunchProcess(argv, options);
  if (process.IsValid())
    base::EnsureProcessGetsReaped(std::move(process));
}

void RunCommand(const std::string& command,
                const base::FilePath& working_directory,
                const std::string& arg) {
  base::nix::CreateLaunchOptionsWithXdgActivation(
      base::BindOnce(&OnLaunchOptionsCreated, command, working_directory, arg));
}

void XDGOpen(const base::FilePath& working_directory, const std::string& path) {
  RunCommand("xdg-open", working_directory, path);
}

void XDGEmail(const std::string& email) {
  RunCommand("xdg-email", base::FilePath(), email);
}

}  // namespace

namespace internal {

void PlatformOpenVerifiedItem(const base::FilePath& path, OpenItemType type) {
  // May result in an interactive dialog.
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  switch (type) {
    case OPEN_FILE:
      XDGOpen(path.DirName(), path.value());
      break;
    case OPEN_FOLDER:
      // The utility process checks the working directory prior to the
      // invocation of xdg-open by changing the current directory into it. This
      // operation only succeeds if |path| is a directory. Opening "." from
      // there ensures that the target of the operation is a directory.  Note
      // that there remains a TOCTOU race where the directory could be unlinked
      // between the time the utility process changes into the directory and the
      // time the application invoked by xdg-open inspects the path by name.
      XDGOpen(path, ".");
      break;
  }
}

}  // namespace internal

void ShowItemInFolder(Profile* profile, const base::FilePath& full_path) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  ShowItemHelper::GetInstance().ShowItemInFolder(profile, full_path);
}

void OpenExternal(const GURL& url) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (url.SchemeIs("mailto"))
    XDGEmail(url.spec());
  else
    XDGOpen(base::FilePath(), url.spec());
}

}  // namespace platform_util
