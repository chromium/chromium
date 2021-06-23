// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_SUSPICIOUS_EXTENSION_BUBBLE_DELEGATE_H_
#define CHROME_BROWSER_EXTENSIONS_SUSPICIOUS_EXTENSION_BUBBLE_DELEGATE_H_

#include <stddef.h>

#include "base/macros.h"
#include "chrome/browser/extensions/extension_message_bubble_controller.h"

namespace extensions {

class SuspiciousExtensionBubbleDelegate
    : public ExtensionMessageBubbleController::Delegate {
 public:
  explicit SuspiciousExtensionBubbleDelegate(Profile* profile);
  ~SuspiciousExtensionBubbleDelegate() override;

  // ExtensionMessageBubbleController::Delegate methods.
  bool ShouldIncludeExtension(const extensions::Extension* extension) override;
  void AcknowledgeExtension(
      const std::string& extension_id,
      ExtensionMessageBubbleController::BubbleAction user_action) override;
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
  bool ShouldShowExtensionList() const override;
  bool ShouldShow(const ExtensionIdList& extensions) const override;
  void OnShown(const ExtensionIdList& extensions) override;
  void OnAction() override;
  void ClearProfileSetForTesting() override;
  bool ShouldLimitToEnabledExtensions() const override;
  bool SupportsPolicyIndicator() override;

 private:
  Profile* profile_;

  DISALLOW_COPY_AND_ASSIGN(SuspiciousExtensionBubbleDelegate);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_SUSPICIOUS_EXTENSION_BUBBLE_DELEGATE_H_
