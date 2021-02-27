// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PLUGINS_RELOAD_PLUGIN_INFOBAR_DELEGATE_H_
#define CHROME_BROWSER_PLUGINS_RELOAD_PLUGIN_INFOBAR_DELEGATE_H_

#include "base/strings/string16.h"
#include "components/infobars/core/confirm_infobar_delegate.h"

class InfoBarService;

namespace content {
class NavigationController;
}

class ReloadPluginInfoBarDelegate : public ConfirmInfoBarDelegate {
 public:
  static void Create(InfoBarService* infobar_service,
                     content::NavigationController* controller,
                     const base::string16& message);

 private:
  ReloadPluginInfoBarDelegate(content::NavigationController* controller,
                              const base::string16& message);
  ~ReloadPluginInfoBarDelegate() override;

  // ConfirmInfobarDelegate:
  infobars::InfoBarDelegate::InfoBarIdentifier GetIdentifier() const override;
  const gfx::VectorIcon& GetVectorIcon() const override;
  base::string16 GetMessageText() const override;
  int GetButtons() const override;
  base::string16 GetButtonLabel(InfoBarButton button) const override;
  bool Accept() override;

  content::NavigationController* controller_;
  base::string16 message_;
};

#endif  // CHROME_BROWSER_PLUGINS_RELOAD_PLUGIN_INFOBAR_DELEGATE_H_
