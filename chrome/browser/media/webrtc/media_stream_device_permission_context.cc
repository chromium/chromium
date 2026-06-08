// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/webrtc/media_stream_device_permission_context.h"

#include "base/command_line.h"
#include "chrome/browser/media/webrtc/media_stream_device_permissions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_utils.h"
#include "components/permissions/permission_decision.h"
#include "components/permissions/permission_request_data.h"
#include "components/permissions/permission_util.h"
#include "content/public/browser/permission_descriptor_util.h"
#include "content/public/browser/permission_request_description.h"
#include "content/public/browser/permission_result.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/url_constants.h"
#include "extensions/buildflags/buildflags.h"
#include "extensions/common/constants.h"
#include "services/network/public/mojom/permissions_policy/permissions_policy_feature.mojom.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/stl_util.h"
#include "components/permissions/android/android_permission_util.h"
#include "components/permissions/android/permissions_reprompt_controller_android.h"
#include "components/permissions/permission_prompt_decision.h"
#include "components/permissions/permission_request_id.h"
#include "components/permissions/permissions_client.h"
#include "content/public/browser/web_contents.h"
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/guest_view/web_view/web_view_permission_helper.h"
#include "extensions/browser/process_map.h"
#include "extensions/browser/suggest_permission_util.h"
#include "extensions/common/extension.h"
#include "extensions/common/permissions/permissions_data.h"
#endif

namespace {

network::mojom::PermissionsPolicyFeature GetPermissionsPolicyFeature(
    ContentSettingsType type) {
  if (type == ContentSettingsType::MEDIASTREAM_MIC) {
    return network::mojom::PermissionsPolicyFeature::kMicrophone;
  }

  DCHECK_EQ(ContentSettingsType::MEDIASTREAM_CAMERA, type);
  return network::mojom::PermissionsPolicyFeature::kCamera;
}

#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
void CallbackPermissionStatusWrapper(
    base::OnceCallback<void(content::PermissionResult)> callback,
    bool allowed) {
  std::move(callback).Run(content::PermissionResult(
      allowed ? blink::mojom::PermissionStatus::GRANTED
              : blink::mojom::PermissionStatus::DENIED,
      content::PermissionStatusSource::UNSPECIFIED));
}
#endif

}  // namespace

MediaStreamDevicePermissionContext::MediaStreamDevicePermissionContext(
    content::BrowserContext* browser_context,
    const ContentSettingsType content_settings_type)
    : permissions::ContentSettingPermissionContextBase(
          browser_context,
          content_settings_type,
          GetPermissionsPolicyFeature(content_settings_type)),
      content_settings_type_(content_settings_type) {
  DCHECK(content_settings_type_ == ContentSettingsType::MEDIASTREAM_MIC ||
         content_settings_type_ == ContentSettingsType::MEDIASTREAM_CAMERA);
}

MediaStreamDevicePermissionContext::~MediaStreamDevicePermissionContext() =
    default;

ContentSetting
MediaStreamDevicePermissionContext::GetContentSettingStatusInternal(
    content::RenderFrameHost* render_frame_host,
    const GURL& requesting_origin,
    const GURL& embedding_origin) const {
  // TODO(raymes): Merge this policy check into content settings
  // crbug.com/41014586.
  const char* policy_name = nullptr;
  const char* urls_policy_name = nullptr;
  if (content_settings_type_ == ContentSettingsType::MEDIASTREAM_MIC) {
    policy_name = prefs::kAudioCaptureAllowed;
    urls_policy_name = prefs::kAudioCaptureAllowedUrls;
  } else {
    DCHECK(content_settings_type_ == ContentSettingsType::MEDIASTREAM_CAMERA);
    policy_name = prefs::kVideoCaptureAllowed;
    urls_policy_name = prefs::kVideoCaptureAllowedUrls;
  }

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kUseFakeUIForMediaStream)) {
    bool blocked = base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
                       switches::kUseFakeUIForMediaStream) == "deny";
    return blocked ? CONTENT_SETTING_BLOCK : CONTENT_SETTING_ALLOW;
  }

  MediaStreamDevicePolicy policy =
      GetDevicePolicy(Profile::FromBrowserContext(browser_context()),
                      requesting_origin, policy_name, urls_policy_name);

  switch (policy) {
    case ALWAYS_DENY:
      return CONTENT_SETTING_BLOCK;
    case ALWAYS_ALLOW:
      return CONTENT_SETTING_ALLOW;
    default:
      DCHECK_EQ(POLICY_NOT_SET, policy);
  }

  // Check the content setting. TODO(raymes): currently mic/camera permission
  // doesn't consider the embedder.
  ContentSetting setting = permissions::ContentSettingPermissionContextBase::
      GetContentSettingStatusInternal(render_frame_host, requesting_origin,
                                      requesting_origin);

  if (setting == CONTENT_SETTING_DEFAULT) {
    setting = CONTENT_SETTING_ASK;
  }

  return setting;
}

