// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERMISSIONS_CHROME_PERMISSIONS_CLIENT_H_
#define CHROME_BROWSER_PERMISSIONS_CHROME_PERMISSIONS_CLIENT_H_

#include "base/no_destructor.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "components/permissions/features.h"
#include "components/permissions/permission_request_enums.h"
#include "components/permissions/permission_uma_util.h"
#include "components/permissions/permissions_client.h"

class ChromePermissionsClient : public permissions::PermissionsClient {
 public:
  ChromePermissionsClient(const ChromePermissionsClient&) = delete;
  ChromePermissionsClient& operator=(const ChromePermissionsClient&) = delete;

  static ChromePermissionsClient* GetInstance();

  // PermissionsClient:
  HostContentSettingsMap* GetSettingsMap(
      content::BrowserContext* browser_context) override;
  scoped_refptr<content_settings::CookieSettings> GetCookieSettings(
      content::BrowserContext* browser_context) override;
  privacy_sandbox::TrackingProtectionSettings* GetTrackingProtectionSettings(
      content::BrowserContext* browser_context) override;
  bool IsSubresourceFilterActivated(content::BrowserContext* browser_context,
                                    const GURL& url) override;
  permissions::OriginKeyedPermissionActionService*
  GetOriginKeyedPermissionActionService(
      content::BrowserContext* browser_context) override;
  permissions::PermissionActionsHistory* GetPermissionActionsHistory(
      content::BrowserContext* browser_context) override;
  permissions::PermissionDecisionAutoBlocker* GetPermissionDecisionAutoBlocker(
      content::BrowserContext* browser_context) override;
  permissions::ObjectPermissionContextBase* GetChooserContext(
      content::BrowserContext* browser_context,
      ContentSettingsType type) override;
  double GetSiteEngagementScore(content::BrowserContext* browser_context,
                                const GURL& origin) override;
  void AreSitesImportant(
      content::BrowserContext* browser_context,
      std::vector<std::pair<url::Origin, bool>>* urls) override;
  bool IsCookieDeletionDisabled(content::BrowserContext* browser_context,
                                const GURL& origin) override;
  void GetUkmSourceId(ContentSettingsType permission_type,
                      content::BrowserContext* browser_context,
                      content::WebContents* web_contents,
                      const GURL& requesting_origin,
                      GetUkmSourceIdCallback callback) override;
  permissions::IconId GetOverrideIconId(
      permissions::RequestType request_type) override;
  std::vector<std::unique_ptr<permissions::PermissionUiSelector>>
  CreatePermissionUiSelectors(
      content::BrowserContext* browser_context) override;

  void TriggerPromptHatsSurveyIfEnabled(
      content::WebContents* web_contents,
      permissions::RequestType request_type,
      std::optional<permissions::PermissionAction> action,
      permissions::PermissionPromptDisposition prompt_disposition,
      permissions::PermissionPromptDispositionReason prompt_disposition_reason,
      permissions::PermissionRequestGestureType gesture_type,
      std::optional<base::TimeDelta> prompt_display_duration,
      bool is_post_prompt,
      const GURL& gurl,
      std::optional<
          permissions::feature_params::PermissionElementPromptPosition>
          pepc_prompt_position,
      ContentSetting initial_permission_status,
      base::OnceCallback<void()> hats_shown_callback_) override;

#if !BUILDFLAG(IS_ANDROID)
  permissions::PermissionIgnoredReason DetermineIgnoreReason(
      content::WebContents* web_contents) override;
#endif

  void OnPromptResolved(
      permissions::RequestType request_type,
      permissions::PermissionAction action,
      const GURL& origin,
      permissions::PermissionPromptDisposition prompt_disposition,
      permissions::PermissionPromptDispositionReason prompt_disposition_reason,
      permissions::PermissionRequestGestureType gesture_type,
      std::optional<QuietUiReason> quiet_ui_reason,
      base::TimeDelta prompt_display_duration,
      std::optional<
          permissions::feature_params::PermissionElementPromptPosition>
          pepc_prompt_position,
      ContentSetting initial_permission_status,
      content::WebContents* web_contents) override;
  std::optional<bool> HadThreeConsecutiveNotificationPermissionDenies(
      content::BrowserContext* browser_context) override;
  std::optional<bool> HasPreviouslyAutoRevokedPermission(
      content::BrowserContext* browser_context,
      const GURL& origin,
      ContentSettingsType permission) override;
  std::optional<url::Origin> GetAutoApprovalOrigin(
      content::BrowserContext* browser_context) override;
  std::optional<permissions::PermissionAction> GetAutoApprovalStatus(
      content::BrowserContext* browser_context,
      const GURL& origin) override;
  bool CanBypassEmbeddingOriginCheck(const GURL& requesting_origin,
                                     const GURL& embedding_origin) override;
  std::optional<GURL> OverrideCanonicalOrigin(
      const GURL& requesting_origin,
      const GURL& embedding_origin) override;
  // Checks if `requesting_origin` and `embedding_origin` are the new tab page
  // origins.
  bool DoURLsMatchNewTabPage(const GURL& requesting_origin,
                             const GURL& embedding_origin) override;
#if BUILDFLAG(IS_ANDROID)
  bool IsDseOrigin(content::BrowserContext* browser_context,
                   const url::Origin& origin) override;
  infobars::InfoBarManager* GetInfoBarManager(
      content::WebContents* web_contents) override;
  infobars::InfoBar* MaybeCreateInfoBar(
      content::WebContents* web_contents,
      ContentSettingsType type,
      base::WeakPtr<permissions::PermissionPromptAndroid> prompt) override;
  std::unique_ptr<PermissionMessageDelegate> MaybeCreateMessageUI(
      content::WebContents* web_contents,
      ContentSettingsType type,
      base::WeakPtr<permissions::PermissionPromptAndroid> prompt) override;
  void RepromptForAndroidPermissions(
      content::WebContents* web_contents,
      const std::vector<ContentSettingsType>& content_settings_types,
      const std::vector<ContentSettingsType>& filtered_content_settings_types,
      const std::vector<std::string>& required_permissions,
      const std::vector<std::string>& optional_permissions,
      PermissionsUpdatedCallback callback) override;
  int MapToJavaDrawableId(int resource_id) override;
  favicon::FaviconService* GetFaviconService(
      content::BrowserContext* browser_context) override;
#else
  std::unique_ptr<permissions::PermissionPrompt> CreatePrompt(
      content::WebContents* web_contents,
      permissions::PermissionPrompt::Delegate* delegate) override;
#endif

  bool HasDevicePermission(ContentSettingsType type) const override;
  bool CanRequestDevicePermission(ContentSettingsType type) const override;

 private:
  friend base::NoDestructor<ChromePermissionsClient>;

  ChromePermissionsClient() = default;
};

#endif  // CHROME_BROWSER_PERMISSIONS_CHROME_PERMISSIONS_CLIENT_H_
