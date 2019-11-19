// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/protected_media_identifier_permission_context.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/string_split.h"
#include "build/build_config.h"
#include "chrome/browser/content_settings/tab_specific_content_settings.h"
#include "chrome/browser/permissions/permission_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "media/base/media_switches.h"
#include "net/base/url_util.h"
#if defined(OS_CHROMEOS)
#include <utility>

#include "base/metrics/histogram_macros.h"
#include "chrome/browser/chromeos/attestation/platform_verification_dialog.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chromeos/constants/chromeos_switches.h"
#include "chromeos/settings/cros_settings_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/user_prefs/user_prefs.h"
#include "ui/views/widget/widget.h"
#elif !defined(OS_ANDROID)
#error This file currently only supports Chrome OS and Android.
#endif

#if defined(OS_CHROMEOS)
using chromeos::attestation::PlatformVerificationDialog;
#endif

ProtectedMediaIdentifierPermissionContext::
    ProtectedMediaIdentifierPermissionContext(Profile* profile)
    : PermissionContextBase(profile,
                            ContentSettingsType::PROTECTED_MEDIA_IDENTIFIER,
                            blink::mojom::FeaturePolicyFeature::kEncryptedMedia)
#if defined(OS_CHROMEOS)

#endif
{
}

ProtectedMediaIdentifierPermissionContext::
    ~ProtectedMediaIdentifierPermissionContext() {
}

#if defined(OS_CHROMEOS)
void ProtectedMediaIdentifierPermissionContext::DecidePermission(
    content::WebContents* web_contents,
    const PermissionRequestID& id,
    const GURL& requesting_origin,
    const GURL& embedding_origin,
    bool user_gesture,
    BrowserPermissionCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Since the dialog is modal, we only support one prompt per |web_contents|.
  // Reject the new one if there is already one pending. See
  // http://crbug.com/447005
  if (pending_requests_.count(web_contents)) {
    std::move(callback).Run(CONTENT_SETTING_ASK);
    return;
  }

  // ShowDialog doesn't use the callback if it returns null.
  auto repeating_callback =
      base::AdaptCallbackForRepeating(std::move(callback));

  // On ChromeOS, we don't use PermissionContextBase::RequestPermission() which
  // uses the standard permission infobar/bubble UI. See http://crbug.com/454847
  // Instead, we show the existing platform verification UI.
  // TODO(xhwang): Remove when http://crbug.com/454847 is fixed.
  views::Widget* widget = PlatformVerificationDialog::ShowDialog(
      web_contents, requesting_origin,
      base::BindOnce(&ProtectedMediaIdentifierPermissionContext::
                         OnPlatformVerificationConsentResponse,
                     weak_factory_.GetWeakPtr(), web_contents, id,
                     requesting_origin, embedding_origin, repeating_callback));

  // This could happen when the permission is requested from an extension. See
  // http://crbug.com/728534
  if (!widget) {
    std::move(repeating_callback).Run(CONTENT_SETTING_ASK);
    return;
  }

  pending_requests_.insert(
      std::make_pair(web_contents, std::make_pair(widget, id)));
}
#endif  // defined(OS_CHROMEOS)

ContentSetting
ProtectedMediaIdentifierPermissionContext::GetPermissionStatusInternal(
    content::RenderFrameHost* render_frame_host,
    const GURL& requesting_origin,
    const GURL& embedding_origin) const {
  DVLOG(1) << __func__ << ": (" << requesting_origin.spec() << ", "
           << embedding_origin.spec() << ")";

  if (!requesting_origin.is_valid() || !embedding_origin.is_valid() ||
      !IsProtectedMediaIdentifierEnabled()) {
    return CONTENT_SETTING_BLOCK;
  }

  ContentSetting content_setting =
      PermissionContextBase::GetPermissionStatusInternal(
          render_frame_host, requesting_origin, embedding_origin);
  DCHECK(content_setting == CONTENT_SETTING_ALLOW ||
         content_setting == CONTENT_SETTING_BLOCK ||
         content_setting == CONTENT_SETTING_ASK);

  // For automated testing of protected content - having a prompt that
  // requires user intervention is problematic. If the domain has been
  // whitelisted as safe - suppress the request and allow.
  if (content_setting == CONTENT_SETTING_ASK &&
      IsOriginWhitelisted(requesting_origin)) {
    content_setting = CONTENT_SETTING_ALLOW;
  }

  return content_setting;
}

bool ProtectedMediaIdentifierPermissionContext::IsOriginWhitelisted(
    const GURL& origin) {
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();

  const std::string whitelist = command_line.GetSwitchValueASCII(
      switches::kUnsafelyAllowProtectedMediaIdentifierForDomain);

  for (const std::string& domain : base::SplitString(
           whitelist, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY)) {
    if (origin.DomainIs(domain)) {
      return true;
    }
  }

  return false;
}

