// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/kcer/extra_instances.h"

#include "ash/components/kcer/kcer_impl.h"
#include "ash/components/kcer/kcer_token.h"
#include "base/no_destructor.h"
#include "content/public/browser/browser_thread.h"

namespace kcer {

// static
ExtraInstances* ExtraInstances::Get() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  static base::NoDestructor<ExtraInstances> factory;
  return factory.get();
}

// static
base::WeakPtr<Kcer> ExtraInstances::GetDefaultKcer() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  base::WeakPtr<Kcer> default_kcer = Get()->default_kcer_;
  return default_kcer ? default_kcer : GetEmptyKcer();
}

// static
base::WeakPtr<Kcer> ExtraInstances::GetEmptyKcer() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return Get()->empty_kcer_.GetWeakPtr();
}

// static
base::WeakPtr<Kcer> ExtraInstances::GetDeviceKcer() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
#if BUILDFLAG(IS_CHROMEOS_ASH)
  return Get()->device_kcer_->GetWeakPtr();
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
  // At the moment `device_kcer_` is never initialized for Lacros. This can be
  // changed if needed.
  return GetEmptyKcer();
#endif
}

void ExtraInstances::InitializeDeviceKcer(
    scoped_refptr<base::TaskRunner> token_task_runner,
    base::WeakPtr<internal::KcerToken> device_token) {
  device_kcer_->Initialize(std::move(token_task_runner), /*user_token=*/nullptr,
                           std::move(device_token));
}

void ExtraInstances::SetDefaultKcer(base::WeakPtr<Kcer> default_kcer) {
  default_kcer_ = std::move(default_kcer);
}

ExtraInstances::ExtraInstances() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  empty_kcer_.Initialize(/*token_task_runner=*/nullptr, /*user_token=*/nullptr,
                         /*device_token=*/nullptr);

#if BUILDFLAG(IS_CHROMEOS_ASH)
  device_kcer_ = std::make_unique<internal::KcerImpl>();
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

// Never called (see NoDestructor<>).
ExtraInstances::~ExtraInstances() = default;

}  // namespace kcer
