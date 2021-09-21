// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHARE_SHARE_SUBMENU_MODEL_H_
#define CHROME_BROWSER_SHARE_SHARE_SUBMENU_MODEL_H_

#include "base/feature_list.h"
#include "ui/base/data_transfer_policy/data_transfer_endpoint.h"
#include "ui/base/models/simple_menu_model.h"
#include "url/gurl.h"

class Browser;

namespace send_tab_to_self {
class SendTabToSelfSubMenuModel;
}

namespace share {

extern const base::Feature kShareMenu;

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
  // depends on |context|. The |source_endpoint| is the source of |url| or
  // whichever other data is being offered for share (image or similar).
  ShareSubmenuModel(Browser* browser,
                    std::unique_ptr<ui::DataTransferEndpoint> source_endpoint,
                    Context context,
                    GURL url);
  ~ShareSubmenuModel() override;

  // ui::SimpleMenuModel::Delegate:
  void ExecuteCommand(int id, int event_flags) override;

 private:
  void AddGenerateQRCodeItem();
  void AddSendTabToSelfItem();
  void AddCopyLinkItem();

  void AddSendTabToSelfSingleTargetItem();

  void GenerateQRCode();
  void SendTabToSelfSingleTarget();
  void CopyLink();

  Browser* browser_;
  std::unique_ptr<ui::DataTransferEndpoint> source_endpoint_;
  Context context_;
  GURL url_;

  std::unique_ptr<send_tab_to_self::SendTabToSelfSubMenuModel>
      stts_submenu_model_;
};

}  // namespace share

#endif  // CHROME_BROWSER_SHARE_SHARE_SUBMENU_MODEL_H_
