// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SERVICE_WEBAPK_WEBAPK_INSTALL_QUEUE_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_WEBAPK_WEBAPK_INSTALL_QUEUE_H_

#include <memory>
#include <string>

#include "ash/components/arc/mojom/webapk.mojom-forward.h"
#include "ash/components/arc/session/connection_observer.h"
#include "base/containers/circular_deque.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"

class Profile;

namespace apps {

class WebApkInstallTask;

// Queue for WebAPK installation and update. Queued apps are processed
// one-by-one while ARC is running.
class WebApkInstallQueue
    : public arc::ConnectionObserver<arc::mojom::WebApkInstance> {
 public:
  explicit WebApkInstallQueue(Profile* profile);
  WebApkInstallQueue(const WebApkInstallQueue&) = delete;
  WebApkInstallQueue& operator=(const WebApkInstallQueue&) = delete;

  ~WebApkInstallQueue() override;

  // Queues the given |app_id| to either install a new WebAPK or update its
  // existing WebAPK, as appropriate.
  void InstallOrUpdate(const std::string& app_id);

  // arc::ConnectionObserver<arc::mojom::WebApkInstance> overrides:
  void OnConnectionReady() override;
  void OnConnectionClosed() override;

  std::unique_ptr<WebApkInstallTask> PopTaskForTest();

 private:
  void PostMaybeStartNext();
  void MaybeStartNext();
  void OnInstallCompleted(bool success);

  raw_ptr<Profile> profile_;
  base::circular_deque<std::unique_ptr<WebApkInstallTask>> pending_installs_;
  std::unique_ptr<WebApkInstallTask> current_install_;
  bool connection_ready_ = false;

  base::WeakPtrFactory<WebApkInstallQueue> weak_ptr_factory_{this};
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_SERVICE_WEBAPK_WEBAPK_INSTALL_QUEUE_H_
