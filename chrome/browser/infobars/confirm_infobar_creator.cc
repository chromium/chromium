// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/infobars/confirm_infobar_creator.h"

#include <utility>

#include "build/build_config.h"
#include "components/infobars/core/confirm_infobar_delegate.h"
#include "components/infobars/core/infobar.h"

#if BUILDFLAG(IS_ANDROID)
// No platform-specific UI on Android.
#else
#include "chrome/browser/ui/views/infobars/confirm_infobar.h"
#include "ui/views/view.h"
#endif

std::unique_ptr<infobars::InfoBar> CreateConfirmInfoBar(
    std::unique_ptr<ConfirmInfoBarDelegate> delegate) {
#if BUILDFLAG(IS_ANDROID)
  // TODO(https://crbug.com/518840663): remove android support once
  // extension dev tools infobar is migrated to other Android UI.
  return std::make_unique<infobars::InfoBar>(std::move(delegate));
#else
  return ConfirmInfoBar::Create(std::move(delegate));
#endif
}

#if !BUILDFLAG(IS_ANDROID)
std::unique_ptr<infobars::InfoBar> CreateConfirmInfoBar(
    std::unique_ptr<ConfirmInfoBarDelegate> delegate,
    std::unique_ptr<views::View> custom_message_view) {
  return ConfirmInfoBar::Create(std::move(delegate),
                                std::move(custom_message_view));
}
#endif
