// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/win/scoped_winrt_initializer.h"

#include "base/logging.h"
#include "base/win/com_init_util.h"
#include "base/win/core_winrt_util.h"
#include "base/win/windows_version.h"

namespace base {
namespace win {

ScopedWinrtInitializer::ScopedWinrtInitializer()
    : hr_(base::win::RoInitialize(RO_INIT_MULTITHREADED)) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK_GE(GetVersion(), Version::WIN8);
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
    base::win::RoUninitialize();
}

bool ScopedWinrtInitializer::Succeeded() const {
  return SUCCEEDED(hr_);
}

}  // namespace win
}  // namespace base
