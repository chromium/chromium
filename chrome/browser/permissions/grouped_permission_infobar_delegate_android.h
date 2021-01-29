// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERMISSIONS_GROUPED_PERMISSION_INFOBAR_DELEGATE_ANDROID_H_
#define CHROME_BROWSER_PERMISSIONS_GROUPED_PERMISSION_INFOBAR_DELEGATE_ANDROID_H_

#include <memory>

#include "base/callback.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/infobars/core/confirm_infobar_delegate.h"

class InfoBarService;

namespace content {
class WebContents;
}

namespace permissions {
class PermissionPromptAndroid;
}

// An InfoBar that displays a permission request.
//
// TODO(crbug.com/986737): This class is only used for displaying notification
// permission requests and has nothing to do with grouped permissions anymore.
class GroupedPermissionInfoBarDelegate : public ConfirmInfoBarDelegate {
 public:
  // Public so we can have std::unique_ptr<GroupedPermissionInfoBarDelegate>.
  ~GroupedPermissionInfoBarDelegate() override;

  static infobars::InfoBar* Create(
      const base::WeakPtr<permissions::PermissionPromptAndroid>&
          permission_prompt,
      InfoBarService* infobar_service);

  size_t PermissionCount() const;

  ContentSettingsType GetContentSettingType(size_t position) const;

  // Returns the string to show in the infobar in its compact state.
  base::string16 GetCompactMessageText() const;

  // Returns the title of the link to show in the infobar in its compact state.
  base::string16 GetCompactLinkText() const;

  // Returns the secondary string to show in the infobar in the expanded state.
  base::string16 GetDescriptionText() const;

  // Whether the secondary button should open site settings.
  bool ShouldSecondaryButtonOpenSettings() const;

  // ConfirmInfoBarDelegate:
  int GetIconId() const override;
  base::string16 GetLinkText() const override;
  GURL GetLinkURL() const override;
  bool LinkClicked(WindowOpenDisposition disposition) override;
  void InfoBarDismissed() override;
  base::string16 GetMessageText() const override;
  bool Accept() override;
  bool Cancel() override;

  // Returns true if we should show the permission request as a mini-infobar.
  static bool ShouldShowMiniInfobar(content::WebContents* web_contents,
                                    ContentSettingsType type);

 private:
  GroupedPermissionInfoBarDelegate(
      const base::WeakPtr<permissions::PermissionPromptAndroid>&
          permission_prompt,
      InfoBarService* infobar_service);

  // ConfirmInfoBarDelegate:
  InfoBarIdentifier GetIdentifier() const override;
  int GetButtons() const override;
  base::string16 GetButtonLabel(InfoBarButton button) const override;

  // InfoBarDelegate:
  bool EqualsDelegate(infobars::InfoBarDelegate* delegate) const override;

  base::WeakPtr<permissions::PermissionPromptAndroid> permission_prompt_;
  InfoBarService* infobar_service_;
  bool details_expanded_;

  DISALLOW_COPY_AND_ASSIGN(GroupedPermissionInfoBarDelegate);
};

#endif  // CHROME_BROWSER_PERMISSIONS_GROUPED_PERMISSION_INFOBAR_DELEGATE_ANDROID_H_
