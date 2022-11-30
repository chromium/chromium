// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PLUGINS_HUNG_PLUGIN_INFOBAR_DELEGATE_H_
#define CHROME_BROWSER_PLUGINS_HUNG_PLUGIN_INFOBAR_DELEGATE_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "components/infobars/core/confirm_infobar_delegate.h"

class HungPluginTabHelper;

namespace infobars {
class ContentInfoBarManager;
}

class HungPluginInfoBarDelegate : public ConfirmInfoBarDelegate {
 public:
  // Creates a hung plugin infobar and delegate and adds the infobar to
  // |infobar_manager|.  Returns the infobar if it was successfully added.
  static infobars::InfoBar* Create(
      infobars::ContentInfoBarManager* infobar_manager,
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

  raw_ptr<HungPluginTabHelper> helper_;
  int plugin_child_id_;

  std::u16string message_;
  std::u16string button_text_;
};

#endif  // CHROME_BROWSER_PLUGINS_HUNG_PLUGIN_INFOBAR_DELEGATE_H_