void ProtectedMediaIdentifierPermissionContext::UpdateTabContext(
    const PermissionRequestID& id,
    const GURL& requesting_frame,
    bool allowed) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // WebContents may have gone away.
  TabSpecificContentSettings* content_settings =
      TabSpecificContentSettings::GetForFrame(id.render_process_id(),
                                              id.render_frame_id());
  if (content_settings) {
    content_settings->OnProtectedMediaIdentifierPermissionSet(
        requesting_frame.GetOrigin(), allowed);
  }
}

bool ProtectedMediaIdentifierPermissionContext::IsRestrictedToSecureOrigins()
    const {
  // EME is not supported on insecure origins, see https://goo.gl/Ks5zf7
  // Note that origins whitelisted by --unsafely-treat-insecure-origin-as-secure
  // flag will be treated as "secure" so they will not be affected.
  return true;
}

// TODO(xhwang): We should consolidate the "protected content" related pref
// across platforms.
bool ProtectedMediaIdentifierPermissionContext::
    IsProtectedMediaIdentifierEnabled() const {
#if defined(OS_CHROMEOS)
  // Platform verification is not allowed in incognito or guest mode.
  if (profile()->IsOffTheRecord() || profile()->IsGuestSession()) {
    DVLOG(1) << "Protected media identifier disabled in incognito or guest "
                "mode.";
    return false;
  }

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(chromeos::switches::kSystemDevMode) &&
      !command_line->HasSwitch(chromeos::switches::kAllowRAInDevMode)) {
    DVLOG(1) << "Protected media identifier disabled in dev mode.";
    return false;
  }

  // This could be disabled by the device policy or by user's master switch.
  bool enabled_for_device = false;
  if (!chromeos::CrosSettings::Get()->GetBoolean(
          chromeos::kAttestationForContentProtectionEnabled,
          &enabled_for_device) ||
      !enabled_for_device ||
      !profile()->GetPrefs()->GetBoolean(prefs::kEnableDRM)) {
    DVLOG(1) << "Protected media identifier disabled by the user or by device "
                "policy.";
    return false;
  }
#endif

  return true;
}

#if defined(OS_CHROMEOS)

static void ReportPermissionActionUMA(PermissionAction action) {
  UMA_HISTOGRAM_ENUMERATION("Permissions.Action.ProtectedMedia", action,
                            PermissionAction::NUM);
}

void ProtectedMediaIdentifierPermissionContext::
    OnPlatformVerificationConsentResponse(
        content::WebContents* web_contents,
        const PermissionRequestID& id,
        const GURL& requesting_origin,
        const GURL& embedding_origin,
        BrowserPermissionCallback callback,
        PlatformVerificationDialog::ConsentResponse response) {
  // The request may have been canceled. Drop the callback in that case.
  // This can happen if the tab is closed.
  PendingRequestMap::iterator request = pending_requests_.find(web_contents);
  if (request == pending_requests_.end()) {
    VLOG(1) << "Platform verification ignored by user.";
    ReportPermissionActionUMA(PermissionAction::IGNORED);
    return;
  }

  DCHECK(request->second.second == id);
  pending_requests_.erase(request);

  ContentSetting content_setting = CONTENT_SETTING_ASK;
  bool persist = false; // Whether the ContentSetting should be saved.
  switch (response) {
    case PlatformVerificationDialog::CONSENT_RESPONSE_NONE:
      // This can happen if user clicked "x", or pressed "Esc", or navigated
      // away without closing the tab.
      VLOG(1) << "Platform verification dismissed by user.";
      ReportPermissionActionUMA(PermissionAction::DISMISSED);
      content_setting = CONTENT_SETTING_ASK;
      persist = false;
      break;
    case PlatformVerificationDialog::CONSENT_RESPONSE_ALLOW:
      VLOG(1) << "Platform verification accepted by user.";
      base::RecordAction(
          base::UserMetricsAction("PlatformVerificationAccepted"));
      ReportPermissionActionUMA(PermissionAction::GRANTED);
      content_setting = CONTENT_SETTING_ALLOW;
      persist = true;
      break;
    case PlatformVerificationDialog::CONSENT_RESPONSE_DENY:
      VLOG(1) << "Platform verification denied by user.";
      base::RecordAction(
          base::UserMetricsAction("PlatformVerificationRejected"));
      ReportPermissionActionUMA(PermissionAction::DENIED);
      content_setting = CONTENT_SETTING_BLOCK;
      persist = true;
      break;
  }

  NotifyPermissionSet(id, requesting_origin, embedding_origin,
                      std::move(callback), persist, content_setting);
}
#endif
