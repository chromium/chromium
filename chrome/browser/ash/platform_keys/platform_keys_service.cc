// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/platform_keys/platform_keys_service.h"

#include <map>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/task/thread_pool.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/net_errors.h"
#include "net/cert/x509_certificate.h"

namespace ash {
namespace platform_keys {

// =============== PlatformKeysServiceImplDelegate =============================

PlatformKeysServiceImplDelegate::PlatformKeysServiceImplDelegate() = default;

PlatformKeysServiceImplDelegate::~PlatformKeysServiceImplDelegate() {
  ShutDown();
}

void PlatformKeysServiceImplDelegate::SetOnShutdownCallback(
    base::OnceClosure on_shutdown_callback) {
  DCHECK(!shut_down_);
  DCHECK(!on_shutdown_callback_);
  on_shutdown_callback_ = std::move(on_shutdown_callback);
}

bool PlatformKeysServiceImplDelegate::IsShutDown() const {
  return shut_down_;
}

void PlatformKeysServiceImplDelegate::ShutDown() {
  if (shut_down_)
    return;

  shut_down_ = true;
  if (on_shutdown_callback_)
    std::move(on_shutdown_callback_).Run();
}

// =================== PlatformKeysServiceImpl =================================

PlatformKeysServiceImpl::PlatformKeysServiceImpl(
    std::unique_ptr<PlatformKeysServiceImplDelegate> delegate)
    : delegate_(std::move(delegate)) {
  // base::Unretained is OK because |delegate_| is owned by this and can
  // only call the callback before it is destroyed.
  delegate_->SetOnShutdownCallback(base::BindOnce(
      &PlatformKeysServiceImpl::OnDelegateShutDown, base::Unretained(this)));
}

PlatformKeysServiceImpl::~PlatformKeysServiceImpl() {
  // Destroy the delegate as it calls back into OnDelegateShutdown() which we
  // should not call on a partially-destroyed `this`.
  delegate_.reset();
}

void PlatformKeysServiceImpl::AddObserver(
    PlatformKeysServiceObserver* observer) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  observers_.AddObserver(observer);
}

void PlatformKeysServiceImpl::RemoveObserver(
    PlatformKeysServiceObserver* observer) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  observers_.RemoveObserver(observer);
}

void PlatformKeysServiceImpl::OnDelegateShutDown() {
  for (auto& observer : observers_) {
    observer.OnPlatformKeysServiceShutDown();
  }
}

// The rest of the methods - the NSS-specific part of the implementation -
// resides in the platform_keys_service_nss.cc file.

}  // namespace platform_keys
}  // namespace ash
