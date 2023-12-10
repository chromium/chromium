// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/extensions/install_limiter.h"

#include <string>

#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/ash/extensions/install_limiter_factory.h"
#include "extensions/browser/extensions_browser_client.h"

namespace {

int64_t GetFileSize(const base::FilePath& file) {
  // Get file size. In case of error, sets 0 as file size to let the installer
  // run and fail.
  int64_t size;
  return base::GetFileSize(file, &size) ? size : 0;
}

}  // namespace

namespace extensions {

////////////////////////////////////////////////////////////////////////////////
// InstallLimiter::DeferredInstall

InstallLimiter::DeferredInstall::DeferredInstall(
    const scoped_refptr<CrxInstaller>& installer,
    const CRXFileInfo& file_info)
    : installer(installer), file_info(file_info) {}

InstallLimiter::DeferredInstall::DeferredInstall(const DeferredInstall& other) =
    default;

InstallLimiter::DeferredInstall::~DeferredInstall() = default;

////////////////////////////////////////////////////////////////////////////////
// InstallLimiter

// static
InstallLimiter* InstallLimiter::Get(Profile* profile) {
  return InstallLimiterFactory::GetForProfile(profile);
}

// static
bool InstallLimiter::ShouldDeferInstall(int64_t app_size,
                                        const std::string& app_id) {
  constexpr int64_t kBigAppSizeThreshold = 1048576;  // 1MB in bytes
  return app_size > kBigAppSizeThreshold &&
         !ExtensionsBrowserClient::Get()->IsScreensaverInDemoMode(app_id);
}

InstallLimiter::InstallLimiter() : disabled_for_test_(false) {}

InstallLimiter::~InstallLimiter() = default;

void InstallLimiter::DisableForTest() {
  disabled_for_test_ = true;
}

void InstallLimiter::Add(const scoped_refptr<CrxInstaller>& installer,
                         const CRXFileInfo& file_info) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // No deferred installs when disabled for test.
  if (disabled_for_test_) {
    installer->InstallCrxFile(file_info);
    return;
  }

  num_installs_waiting_for_file_size_++;

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&GetFileSize, file_info.path),
      base::BindOnce(&InstallLimiter::AddWithSize,
                     weak_ptr_factory_.GetWeakPtr(), installer, file_info));
}

void InstallLimiter::OnAllExternalProvidersReady() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  all_external_providers_ready_ = true;

  if (AllInstallsQueuedWithFileSize()) {
    // Stop wait timer and let install notification drive deferred installs.
    wait_timer_.Stop();
    CheckAndRunDeferrredInstalls();
  }
}

void InstallLimiter::AddWithSize(const scoped_refptr<CrxInstaller>& installer,
                                 const CRXFileInfo& file_info,
                                 int64_t size) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  num_installs_waiting_for_file_size_--;

  if (!ShouldDeferInstall(size, installer->expected_id()) ||
      AllInstallsQueuedWithFileSize()) {
    RunInstall(installer, file_info);

    // Stop wait timer and let install notification drive deferred installs.
    wait_timer_.Stop();
    return;
  }

  deferred_installs_.push(DeferredInstall(installer, file_info));

  // When there are no running installs, wait a bit before running deferred
  // installs to allow small app install to take precedence, especially when a
  // big app is the first one in the list.
  if (num_running_installs_ == 0 && !wait_timer_.IsRunning()) {
    const int kMaxWaitTimeInMs = 5000;  // 5 seconds.
    wait_timer_.Start(FROM_HERE, base::Milliseconds(kMaxWaitTimeInMs), this,
                      &InstallLimiter::CheckAndRunDeferrredInstalls);
  }
}

void InstallLimiter::CheckAndRunDeferrredInstalls() {
  if (deferred_installs_.empty() || num_running_installs_ > 0)
    return;

  const DeferredInstall& deferred = deferred_installs_.front();
  RunInstall(deferred.installer, deferred.file_info);
  deferred_installs_.pop();
}

void InstallLimiter::RunInstall(const scoped_refptr<CrxInstaller>& installer,
                                const CRXFileInfo& file_info) {
  installer->AddInstallerCallback(base::BindOnce(
      &InstallLimiter::OnInstallerDone, weak_ptr_factory_.GetWeakPtr()));
  installer->InstallCrxFile(file_info);
  num_running_installs_++;
}

void InstallLimiter::OnInstallerDone(
    const std::optional<CrxInstallError>& error) {
  CHECK(num_running_installs_ > 0);
  num_running_installs_--;
  CheckAndRunDeferrredInstalls();
}

bool InstallLimiter::AllInstallsQueuedWithFileSize() const {
  return all_external_providers_ready_ &&
         num_installs_waiting_for_file_size_ == 0;
}

}  // namespace extensions
