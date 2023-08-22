// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/win/scoped_winrt_initializer.h"

#include <roapi.h>

#include "base/check_op.h"
#include "base/win/com_init_util.h"

namespace base::win {

ScopedWinrtInitializer::ScopedWinrtInitializer()
    : hr_(::RoInitialize(RO_INIT_MULTITHREADED)) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
#if DCHECK_IS_ON()
  if (SUCCEEDED(hr_))
    AssertComApartmentType(ComApartmentType::MTA);
  else
    DCHECK_NE(RPC_E_CHANGED_MODE, hr_) << "Invalid COM thread model change";
#endif
}

ScopedWinrtInitializer::~ScopedWinrtInitializer() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (SUCCEEDED(hr_))
    ::RoUninitialize();
}

bool ScopedWinrtInitializer::Succeeded() const {
  return SUCCEEDED(hr_);
}

}  // namespace base::win
