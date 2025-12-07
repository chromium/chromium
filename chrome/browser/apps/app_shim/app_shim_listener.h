// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SHIM_APP_SHIM_LISTENER_H_
#define CHROME_BROWSER_APPS_APP_SHIM_APP_SHIM_LISTENER_H_

#include <memory>

#include "base/memory/ref_counted.h"
#include "chrome/browser/apps/app_shim/mach_bootstrap_acceptor.h"
#include "content/public/browser/browser_thread.h"
#include "mojo/public/cpp/platform/platform_channel_endpoint.h"

namespace base {
class FilePath;
}

namespace test {
class AppShimListenerTestApi;
}

// The AppShimListener receives connections from app shims on a Mach
// bootstrap namespace entry (mach_acceptor_) and creates a helper object
// to manage the connection.
class AppShimListener : public apps::MachBootstrapAcceptor::Delegate,
                        public base::RefCountedThreadSafe<
                            AppShimListener,
                            content::BrowserThread::DeleteOnUIThread> {
 public:
  AppShimListener();
  AppShimListener(const AppShimListener&) = delete;
  AppShimListener& operator=(const AppShimListener&) = delete;

  // Init passes this AppShimListener to PostTask which requires it to have
  // a non-zero refcount. Therefore, Init cannot be called in the constructor
  // since the refcount is zero at that point.
  void Init();

 private:
  friend class base::RefCountedThreadSafe<AppShimListener>;
  friend struct content::BrowserThread::DeleteOnThread<
      content::BrowserThread::UI>;
  friend class base::DeleteHelper<AppShimListener>;
  friend class test::AppShimListenerTestApi;
  virtual ~AppShimListener();

  bool has_initialized_ = false;

  // MachBootstrapAcceptor::Delegate:
  void OnClientConnected(mojo::PlatformChannelEndpoint endpoint,
                         audit_token_t audit_token) override;
  void OnServerChannelCreateError() override;

  // The |acceptor_| must be created on a thread which allows blocking I/O.
  void InitOnBackgroundThread();

  base::FilePath directory_in_tmp_;

  std::unique_ptr<apps::MachBootstrapAcceptor> mach_acceptor_;
};

#endif  // CHROME_BROWSER_APPS_APP_SHIM_APP_SHIM_LISTENER_H_
