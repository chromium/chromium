// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTERNAL_PROTOCOL_DIALOG_H_
#define CHROME_BROWSER_CHROMEOS_EXTERNAL_PROTOCOL_DIALOG_H_

#include <string>

#include "base/macros.h"
#include "base/strings/string16.h"
#include "base/time/time.h"
#include "ui/views/window/dialog_delegate.h"

class GURL;

namespace content {
class WebContents;
}

namespace views {
class MessageBoxView;
}

// The external protocol dialog for Chrome OS shown when there are no handlers.
class ExternalProtocolNoHandlersDialog : public views::DialogDelegate {
 public:
  ExternalProtocolNoHandlersDialog(content::WebContents* web_contents,
                                   const GURL& url);
  ~ExternalProtocolNoHandlersDialog() override;

  // views::DialogDelegate:
  base::string16 GetWindowTitle() const override;
  views::View* GetContentsView() override;
  const views::Widget* GetWidget() const override;
  views::Widget* GetWidget() override;

 private:
  // The message box view whose commands we handle.
  views::MessageBoxView* message_box_view_;

  // The time at which this dialog was created.
  base::TimeTicks creation_time_;

  // The scheme of the url.
  std::string scheme_;

  DISALLOW_COPY_AND_ASSIGN(ExternalProtocolNoHandlersDialog);
};

#endif  // CHROME_BROWSER_CHROMEOS_EXTERNAL_PROTOCOL_DIALOG_H_
