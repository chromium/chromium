// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_NTP_OVERRIDDEN_BUBBLE_DELEGATE_H_
#define CHROME_BROWSER_EXTENSIONS_NTP_OVERRIDDEN_BUBBLE_DELEGATE_H_

#include <stddef.h>

#include <string>

#include "base/macros.h"
#include "chrome/browser/extensions/extension_message_bubble_controller.h"

class PrefRegistrySimple;

namespace extensions {

class NtpOverriddenBubbleDelegate
    : public ExtensionMessageBubbleController::Delegate {
 public:
  // Name of the preference that says whether the user has been notified about
  // extension overriding the new tab page.
  static const char kNtpBubbleAcknowledged[];

  explicit NtpOverriddenBubbleDelegate(Profile* profile);
  ~NtpOverriddenBubbleDelegate() override;

  // Registers associated preferences.
  static void RegisterPrefs(PrefRegistrySimple* registry);

  // Iterates over existing NTP-overriding extensions installed in the given
  // |profile| and marks them as acknowledged. Stores a preference indicating
  // the action was completed. Subsequent calls will *not* acknowledge more
  // extensions. This is needed to avoid prompting users with existing
  // extensions when we expand the warning to new platforms.
  // TODO(devlin): Remove this in M62.
  static void MaybeAcknowledgeExistingNtpExtensions(Profile* profile);

  // ExtensionMessageBubbleController::Delegate methods.
  bool ShouldIncludeExtension(const extensions::Extension* extension) override;
  void AcknowledgeExtension(
      const std::string& extension_id,
      extensions::ExtensionMessageBubbleController::BubbleAction user_action)
      override;
  void PerformAction(const extensions::ExtensionIdList& list) override;
  base::string16 GetTitle() const override;
  base::string16 GetMessageBody(bool anchored_to_browser_action,
                                int extension_count) const override;
  base::string16 GetOverflowText(
      const base::string16& overflow_count) const override;
  GURL GetLearnMoreUrl() const override;
  base::string16 GetActionButtonLabel() const override;
  base::string16 GetDismissButtonLabel() const override;
  bool ShouldCloseOnDeactivate() const override;
  bool ShouldAcknowledgeOnDeactivate() const override;
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

  static void set_acknowledge_existing_extensions_for_testing(
      bool acknowledge_existing_extensions);

 private:
  Profile* profile_;

  // The ID of the extension we are showing the bubble for.
  std::string extension_id_;

  DISALLOW_COPY_AND_ASSIGN(NtpOverriddenBubbleDelegate);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_NTP_OVERRIDDEN_BUBBLE_DELEGATE_H_
