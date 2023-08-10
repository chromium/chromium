// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_SETTINGS_API_BUBBLE_DELEGATE_H_
#define CHROME_BROWSER_EXTENSIONS_SETTINGS_API_BUBBLE_DELEGATE_H_

#include <stddef.h>

#include <string>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/extensions/extension_message_bubble_controller.h"
#include "chrome/common/extensions/manifest_handlers/settings_overrides_handler.h"
#include "extensions/common/extension_id.h"

namespace extensions {

class SettingsApiBubbleDelegate
    : public ExtensionMessageBubbleController::Delegate {
 public:
  SettingsApiBubbleDelegate(Profile* profile, SettingsApiOverrideType type);

  SettingsApiBubbleDelegate(const SettingsApiBubbleDelegate&) = delete;
  SettingsApiBubbleDelegate& operator=(const SettingsApiBubbleDelegate&) =
      delete;

  ~SettingsApiBubbleDelegate() override;

  // The preference used to indicate if the user has acknowledged the extension
  // taking over some aspect of the user's settings (homepage, startup pages,
  // or search engine).
  // TODO(devlin): We currently use one preference for all of these, but that's
  // probably not desirable.
  static const char kAcknowledgedPreference[];

  // ExtensionMessageBubbleController::Delegate methods.
  bool ShouldIncludeExtension(const Extension* extension) override;
  void AcknowledgeExtension(
      const std::string& extension_id,
      ExtensionMessageBubbleController::BubbleAction user_action) override;
  void PerformAction(const ExtensionIdList& list) override;
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
  bool ShouldLimitToEnabledExtensions() const override;
  bool SupportsPolicyIndicator() override;

 private:
  // Returns a key unique to the type of bubble that can be used to retrieve
  // state specific to the type (e.g., shown for profiles).
  const char* GetKey() const;
  // The type of settings override this bubble will report on. This can be, for
  // example, a bubble to notify the user that the search engine has been
  // changed by an extension (or homepage/startup pages/etc).
  SettingsApiOverrideType type_;

  // The ID of the extension we are showing the bubble for.
  ExtensionId extension_id_;

  raw_ptr<Profile> profile_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_SETTINGS_API_BUBBLE_DELEGATE_H_
