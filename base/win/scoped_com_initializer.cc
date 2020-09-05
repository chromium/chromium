// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/win/scoped_com_initializer.h"

#include <wrl/implements.h>

#include "base/check_op.h"

namespace base {
namespace win {

ScopedCOMInitializer::ScopedCOMInitializer(Uninitialization uninitialization) {
  Initialize(COINIT_APARTMENTTHREADED, uninitialization);
}

ScopedCOMInitializer::ScopedCOMInitializer(SelectMTA mta,
                                           Uninitialization uninitialization) {
  Initialize(COINIT_MULTITHREADED, uninitialization);
}

ScopedCOMInitializer::~ScopedCOMInitializer() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (Succeeded()) {
    if (com_balancer_) {
      com_balancer_->Disable();
      com_balancer_.Reset();
    }
    CoUninitialize();
  }
}

bool ScopedCOMInitializer::Succeeded() const {
  return SUCCEEDED(hr_);
}

DWORD ScopedCOMInitializer::GetCOMBalancerReferenceCountForTesting() const {
  if (com_balancer_)
    return com_balancer_->GetReferenceCountForTesting();
  return 0;
}

void ScopedCOMInitializer::Initialize(COINIT init,
                                      Uninitialization uninitialization) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  // COINIT_DISABLE_OLE1DDE is always added based on:
  // https://docs.microsoft.com/en-us/windows/desktop/learnwin32/initializing-the-com-library
  if (uninitialization == Uninitialization::kBlockPremature) {
    com_balancer_ = Microsoft::WRL::Details::Make<internal::ComInitBalancer>(
        init | COINIT_DISABLE_OLE1DDE);
  }
  hr_ = ::CoInitializeEx(nullptr, init | COINIT_DISABLE_OLE1DDE);
  DCHECK_NE(RPC_E_CHANGED_MODE, hr_) << "Invalid COM thread model change";
}

}  // namespace win
}  // namespace base
