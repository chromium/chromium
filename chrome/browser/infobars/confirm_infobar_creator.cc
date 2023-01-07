// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/infobars/confirm_infobar_creator.h"

#include "build/build_config.h"
#include "components/infobars/core/confirm_infobar_delegate.h"

#if BUILDFLAG(IS_ANDROID)
#include "components/infobars/android/confirm_infobar.h"
#else
#include "chrome/browser/ui/views/infobars/confirm_infobar.h"
#endif

std::unique_ptr<infobars::InfoBar> CreateConfirmInfoBar(
    std::unique_ptr<ConfirmInfoBarDelegate> delegate) {
#if BUILDFLAG(IS_ANDROID)
  return std::make_unique<infobars::ConfirmInfoBar>(std::move(delegate));
#else
  return std::make_unique<ConfirmInfoBar>(std::move(delegate));
#endif
}
