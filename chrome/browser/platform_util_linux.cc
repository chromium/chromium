// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/platform_util.h"

#include <fcntl.h>

#include <memory>
#include <optional>
#include <queue>
#include <string>
#include <vector>

#include "base/callback_list.h"
#include "base/check.h"
#include "base/check_op.h"
#include "base/containers/contains.h"
#include "base/files/file_path.h"
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
#include "base/task/thread_pool.h"
#include "base/types/expected.h"
#include "chrome/browser/platform_util_internal.h"
#include "components/dbus/thread_linux/dbus_thread_linux.h"
#include "components/dbus/utils/call_method.h"
#include "components/dbus/utils/check_for_service_and_start.h"
#include "components/dbus/xdg/request.h"
#include "content/public/browser/browser_thread.h"
#include "dbus/bus.h"
#include "dbus/object_proxy.h"
#include "url/gurl.h"

using content::BrowserThread;

namespace platform_util {

namespace {

const char kFreedesktopFileManagerName[] = "org.freedesktop.FileManager1";
const char kFreedesktopFileManagerPath[] = "/org/freedesktop/FileManager1";
const char kMethodShowItems[] = "ShowItems";

const char kFreedesktopPortalName[] = "org.freedesktop.portal.Desktop";
const char kFreedesktopPortalPath[] = "/org/freedesktop/portal/desktop";
const char kFreedesktopPortalOpenURI[] = "org.freedesktop.portal.OpenURI";
const char kMethodOpenDirectory[] = "OpenDirectory";
const char kActivationTokenKey[] = "activation_token";

class ShowItemHelper {
 public:
  static ShowItemHelper& GetInstance() {
    static base::NoDestructor<ShowItemHelper> instance;
    return *instance;
  }

  ShowItemHelper() = default;

  ShowItemHelper(const ShowItemHelper&) = delete;
  ShowItemHelper& operator=(const ShowItemHelper&) = delete;

  void ShowItemInFolder(const base::FilePath& full_path) {
    // Skip opening the folder during browser tests, to avoid leaving an open
    // file explorer window behind.
    if (!internal::AreShellOperationsAllowed()) {
      return;
    }
    if (!bus_) {
      bus_ = dbus_thread_linux::GetSharedSessionBus();
    }

    if (api_type_.has_value()) {
      ShowItemInFolderOnApiTypeSet(full_path);
      return;
    }

    bool api_availability_check_in_progress = !pending_requests_.empty();
    pending_requests_.push(full_path);
    if (!api_availability_check_in_progress) {
      // Initiate check to determine if portal or the FileManager API should
      // be used. The portal API is always preferred if available.
      dbus_utils::CheckForServiceAndStart(
          bus_.get(), kFreedesktopPortalName,
          base::BindOnce(&ShowItemHelper::CheckPortalRunningResponse,
                         // Unretained is safe, the ShowItemHelper instance is
                         // never destroyed.
                         base::Unretained(this)));
    }
  }

 private:
  enum class ApiType { kNone, kPortal, kFileManager };

  void ShowItemInFolderOnApiTypeSet(const base::FilePath& full_path) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    CHECK(api_type_.has_value());
    switch (*api_type_) {
      case ApiType::kPortal:
        ShowItemUsingPortal(full_path);
        break;
      case ApiType::kFileManager:
        ShowItemUsingFileManager(full_path);
        break;
      case ApiType::kNone:
        OpenParentFolderFallback(full_path);
        break;
    }
  }

  void ProcessPendingRequests() {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    if (!bus_) {
      return;
    }

    CHECK(!pending_requests_.empty());
    while (!pending_requests_.empty()) {
      ShowItemInFolderOnApiTypeSet(pending_requests_.front());
      pending_requests_.pop();
    }
  }

  void CheckPortalRunningResponse(std::optional<bool> is_running) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    if (is_running.value_or(false)) {
      api_type_ = ApiType::kPortal;
      ProcessPendingRequests();
    } else {
      // Portal is unavailable.
      // Check if FileManager is available.
      dbus_utils::CheckForServiceAndStart(
          bus_.get(), kFreedesktopFileManagerName,
          base::BindOnce(&ShowItemHelper::CheckFileManagerRunningResponse,
                         // Unretained is safe, the ShowItemHelper instance is
                         // never destroyed.
                         base::Unretained(this)));
    }
  }

