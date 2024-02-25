// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/idle/idle_detection_permission_context.h"

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/rand_util.h"
#include "chrome/browser/visibility_timer_tab_helper.h"
#include "components/content_settings/browser/page_specific_content_settings.h"
#include "components/permissions/permission_request_id.h"
#include "content/public/browser/browser_context.h"
#include "third_party/blink/public/mojom/permissions_policy/permissions_policy_feature.mojom.h"
#include "url/gurl.h"

IdleDetectionPermissionContext::IdleDetectionPermissionContext(
    content::BrowserContext* browser_context)
    : PermissionContextBase(
          browser_context,
          ContentSettingsType::IDLE_DETECTION,
          blink::mojom::PermissionsPolicyFeature::kIdleDetection) {}

IdleDetectionPermissionContext::~IdleDetectionPermissionContext() = default;

void IdleDetectionPermissionContext::UpdateTabContext(
    const permissions::PermissionRequestID& id,
    const GURL& requesting_frame,
    bool allowed) {
  content_settings::PageSpecificContentSettings* content_settings =
      content_settings::PageSpecificContentSettings::GetForFrame(
          id.global_render_frame_host_id());
  if (!content_settings)
    return;

  if (allowed)
    content_settings->OnContentAllowed(ContentSettingsType::IDLE_DETECTION);
  else
    content_settings->OnContentBlocked(ContentSettingsType::IDLE_DETECTION);
}

void IdleDetectionPermissionContext::DecidePermission(
    permissions::PermissionRequestData request_data,
    permissions::BrowserPermissionCallback callback) {
  // Idle detection permission is always denied in incognito. To prevent sites
  // from using that to detect whether incognito mode is active, we deny after a
  // random time delay, to simulate a user clicking a bubble/infobar. See also
  // ContentSettingsRegistry::Init, which marks idle detection as
  // INHERIT_IF_LESS_PERMISSIVE, and
  // PermissionMenuModel::PermissionMenuModel which prevents users from manually
  // allowing the permission.
  if (browser_context()->IsOffTheRecord()) {
    content::RenderFrameHost* rfh = content::RenderFrameHost::FromID(
        request_data.id.global_render_frame_host_id());

    content::WebContents* web_contents =
        content::WebContents::FromRenderFrameHost(rfh);

    // Random number of seconds in the range [1.0, 2.0).
    double delay_seconds = 1.0 + 1.0 * base::RandDouble();
    VisibilityTimerTabHelper::CreateForWebContents(web_contents);
    VisibilityTimerTabHelper::FromWebContents(web_contents)
        ->PostTaskAfterVisibleDelay(
            FROM_HERE,
            base::BindOnce(&IdleDetectionPermissionContext::NotifyPermissionSet,
                           weak_factory_.GetWeakPtr(), request_data.id,
                           request_data.requesting_origin,
                           request_data.embedding_origin, std::move(callback),
                           /*persist=*/true, CONTENT_SETTING_BLOCK,
                           /*is_one_time=*/false, /*is_final_decision=*/true),
            base::Seconds(delay_seconds));
    return;
  }

  PermissionContextBase::DecidePermission(std::move(request_data),
                                          std::move(callback));
}
