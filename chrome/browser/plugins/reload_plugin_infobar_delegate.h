// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PLUGINS_RELOAD_PLUGIN_INFOBAR_DELEGATE_H_
#define CHROME_BROWSER_PLUGINS_RELOAD_PLUGIN_INFOBAR_DELEGATE_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "components/infobars/core/confirm_infobar_delegate.h"

namespace content {
class NavigationController;
}

namespace infobars {
class ContentInfoBarManager;
}

class ReloadPluginInfoBarDelegate : public ConfirmInfoBarDelegate {
 public:
  static void Create(infobars::ContentInfoBarManager* infobar_manager,
                     content::NavigationController* controller,
                     const std::u16string& message);

 private:
  ReloadPluginInfoBarDelegate(content::NavigationController* controller,
                              const std::u16string& message);
  ~ReloadPluginInfoBarDelegate() override;

  // ConfirmInfobarDelegate:
  infobars::InfoBarDelegate::InfoBarIdentifier GetIdentifier() const override;
  const gfx::VectorIcon& GetVectorIcon() const override;
  std::u16string GetMessageText() const override;
  int GetButtons() const override;
  std::u16string GetButtonLabel(InfoBarButton button) const override;
  bool Accept() override;

  raw_ptr<content::NavigationController> controller_;
  std::u16string message_;
};

#endif  // CHROME_BROWSER_PLUGINS_RELOAD_PLUGIN_INFOBAR_DELEGATE_H_
