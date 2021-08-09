// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHARING_SHARE_SUBMENU_MODEL_H_
#define CHROME_BROWSER_SHARING_SHARE_SUBMENU_MODEL_H_

#include "ui/base/models/simple_menu_model.h"
#include "url/gurl.h"

class Browser;

namespace send_tab_to_self {
class SendTabToSelfSubMenuModel;
}

namespace sharing {

// ShareSubmenuModel is a MenuModel intended to be slotted into another menu,
// usually a context menu, to offer a set of sharing options. Currently, it
// contains these items:
//
//   (Optionally) "Generate a QR code for this"
//   (Optionally) "Send this to my device"
//
// It is possible for there to be zero items in this submenu, in which case
// callers should take care not to actually add it to the containing menu.
class ShareSubmenuModel : public ui::SimpleMenuModel,
                          public ui::SimpleMenuModel::Delegate {
 public:
  enum class Context {
    // We're offering to share an entire page
    PAGE,

    // We're offering to share a specified link
    LINK,

    // We're offering to share a specified image
    IMAGE,
  };

  // The |url| parameter is a bit tricky: it is the "target URL" of the
  // containing menu, whatever that happens to be. The exact meaning of that
  // depends on |context|.
  ShareSubmenuModel(Browser* browser, Context context, GURL url);
  ~ShareSubmenuModel() override;

  // ui::SimpleMenuModel::Delegate:
  void ExecuteCommand(int id, int event_flags) override;

 private:
  void AddGenerateQRCodeItem();
  void AddSendTabToSelfItem();

  void AddSendTabToSelfSingleTargetItem();

  void GenerateQRCode();
  void SendTabToSelfSingleTarget();

  Browser* browser_;
  Context context_;
  GURL url_;

  std::unique_ptr<send_tab_to_self::SendTabToSelfSubMenuModel>
      stts_submenu_model_;
};

}  // namespace sharing

#endif  // CHROME_BROWSER_SHARING_SHARE_SUBMENU_MODEL_H_
