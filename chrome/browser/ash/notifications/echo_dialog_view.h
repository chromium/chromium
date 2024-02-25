// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_NOTIFICATIONS_ECHO_DIALOG_VIEW_H_
#define CHROME_BROWSER_ASH_NOTIFICATIONS_ECHO_DIALOG_VIEW_H_

#include "base/functional/callback.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/window/dialog_delegate.h"

namespace views {
class View;
}  // namespace views

namespace gfx {
class FontList;
}  // namespace gfx

namespace ash {

class EchoDialogListener;

// Dialog shown by echoPrivate extension API when getUserConsent function is
// called. The API is used by echo extension when an offer from a service is
// being redeemed. The dialog is shown to get an user consent. If the echo
// extension is not allowed by policy to redeem offers, the dialog informs user
// about this.
class EchoDialogView : public views::DialogDelegateView {
  METADATA_HEADER(EchoDialogView, views::DialogDelegateView)

 public:
  struct Params {
    bool echo_enabled = false;
    std::u16string service_name;
    std::u16string origin;
  };

  EchoDialogView(EchoDialogListener* listener, const Params& params);
  EchoDialogView(const EchoDialogView&) = delete;
  EchoDialogView& operator=(const EchoDialogView&) = delete;
  ~EchoDialogView() override;

  // Shows the dialog.
  void Show(gfx::NativeWindow parent);

  // The callback is invoked after any dialog is shown. Test-only.
  using ShowCallback = base::OnceCallback<void(EchoDialogView*)>;
  static void AddShowCallbackForTesting(ShowCallback callback);

 private:
  friend class ExtensionEchoPrivateApiTest;

  // Initializes dialog layout that will be showed when echo extension is
  // allowed to redeem offers. |service_name| is the name of the service that
  // requests user consent to redeem an offer. |origin| is the service's origin
  // url. Service name should be underlined in the dialog, and hovering over its
  // label should display tooltip containing |origin|.
  // The dialog will have both OK and Cancel buttons.
  void InitForEnabledEcho(const std::u16string& service_name,
                          const std::u16string& origin);

  // Initializes dialog layout that will be shown when echo extension is not
  // allowed to redeem offers. The dialog will be showing a message that the
  // offer redeeming is disabled by policy.
  // The dialog will have only Cancel button.
  void InitForDisabledEcho();

  // Sets the border and label view.
  void SetBorderAndLabel(std::unique_ptr<views::View> label,
                         const gfx::FontList& label_font_list);
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_NOTIFICATIONS_ECHO_DIALOG_VIEW_H_
