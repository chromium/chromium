// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_NTP_OVERRIDDEN_BUBBLE_DELEGATE_H_
#define CHROME_BROWSER_EXTENSIONS_NTP_OVERRIDDEN_BUBBLE_DELEGATE_H_

#include <stddef.h>

#include <string>

#include "base/macros.h"
#include "chrome/browser/extensions/extension_message_bubble_controller.h"

namespace extensions {

class NtpOverriddenBubbleDelegate
    : public ExtensionMessageBubbleController::Delegate {
 public:
  explicit NtpOverriddenBubbleDelegate(Profile* profile);
  ~NtpOverriddenBubbleDelegate() override;

  // ExtensionMessageBubbleController::Delegate methods.
  bool ShouldIncludeExtension(const extensions::Extension* extension) override;
  void AcknowledgeExtension(
      const std::string& extension_id,
      extensions::ExtensionMessageBubbleController::BubbleAction user_action)
      override;
  void PerformAction(const extensions::ExtensionIdList& list) override;
  std::u16string GetTitle() const override;
  std::u16string GetMessageBody(bool anchored_to_browser_action,
                                int extension_count) const override;
  std::u16string GetOverflowText(
      const std::u16string& overflow_count) const override;
  GURL GetLearnMoreUrl() const override;
  std::u16string GetActionButtonLabel() const override;
  std::u16string GetDismissButtonLabel() const override;
  bool ShouldCloseOnDeactivate() const override;
  bool ShouldShow(const ExtensionIdList& extensions) const override;
  void OnShown(const ExtensionIdList& extensions) override;
  void OnAction() override;
  void ClearProfileSetForTesting() override;
  bool ShouldShowExtensionList() const override;
  bool ShouldHighlightExtensions() const override;
  bool ShouldLimitToEnabledExtensions() const override;
  void LogExtensionCount(size_t count) override;
  void LogAction(ExtensionMessageBubbleController::BubbleAction) override;
  bool SupportsPolicyIndicator() override;

 private:
  Profile* profile_;

  // The ID of the extension we are showing the bubble for.
  std::string extension_id_;

  DISALLOW_COPY_AND_ASSIGN(NtpOverriddenBubbleDelegate);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_NTP_OVERRIDDEN_BUBBLE_DELEGATE_H_
