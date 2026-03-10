// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/web_authentication_proxy/remote_session_state_change.h"

#include <memory>

#include "base/base_paths.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/common/chrome_paths_internal.h"
#include "chrome/common/extensions/api/web_authentication_proxy.h"

namespace extensions {

namespace {

void OnSessionStateChangeFileUpdate(
    const base::FilePath& watched_path,
    const base::RepeatingCallback<void()>& update_callback,
    const base::FilePath& path,
    bool is_error) {
  DCHECK_EQ(path, watched_path);
  if (is_error) {
    DLOG(ERROR) << "OnRemoteSessionStateFileUpdate() error";
    return;
  }
  update_callback.Run();
}

void WatchSessionStateChangeFile(
    base::FilePathWatcher* watcher,
    const ExtensionId& extension_id,
    base::RepeatingCallback<void()> update_callback) {
  base::FilePath dir;
  if (!WebAuthenticationProxyRemoteSessionStateChangeNotifier::
          GetSessionStateChangeDir(&dir)) {
    DLOG(ERROR) << "GetSessionStateChangeDir failed";
    return;
  }

  if (!base::PathExists(dir)) {
    base::CreateDirectory(dir);
  }
  const base::FilePath path = dir.AppendASCII(extension_id);
  if (!watcher->Watch(path, base::FilePathWatcher::Type::kNonRecursive,
                      base::BindRepeating(&OnSessionStateChangeFileUpdate, path,
                                          update_callback))) {
    DLOG(ERROR) << "FilePathWatcher::Watch() failed";
  }
  DVLOG(1) << "WebAuthenticationProxyRemoteSessionStateChangeNotifier at "
           << path;
}
}  // namespace

bool WebAuthenticationProxyRemoteSessionStateChangeNotifier::
    GetSessionStateChangeDir(base::FilePath* out) {
  // The path must be stable, i.e. the remote desktop app should not need to do
  // any sort of discovery, which rules out the User Data Directory. It also has
  // to be user-writable, because the app isn't expected to run as root.
  base::FilePath default_udd;
  if (!chrome::GetDefaultUserDataDirectory(&default_udd)) {
    return false;
  }
  *out = default_udd.Append(
      FILE_PATH_LITERAL("WebAuthenticationProxyRemoteSessionStateChange"));
  return true;
}

WebAuthenticationProxyRemoteSessionStateChangeNotifier::
    WebAuthenticationProxyRemoteSessionStateChangeNotifier(
        EventRouter* event_router,
        ExtensionId extension_id)
    : event_router_(event_router), extension_id_(std::move(extension_id)) {
  DCHECK(event_router_);
  auto broadcast_event_on_change =
      base::BindPostTaskToCurrentDefault(base::BindRepeating(
          &WebAuthenticationProxyRemoteSessionStateChangeNotifier::
              BroadcastRemoteSessionStateChangeEvent,
          weak_ptr_factory_.GetWeakPtr()));
  // This task could run after `this` has been deleted. But `watcher_` is
  // getting destroyed on `io_runner_`, so it will still be alive.
  io_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&WatchSessionStateChangeFile, watcher_.get(),
                     extension_id_, std::move(broadcast_event_on_change)));
}

WebAuthenticationProxyRemoteSessionStateChangeNotifier::
    ~WebAuthenticationProxyRemoteSessionStateChangeNotifier() = default;

void WebAuthenticationProxyRemoteSessionStateChangeNotifier::
    BroadcastRemoteSessionStateChangeEvent() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  event_router_->DispatchEventToExtension(
      extension_id_,
      std::make_unique<Event>(
          events::WEB_AUTHENTICATION_PROXY_ON_REMOTE_SESSION_STATE_CHANGE,
          api::web_authentication_proxy::OnRemoteSessionStateChange::kEventName,
          api::web_authentication_proxy::OnRemoteSessionStateChange::Create()));
}

}  // namespace extensions
