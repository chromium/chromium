// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PLUGINS_HUNG_PLUGIN_INFOBAR_DELEGATE_H_
#define CHROME_BROWSER_PLUGINS_HUNG_PLUGIN_INFOBAR_DELEGATE_H_

#include <string>

#include "components/infobars/core/confirm_infobar_delegate.h"

class HungPluginTabHelper;
class InfoBarService;

class HungPluginInfoBarDelegate : public ConfirmInfoBarDelegate {
 public:
  // Creates a hung plugin infobar and delegate and adds the infobar to
  // |infobar_service|.  Returns the infobar if it was successfully added.
  static infobars::InfoBar* Create(InfoBarService* infobar_service,
                                   HungPluginTabHelper* helper,
                                   int plugin_child_id,
                                   const std::u16string& plugin_name);

 private:
  HungPluginInfoBarDelegate(HungPluginTabHelper* helper,
                            int plugin_child_id,
                            const std::u16string& plugin_name);
  ~HungPluginInfoBarDelegate() override;

  // ConfirmInfoBarDelegate:
  infobars::InfoBarDelegate::InfoBarIdentifier GetIdentifier() const override;
  const gfx::VectorIcon& GetVectorIcon() const override;
  std::u16string GetMessageText() const override;
  int GetButtons() const override;
  std::u16string GetButtonLabel(InfoBarButton button) const override;
  bool Accept() override;

  HungPluginTabHelper* helper_;
  int plugin_child_id_;

  std::u16string message_;
  std::u16string button_text_;
};

#endif  // CHROME_BROWSER_PLUGINS_HUNG_PLUGIN_INFOBAR_DELEGATE_H_
