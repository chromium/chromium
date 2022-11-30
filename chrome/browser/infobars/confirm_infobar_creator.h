// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_INFOBARS_CONFIRM_INFOBAR_CREATOR_H_
#define CHROME_BROWSER_INFOBARS_CONFIRM_INFOBAR_CREATOR_H_

#include <memory>

class ConfirmInfoBarDelegate;

namespace infobars {
class InfoBar;
}

// Cross-platform method for creating a confirm infobar.
std::unique_ptr<infobars::InfoBar> CreateConfirmInfoBar(
    std::unique_ptr<ConfirmInfoBarDelegate> delegate);

#endif  // CHROME_BROWSER_INFOBARS_CONFIRM_INFOBAR_CREATOR_H_