  void CheckFileManagerRunningResponse(std::optional<bool> is_running) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    if (is_running.value_or(false)) {
      api_type_ = ApiType::kFileManager;
    } else {
      // Neither portal nor FileManager is available.
      api_type_ = ApiType::kNone;
    }
    ProcessPendingRequests();
  }

  void ShowItemUsingPortal(const base::FilePath& full_path) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    CHECK(api_type_.has_value());
    CHECK_EQ(*api_type_, ApiType::kPortal);
    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE, {base::MayBlock()},
        base::BindOnce(
            [](const base::FilePath& full_path) {
              base::ScopedFD fd(HANDLE_EINTR(
                  open(full_path.value().c_str(), O_RDONLY | O_CLOEXEC)));
              return fd;
            },
            full_path),
        base::BindOnce(&ShowItemHelper::ShowItemUsingPortalFdOpened,
                       // Unretained is safe, the ShowItemHelper instance is
                       // never destroyed.
                       base::Unretained(this), full_path));
  }

  void ShowItemUsingPortalFdOpened(const base::FilePath& full_path,
                                   base::ScopedFD fd) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    if (!bus_) {
      return;
    }
    if (!fd.is_valid()) {
      // At least open the parent folder, as long as we're not in the unit
      // tests.
      OpenParentFolderFallback(full_path);
      return;
    }
    base::nix::CreateXdgActivationToken(base::BindOnce(
        &ShowItemHelper::ShowItemUsingPortalWithToken,
        // Unretained is safe, the ShowItemHelper instance is never destroyed.
        base::Unretained(this), full_path, std::move(fd)));
  }

  void ShowItemUsingPortalWithToken(const base::FilePath& full_path,
                                    base::ScopedFD fd,
                                    std::string activation_token) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    if (!bus_) {
      return;
    }

    if (!portal_object_proxy_) {
      portal_object_proxy_ = bus_->GetObjectProxy(
          kFreedesktopPortalName, dbus::ObjectPath(kFreedesktopPortalPath));
    }

    dbus_xdg::Dictionary options;
    options[kActivationTokenKey] =
        dbus_utils::Variant::Wrap<"s">(activation_token);
    // In the rare occasion that another request comes in before the response is
    // received, we will end up overwriting this request object with the new one
    // and the response from the first request will not be handled in that case.
    // This should be acceptable as it means the two requests were received too
    // close to each other from the user and the first one was handled on a best
    // effort basis.
    portal_open_directory_request_ = std::make_unique<dbus_xdg::Request>(
        bus_, portal_object_proxy_, kFreedesktopPortalOpenURI,
        kMethodOpenDirectory, std::move(options),
        base::BindOnce(&ShowItemHelper::ShowItemUsingPortalResponse,
                       // Unretained is safe, the ShowItemHelper instance is
                       // never destroyed.
                       base::Unretained(this), full_path),
        std::string(), std::move(fd));
  }

  void ShowItemUsingPortalResponse(
      const base::FilePath& full_path,
      base::expected<dbus_xdg::Dictionary, dbus_xdg::ResponseError> results) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    portal_open_directory_request_.reset();
    if (!results.has_value()) {
      OpenParentFolderFallback(full_path);
    }
  }

  void ShowItemUsingFileManager(const base::FilePath& full_path) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    if (!bus_) {
      return;
    }
    CHECK(api_type_.has_value());
    CHECK_EQ(*api_type_, ApiType::kFileManager);
    if (!file_manager_object_proxy_) {
      file_manager_object_proxy_ =
          bus_->GetObjectProxy(kFreedesktopFileManagerName,
                               dbus::ObjectPath(kFreedesktopFileManagerPath));
    }

    std::vector<std::string> file_to_highlight{"file://" + full_path.value()};
    dbus_utils::CallMethod<"ass", "">(
        file_manager_object_proxy_, kFreedesktopFileManagerName,
        kMethodShowItems,
        base::BindOnce(&ShowItemHelper::ShowItemUsingFileManagerResponse,
                       // Unretained is safe, the ShowItemHelper instance is
                       // never destroyed.
                       base::Unretained(this), full_path),
        std::move(file_to_highlight), /*startup-id=*/"");
  }

  void ShowItemUsingFileManagerResponse(
      const base::FilePath& full_path,
      dbus_utils::CallMethodResultSig<""> response) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    if (!response.has_value()) {
      // If the bus call fails, at least open the parent folder.
      OpenParentFolderFallback(full_path);
    }
  }

  void OpenParentFolderFallback(const base::FilePath& full_path) {
    OpenItem(
        // profile is not used in linux
        /*profile=*/nullptr, full_path.DirName(), OPEN_FOLDER,
        OpenOperationCallback());
  }

  scoped_refptr<dbus::Bus> bus_;

  std::optional<ApiType> api_type_;
  // The proxy objects are owned by `bus_`.
  raw_ptr<dbus::ObjectProxy> portal_object_proxy_ = nullptr;
  raw_ptr<dbus::ObjectProxy> file_manager_object_proxy_ = nullptr;
  std::unique_ptr<dbus_xdg::Request> portal_open_directory_request_;

  // Requests that are queued until the API availability is determined.
  std::queue<base::FilePath> pending_requests_;
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
      disable_gnome_bug_buddy == std::string("SET_BY_GOOGLE_CHROME")) {
    options.environment["GNOME_DISABLE_CRASH_DIALOG"] = std::string();
  }

  base::Process process = base::LaunchProcess(argv, options);
  if (process.IsValid()) {
    base::EnsureProcessGetsReaped(std::move(process));
  }
}

void RunCommand(const std::string& command,
                const base::FilePath& working_directory,
                const std::string& arg) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
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
  switch (type) {
    case OPEN_FILE:
      // Launch options with xdg activation token can only be obtained on the UI
      // thread.
      content::GetUIThreadTaskRunner()->PostTask(
          FROM_HERE, base::BindOnce(&XDGOpen, path.DirName(), path.value()));
      break;
    case OPEN_FOLDER:
      // The utility process checks the working directory prior to the
      // invocation of xdg-open by changing the current directory into it. This
      // operation only succeeds if |path| is a directory. Opening "." from
      // there ensures that the target of the operation is a directory.  Note
      // that there remains a TOCTOU race where the directory could be unlinked
      // between the time the utility process changes into the directory and the
      // time the application invoked by xdg-open inspects the path by name.
      // Launch options with xdg activation token can only be obtained on the UI
      // thread.
      content::GetUIThreadTaskRunner()->PostTask(
          FROM_HERE, base::BindOnce(&XDGOpen, path, "."));
      break;
  }
}

}  // namespace internal

void ShowItemInFolder(Profile*, const base::FilePath& full_path) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  ShowItemHelper::GetInstance().ShowItemInFolder(full_path);
}

void OpenExternal(const GURL& url) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (url.SchemeIs("mailto")) {
    XDGEmail(url.spec());
  } else {
    XDGOpen(base::FilePath(), url.spec());
  }
}

}  // namespace platform_util