#if BUILDFLAG(IS_ANDROID)
// There are two other permissions that need to check corresponding OS-level
// permissions, and they take two different approaches to this. Geolocation only
// stores the permission ContentSetting if both requests are granted (or if the
// site permission is "Block"). WebXR permissions are following the approach
// found here.
void MediaStreamDevicePermissionContext::NotifyPermissionSet(
    const permissions::PermissionRequestData& request_data,
    permissions::BrowserPermissionCallback callback,
    bool persist,
    const content::PermissionResult* permission_result,
    const permissions::PermissionPromptDecision& decision) {
  DCHECK(decision.is_final);

  // For Android, we need to customize the ContentSettingPermissionContextBase's
  // behavior if the permission was granted. We will:
  // 1. Check if the permission was granted by the user - if not, we'll fall
  //    back to the implementation in the base class.
  // 2. Handle persisting the permission if needed (this is the same as base
  //    class implementation).
  // 3. Check if the OS permissions need to be re-prompted.
  // a) If no, we'll call base class impl. & say that the permission was granted
  //    by the user, but skip persisting it (because we already did in step 2).
  // b) If yes but we cannot show the info bar, we will call base class impl. &
  //    say that the permission was rejected by the user, and we won't persist
  //    it (because we already did in step 2 and the user didn't actually reject
  //    the permission).
  // c) If yes and we can show the info bar, we show it and propagate the answer
  //    to the base class impl., skipping persisting it (because we already
  //    persisted it and the user didn't actually reject it).
  //
  // Note that base class implementation will call into `UpdateTabContext()`
  // virtual method when we invoke `NotifyPermissionSet()` from the base class.
  // This is fine, even in 3b) and 3c), where we call it with a parameter that
  // does not correspond to user's answer to Chrome-level permission, because
  // `MediaStreamDevicePermissionContext` does *not* have a custom
  // implementation for `UpdateTabContext()` - if it did, we'd need to stop
  // calling into base class with the parameter not matching user's answer.

  DCHECK(content_settings_type_ == ContentSettingsType::MEDIASTREAM_CAMERA ||
         content_settings_type_ == ContentSettingsType::MEDIASTREAM_MIC);

  // Camera and Microphone need to check for additional permissions, but only if
  // they were actually allowed:
  if (decision.overall_decision != PermissionDecision::kAllow) {
    ContentSettingPermissionContextBase::NotifyPermissionSet(
        request_data, std::move(callback), persist, permission_result,
        decision);
    return;
  }

  content::PermissionResult new_permission_result =
      permission_result ? *permission_result
                        : ComputeNewPermissionResult(request_data, decision);

  // Whether or not the user will ultimately accept the OS permissions, we want
  // to save the content_setting here if we should. This is done here because we
  // won't set `persist=true` when calling
  // `ContentSettingPermissionContextBase::NotifyPermissionSet()` after this
  // point.
  if (persist) {
    CHECK(new_permission_result.retrieved_permission_setting.has_value());

    UpdateSetting(
        request_data,
        new_permission_result.retrieved_permission_setting.value(),
        decision.overall_decision == PermissionDecision::kAllowThisTime);
  }

  // Must exist since permission requests must be initiated from an RFH
  auto* rfh = content::RenderFrameHost::FromID(
      request_data.id.global_render_frame_host_id());

  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(rfh);

  if (!web_contents) {
    // If we can't get the web contents, we don't know the state of the OS
    // permission, so assume we don't have it.
    OnAndroidPermissionDecided(request_data, new_permission_result,
                               std::move(callback),
                               /*permission_granted=*/false);
    return;
  }

  // Otherwise, the user granted permission to use `content_settings_type_`, so
  // now we need to check if we need to prompt for Android system permissions.
  std::vector<ContentSettingsType> permission_type = {content_settings_type_};

  // For PEPC-initiated permission requests we never need to handle android
  // permissions, so we can shortcut to calling NotifyPermissionSet directly.
  const auto* request = FindPermissionRequest(request_data.id);
  if (request && request->IsEmbeddedPermissionElementInitiated()) {
    ContentSettingPermissionContextBase::NotifyPermissionSet(
        request_data, std::move(callback), persist, &new_permission_result,
        decision);
    return;
  }

  permissions::PermissionRepromptState reprompt_state =
      permissions::ShouldRepromptUserForPermissions(web_contents,
                                                    permission_type);
  switch (reprompt_state) {
    case permissions::PermissionRepromptState::kNoNeed:
      // We would have already returned if permission was denied by the user,
      // and this result indicates that we have all the OS permissions we need.
      OnAndroidPermissionDecided(request_data, new_permission_result,
                                 std::move(callback),
                                 /*permission_granted=*/true);
      return;

    case permissions::PermissionRepromptState::kCannotShow:
      // If we cannot show the info bar, then we have to assume we don't have
      // the permissions we need.
      OnAndroidPermissionDecided(request_data, new_permission_result,
                                 std::move(callback),
                                 /*permission_granted=*/false);
      return;

    case permissions::PermissionRepromptState::kShow:
      // Otherwise, prompt the user that we need additional permissions.
      permissions::PermissionsRepromptControllerAndroid::CreateForWebContents(
          web_contents);
      permissions::PermissionsRepromptControllerAndroid::FromWebContents(
          web_contents)
          ->RepromptPermissionRequest(
              permission_type, content_settings_type_,
              base::BindOnce(&MediaStreamDevicePermissionContext::
                                 OnAndroidPermissionDecided,
                             weak_ptr_factory_.GetWeakPtr(),
                             request_data.Clone(), new_permission_result,
                             std::move(callback)));
      return;
  }
}

