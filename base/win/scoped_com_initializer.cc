// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/win/scoped_com_initializer.h"

#include <wrl/implements.h>

#include <ostream>

#include "base/check_op.h"
#include "base/win/resource_exhaustion.h"

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

  // CoInitializeEx may call RegisterClassEx to get an ATOM. On failure, the
  // call to RegisterClassEx sets the last error code to
  // ERROR_NOT_ENOUGH_MEMORY. CoInitializeEx is retuning the converted error
  // code (a.k.a HRESULT_FROM_WIN32(...)). The following code handles the case
  // where HRESULT_FROM_WIN32(ERROR_NOT_ENOUGH_MEMORY) is being returned by
  // CoInitializeEx. We assume they are due to ATOM exhaustion. This appears to
  // happen most often when the browser is being driven by automation tools,
  // though the underlying reason for this remains a mystery
  // (https://crbug.com/1470483). There is nothing that Chrome can do to
  // meaningfully run until the user restarts their session by signing out of
  // Windows or restarting their computer.
  if (init == COINIT_APARTMENTTHREADED &&
      hr_ == HRESULT_FROM_WIN32(ERROR_NOT_ENOUGH_MEMORY)) {
    base::win::OnResourceExhausted();
  }
}

}  // namespace win
}  // namespace base
