// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_GENERATED_PASSWORD_SAVED_INFOBAR_DELEGATE_ANDROID_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_GENERATED_PASSWORD_SAVED_INFOBAR_DELEGATE_ANDROID_H_

#include <string>

#include "base/macros.h"
#include "components/infobars/core/infobar_delegate.h"
#include "ui/gfx/range/range.h"

namespace content {
class WebContents;
}

// Android-only infobar to notify that the generated password was saved.
class GeneratedPasswordSavedInfoBarDelegateAndroid
    : public infobars::InfoBarDelegate {
 public:
  // Creates and shows the infobar. Implemented with
  // GeneratedPasswordSavedInfoBar.
  static void Create(content::WebContents* web_contents);

  ~GeneratedPasswordSavedInfoBarDelegateAndroid() override;

  // Returns the translated text of the message to display.
  const std::u16string& message_text() const { return message_text_; }

  // Returns the translated text of the details message to display. T
  const std::u16string& details_message_text() const {
    return details_message_text_;
  }

  // Returns the range of the details message text that should be a link.
  const gfx::Range& inline_link_range() const { return inline_link_range_; }

  // Returns the translated label of the button.
  const std::u16string& button_label() const { return button_label_; }

  // Called when the link in the message is clicked.
  void OnInlineLinkClicked();

 private:
  GeneratedPasswordSavedInfoBarDelegateAndroid();

  // infobars::InfoBarDelegate:
  infobars::InfoBarDelegate::InfoBarIdentifier GetIdentifier() const override;
  int GetIconId() const override;

  // The translated text of the message to display.
  std::u16string message_text_;

  // The translated text of the details message to display. This message
  // explains where the generated password is saved.
  std::u16string details_message_text_;

  // The range of the details message that should be a link.
  gfx::Range inline_link_range_;

  // The translated label of the button.
  std::u16string button_label_;

  DISALLOW_COPY_AND_ASSIGN(GeneratedPasswordSavedInfoBarDelegateAndroid);
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_GENERATED_PASSWORD_SAVED_INFOBAR_DELEGATE_ANDROID_H_