void MediaStreamDevicePermissionContext::OnAndroidPermissionDecided(
    const permissions::PermissionRequestData& request_data,
    const content::PermissionResult& website_permission_result,
    permissions::BrowserPermissionCallback callback,
    bool permission_granted) {
  // If we were supposed to persist the setting we've already done so in the
  // initial override of |NotifyPermissionSet|. At this point, if the user
  // has denied the OS level permission, we want to notify the requestor that
  // the permission has been blocked.
  PermissionDecision result_decision = permission_granted
                                           ? PermissionDecision::kAllow
                                           : PermissionDecision::kDeny;
  // `persist=false` because the user's response to Chrome-level permission is
  // already persisted, and `is_one_time=false` because it is only relevant when
  // persisting permission.
  ContentSettingPermissionContextBase::NotifyPermissionSet(
      request_data, std::move(callback), /*persist=*/false,
      // If the OS-level setting was denied, force recomputing the final
      // PermissionResult.
      /*permission_result=*/
      permission_granted ? &website_permission_result : nullptr,
      permissions::PermissionPromptDecision{.overall_decision = result_decision,
                                            .is_final = true});
}

void MediaStreamDevicePermissionContext::UpdateTabContext(
    const permissions::PermissionRequestData& request_data,
    bool allowed) {
  // See the comment in `NotifyPermissionSet()` for context on why this method
  // should be empty.
}
#endif

void MediaStreamDevicePermissionContext::DecidePermission(
    std::unique_ptr<permissions::PermissionRequestData> request_data,
    permissions::BrowserPermissionCallback callback) {
#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
  content::RenderFrameHost* rfh = content::RenderFrameHost::FromID(
      request_data->id.global_render_frame_host_id());
  if (rfh) {
    extensions::WebViewPermissionHelper* web_view_permission_helper =
        extensions::WebViewPermissionHelper::FromRenderFrameHost(rfh);
    if (web_view_permission_helper) {
      // TODO(crbug.com/521370750): This is part of the plumbing to support
      // PEPC inside <webview> guests. Track full support/enablement here.
      web_view_permission_helper->RequestMediaPermission(
          content_settings_type_, request_data->requesting_origin,
          request_data->user_gesture,
          base::BindOnce(&CallbackPermissionStatusWrapper,
                         std::move(callback)));
      return;
    }

    extensions::ExtensionRegistry* extension_registry =
        extensions::ExtensionRegistry::Get(browser_context());
    url::Origin requesting_origin = rfh->GetLastCommittedOrigin();
    const extensions::Extension* extension =
        extension_registry->enabled_extensions().GetExtensionOrAppByURL(
            requesting_origin.GetURL());
    if (extension) {
      extensions::mojom::APIPermissionID permission_id =
          (content_settings_type_ == ContentSettingsType::MEDIASTREAM_MIC)
              ? extensions::mojom::APIPermissionID::kAudioCapture
              : extensions::mojom::APIPermissionID::kVideoCapture;
      bool has_permission = false;
      if (extension->is_platform_app()) {
        has_permission =
            extensions::IsExtensionWithPermissionOrSuggestInConsole(
                permission_id, extension, rfh->GetMainFrame());
      } else {
        has_permission =
            extension->permissions_data()->HasAPIPermission(permission_id);
      }
      if (has_permission) {
        if (extensions::ProcessMap::Get(browser_context())
                ->Contains(
                    extension->id(),
                    request_data->id.global_render_frame_host_id().child_id)) {
          std::move(callback).Run(content::PermissionResult(
              blink::mojom::PermissionStatus::GRANTED,
              content::PermissionStatusSource::UNSPECIFIED));
          return;
        }
      }
    }
  }
#endif  // BUILDFLAG(ENABLE_EXTENSIONS_CORE)
  permissions::ContentSettingPermissionContextBase::DecidePermission(
      std::move(request_data), std::move(callback));
}

void MediaStreamDevicePermissionContext::ResetPermission(
    const GURL& requesting_origin,
    const GURL& embedding_origin) {
  NOTREACHED() << "ResetPermission is not implemented";
}
