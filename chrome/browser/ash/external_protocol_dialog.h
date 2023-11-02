// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_EXTERNAL_PROTOCOL_DIALOG_H_
#define CHROME_BROWSER_ASH_EXTERNAL_PROTOCOL_DIALOG_H_

#include <string>

#include "base/time/time.h"
#include "ui/views/window/dialog_delegate.h"

namespace aura {
class Window;
}

namespace views {
class MessageBoxView;
}

namespace ash {

// The external protocol dialog for Chrome OS shown when we have a URL with a
// Tel scheme but there are no handlers.
class ExternalProtocolNoHandlersTelSchemeDialog : public views::DialogDelegate {
 public:
  explicit ExternalProtocolNoHandlersTelSchemeDialog(
      aura::Window* parent_window);

  ExternalProtocolNoHandlersTelSchemeDialog(
      const ExternalProtocolNoHandlersTelSchemeDialog&) = delete;
  ExternalProtocolNoHandlersTelSchemeDialog& operator=(
      const ExternalProtocolNoHandlersTelSchemeDialog&) = delete;

  ~ExternalProtocolNoHandlersTelSchemeDialog() override;

  // views::DialogDelegate:
  std::u16string GetWindowTitle() const override;
  views::View* GetContentsView() override;
  const views::Widget* GetWidget() const override;
  views::Widget* GetWidget() override;

 private:
  // The message box view whose commands we handle.
  views::MessageBoxView* message_box_view_;

  // The time at which this dialog was created.
  base::TimeTicks creation_time_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_EXTERNAL_PROTOCOL_DIALOG_H_
