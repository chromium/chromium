// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERMISSIONS_PERMISSION_INFOBAR_DELEGATE_ANDROID_H_
#define CHROME_BROWSER_PERMISSIONS_PERMISSION_INFOBAR_DELEGATE_ANDROID_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/permissions/quiet_permission_prompt_model_android.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/infobars/core/confirm_infobar_delegate.h"

namespace infobars {
class ContentInfoBarManager;
}

namespace permissions {
class PermissionPromptAndroid;
}

// An InfoBar that displays a permission request.
class PermissionInfoBarDelegate : public ConfirmInfoBarDelegate {
 public:
  PermissionInfoBarDelegate(const PermissionInfoBarDelegate&) = delete;
  PermissionInfoBarDelegate& operator=(const PermissionInfoBarDelegate&) =
      delete;

  // Public so we can have std::unique_ptr<PermissionInfoBarDelegate>.
  ~PermissionInfoBarDelegate() override;

  static infobars::InfoBar* Create(
      const base::WeakPtr<permissions::PermissionPromptAndroid>&
          permission_prompt,
      infobars::ContentInfoBarManager* infobar_manager);

  size_t PermissionCount() const;

  ContentSettingsType GetContentSettingType(size_t position) const;

  // Returns the string to show in the infobar in its compact state.
  std::u16string GetCompactMessageText() const;

  // Returns the title of the link to show in the infobar in its compact state.
  std::u16string GetCompactLinkText() const;

  // Returns the secondary string to show in the infobar in the expanded state.
  std::u16string GetDescriptionText() const;

  // Whether the secondary button should open site settings.
  bool ShouldSecondaryButtonOpenSettings() const;

  // ConfirmInfoBarDelegate:
  int GetIconId() const override;
  std::u16string GetLinkText() const override;
  GURL GetLinkURL() const override;
  bool LinkClicked(WindowOpenDisposition disposition) override;
  void InfoBarDismissed() override;
  std::u16string GetMessageText() const override;
  bool Accept() override;
  bool Cancel() override;

 private:
  PermissionInfoBarDelegate(
      const base::WeakPtr<permissions::PermissionPromptAndroid>&
          permission_prompt,
      infobars::ContentInfoBarManager* infobar_manager);

  // ConfirmInfoBarDelegate:
  InfoBarIdentifier GetIdentifier() const override;
  int GetButtons() const override;
  std::u16string GetButtonLabel(InfoBarButton button) const override;

  // InfoBarDelegate:
  bool EqualsDelegate(infobars::InfoBarDelegate* delegate) const override;

  base::WeakPtr<permissions::PermissionPromptAndroid> permission_prompt_;
  raw_ptr<infobars::ContentInfoBarManager> infobar_manager_;
  bool details_expanded_;

  QuietPermissionPromptModelAndroid prompt_model_;
};

#endif  // CHROME_BROWSER_PERMISSIONS_PERMISSION_INFOBAR_DELEGATE_ANDROID_H_
