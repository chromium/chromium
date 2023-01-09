// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_WEB_AUTHENTICATION_PROXY_REMOTE_SESSION_STATE_CHANGE_H_
#define CHROME_BROWSER_EXTENSIONS_API_WEB_AUTHENTICATION_PROXY_REMOTE_SESSION_STATE_CHANGE_H_

#include <memory>

#include "base/files/file_path.h"
#include "base/files/file_path_watcher.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/event_router.h"

namespace extensions {

// WebAuthenticationProxyRemoteSessionStateChangeNotifier watches for changes to
// a per-extension file in the well-known directory path returned by
// `GetSessionStateChangeDir()`, and raises
// `webAuthentcationProxy.onRemoteSessionStateChange` events in response.
class WebAuthenticationProxyRemoteSessionStateChangeNotifier {
 public:
  // Returns the directory in which the extension is expected to write its file.
  static bool GetSessionStateChangeDir(base::FilePath* out);

  WebAuthenticationProxyRemoteSessionStateChangeNotifier(
      EventRouter* event_router,
      ExtensionId extension_id);
  WebAuthenticationProxyRemoteSessionStateChangeNotifier(
      const WebAuthenticationProxyRemoteSessionStateChangeNotifier&) = delete;
  WebAuthenticationProxyRemoteSessionStateChangeNotifier& operator=(
      WebAuthenticationProxyRemoteSessionStateChangeNotifier&) = delete;
  virtual ~WebAuthenticationProxyRemoteSessionStateChangeNotifier();

 private:
  void OnRemoteSessionStateFileUpdate(
      base::RepeatingCallback<void()> on_file_change,
      const base::FilePath& path,
      bool error);
  void BroadcastRemoteSessionStateChangeEvent();

  const raw_ptr<EventRouter> event_router_;
  const ExtensionId extension_id_;

  // FilePathWatcher::Watch() may block, and must be called on the same sequence
  // as the destructor.
  scoped_refptr<base::SequencedTaskRunner> io_runner_ =
      base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()});

  std::unique_ptr<base::FilePathWatcher, base::OnTaskRunnerDeleter> watcher_{
      new base::FilePathWatcher(), base::OnTaskRunnerDeleter(io_runner_)};

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<WebAuthenticationProxyRemoteSessionStateChangeNotifier>
      weak_ptr_factory_{this};
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_WEB_AUTHENTICATION_PROXY_REMOTE_SESSION_STATE_CHANGE_H_
