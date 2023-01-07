// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_TEST_EXTENSION_MESSAGE_BUBBLE_DELEGATE_H_
#define CHROME_BROWSER_EXTENSIONS_TEST_EXTENSION_MESSAGE_BUBBLE_DELEGATE_H_

#include <map>
#include <set>
#include <string>

#include "chrome/browser/extensions/extension_message_bubble_controller.h"
#include "extensions/common/extension_id.h"

namespace extensions {

// A programmable test delegate to exercise the ExtensionMessageBubbleController
// framework.
class TestExtensionMessageBubbleDelegate
    : public ExtensionMessageBubbleController::Delegate {
 public:
  explicit TestExtensionMessageBubbleDelegate(Profile* profile);
  TestExtensionMessageBubbleDelegate(
      const TestExtensionMessageBubbleDelegate&) = delete;
  TestExtensionMessageBubbleDelegate& operator=(
      const TestExtensionMessageBubbleDelegate&) = delete;
  ~TestExtensionMessageBubbleDelegate() override;

  // ExtensionMessageBubbleController::Delegate:
  bool ShouldIncludeExtension(const Extension* extension) override;
  void AcknowledgeExtension(
      const ExtensionId& extension_id,
      ExtensionMessageBubbleController::BubbleAction action) override;
  void PerformAction(const ExtensionIdList& list) override;
  std::u16string GetTitle() const override;
  std::u16string GetMessageBody(bool anchored_to_browser_action,
                                int extension_count) const override;
  std::u16string GetOverflowText(
      const std::u16string& overflow_count) const override;
  std::u16string GetLearnMoreLabel() const override;
  GURL GetLearnMoreUrl() const override;
  std::u16string GetActionButtonLabel() const override;
  std::u16string GetDismissButtonLabel() const override;
  bool ShouldCloseOnDeactivate() const override;
  bool ShouldShow(const ExtensionIdList& extensions) const override;
  void OnShown(const ExtensionIdList& extensions) override;
  void ClearProfileSetForTesting() override;
  bool ShouldShowExtensionList() const override;
  bool ShouldLimitToEnabledExtensions() const override;
  bool SupportsPolicyIndicator() override;

  // Indicate that `ShouldIncludeExtension()` should always return true for
  // the given `extension_id`.
  void IncludeExtensionId(const ExtensionId& extension_id);

  // Returns true if the given `extension_id` was acknowledged.
  bool WasExtensionAcknowledged(const ExtensionId& extension_id) const;

 private:
  std::set<ExtensionId> extension_ids_;

  std::map<ExtensionId, ExtensionMessageBubbleController::BubbleAction>
      acknowledged_extensions_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_TEST_EXTENSION_MESSAGE_BUBBLE_DELEGATE_H_
