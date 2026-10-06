// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_INFOBARS_CONFIRM_INFOBAR_CREATOR_H_
#define CHROME_BROWSER_INFOBARS_CONFIRM_INFOBAR_CREATOR_H_

#include <memory>

#include "build/build_config.h"

class ConfirmInfoBarDelegate;

namespace infobars {
class InfoBar;
}

namespace views {
class View;
}

// Cross-platform method for creating a confirm infobar.
std::unique_ptr<infobars::InfoBar> CreateConfirmInfoBar(
    std::unique_ptr<ConfirmInfoBarDelegate> delegate);

#if !BUILDFLAG(IS_ANDROID)
std::unique_ptr<infobars::InfoBar> CreateConfirmInfoBar(
    std::unique_ptr<ConfirmInfoBarDelegate> delegate,
    std::unique_ptr<views::View> custom_message_view);
#endif

#endif  // CHROME_BROWSER_INFOBARS_CONFIRM_INFOBAR_CREATOR_H_
