// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/guest_view/web_view/chrome_web_view_permission_helper_delegate.h"

#include <map>
#include <utility>

#include "base/check_deref.h"
#include "base/functional/bind.h"
#include "base/metrics/user_metrics.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/webui_url_constants.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/guest_view/browser/guest_view_base.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/permission_controller.h"
#include "content/public/browser/permission_descriptor_util.h"
#include "content/public/browser/permission_result.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/common/buildflags.h"
#include "content/public/common/url_constants.h"
#include "extensions/browser/guest_view/web_view/web_view_constants.h"
#include "extensions/browser/guest_view/web_view/web_view_guest.h"
#include "extensions/browser/guest_view/web_view/web_view_permission_types.h"
#include "services/network/public/cpp/permissions_policy/permissions_policy.h"
#include "services/network/public/mojom/permissions_policy/permissions_policy_feature.mojom-shared.h"
#include "third_party/blink/public/common/permissions/permission_utils.h"
#include "url/origin.h"

#if BUILDFLAG(ENABLE_PLUGINS)
#include "chrome/browser/plugins/chrome_plugin_service_filter.h"
#endif

namespace extensions {

namespace {

void CallbackWrapper(base::OnceCallback<void(bool)> callback,
                     content::PermissionResult permission_result) {
  std::move(callback).Run(permission_result.status ==
                          blink::mojom::PermissionStatus::GRANTED);
}

// Checks the embedder's permissions policy for whether the feature is enabled
// for both the requesting origin and the embedder's origin.
bool IsFeatureEnabledByEmbedderPermissionsPolicy(
    WebViewGuest* web_view_guest,
    network::mojom::PermissionsPolicyFeature feature,
    const url::Origin& requesting_origin) {
  content::RenderFrameHost* embedder_rfh = web_view_guest->embedder_rfh();
  CHECK(embedder_rfh);

  const network::PermissionsPolicy* permissions_policy =
      embedder_rfh->GetPermissionsPolicy();
  CHECK(permissions_policy);
  if (!permissions_policy->IsFeatureEnabledForOrigin(feature,
                                                     requesting_origin)) {
    return false;
  }

  if (!permissions_policy->IsFeatureEnabledForOrigin(
          feature, embedder_rfh->GetLastCommittedOrigin())) {
    return false;
  }
  return true;
}

// Checks whether the embedder frame's origin is allowed the given content
// setting.
bool IsContentSettingAllowedInEmbedder(
    WebViewGuest* web_view_guest,
    ContentSettingsType content_settings_type) {
  GURL embedder_url = CHECK_DEREF(web_view_guest->embedder_rfh())
                          .GetLastCommittedOrigin()
                          .GetURL();

  const HostContentSettingsMap* const content_settings =
      HostContentSettingsMapFactory::GetForProfile(
          web_view_guest->browser_context());
  ContentSetting setting = content_settings->GetContentSetting(
      embedder_url, embedder_url, content_settings_type);
  return setting == CONTENT_SETTING_ALLOW ||
         setting == CONTENT_SETTING_SESSION_ONLY;
}

}  // anonymous namespace

#if BUILDFLAG(ENABLE_PLUGINS)
// static
void ChromeWebViewPermissionHelperDelegate::BindPluginAuthHost(
    mojo::PendingAssociatedReceiver<chrome::mojom::PluginAuthHost> receiver,
    content::RenderFrameHost* rfh) {
  auto* permission_helper =
      extensions::WebViewPermissionHelper::FromRenderFrameHost(rfh);
  if (!permission_helper)
    return;
  WebViewPermissionHelperDelegate* delegate = permission_helper->delegate();
  if (!delegate)
    return;
  auto* chrome_delegate =
      static_cast<ChromeWebViewPermissionHelperDelegate*>(delegate);
  chrome_delegate->plugin_auth_host_receivers_.Bind(rfh, std::move(receiver));
}
#endif

ChromeWebViewPermissionHelperDelegate::ChromeWebViewPermissionHelperDelegate(
    WebViewPermissionHelper* web_view_permission_helper)
    : WebViewPermissionHelperDelegate(web_view_permission_helper)
#if BUILDFLAG(ENABLE_PLUGINS)
      ,
      plugin_auth_host_receivers_(
          web_view_permission_helper->web_view_guest()->web_contents(),
          this)
#endif
{
}

ChromeWebViewPermissionHelperDelegate::
    ~ChromeWebViewPermissionHelperDelegate() = default;

#if BUILDFLAG(ENABLE_PLUGINS)

void ChromeWebViewPermissionHelperDelegate::BlockedUnauthorizedPlugin(
    const std::u16string& name,
    const std::string& identifier) {
  // `loadplugin` permission is not supported in Controlled Frame.
  if (web_view_guest()->IsOwnedByControlledFrameEmbedder()) {
    return;
  }

  const char kPluginName[] = "name";
  const char kPluginIdentifier[] = "identifier";

  base::Value::Dict info;
  info.Set(kPluginName, name);
  info.Set(kPluginIdentifier, identifier);
  web_view_permission_helper()->RequestPermission(
      WEB_VIEW_PERMISSION_TYPE_LOAD_PLUGIN, std::move(info),
      base::BindOnce(
          &ChromeWebViewPermissionHelperDelegate::OnPermissionResponse,
          weak_factory_.GetWeakPtr(), identifier),
      true /* allowed_by_default */);
  base::RecordAction(
      base::UserMetricsAction("WebView.Guest.PluginLoadRequest"));
}

void ChromeWebViewPermissionHelperDelegate::OnPermissionResponse(
    const std::string& identifier,
    bool allow,
    const std::string& input) {
  if (allow) {
    ChromePluginServiceFilter::GetInstance()->AuthorizeAllPlugins(
        web_view_permission_helper()->web_view_guest()->GetGuestMainFrame(),
        true, identifier);
  }
}

#endif  // BUILDFLAG(ENABLE_PLUGINS)

void ChromeWebViewPermissionHelperDelegate::
    RequestMediaAccessPermissionForControlledFrame(
        content::WebContents* source,
        const content::MediaStreamRequest& request,
        content::MediaResponseCallback callback) {
  if (!web_view_guest()->attached()) {
    std::move(callback).Run(
        blink::mojom::StreamDevicesSet(),
        blink::mojom::MediaStreamRequestResult::FAILED_DUE_TO_SHUTDOWN,
        std::unique_ptr<content::MediaStreamUI>());
    return;
  }

  {
    if (request.audio_type !=
            blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE &&
        request.video_type !=
            blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE) {
      std::move(callback).Run(
          blink::mojom::StreamDevicesSet(),
          blink::mojom::MediaStreamRequestResult::PERMISSION_DISMISSED,
          std::unique_ptr<content::MediaStreamUI>());
      return;
    }

    bool audio_denied =
        request.audio_type ==
            blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE &&
        !IsFeatureEnabledByEmbedderPermissionsPolicy(
            web_view_guest(),
            network::mojom::PermissionsPolicyFeature::kMicrophone,
            request.url_origin);

    bool video_denied =
        request.video_type ==
            blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE &&
        !IsFeatureEnabledByEmbedderPermissionsPolicy(
            web_view_guest(), network::mojom::PermissionsPolicyFeature::kCamera,
            request.url_origin);

    if (audio_denied || video_denied) {
      std::move(callback).Run(
          blink::mojom::StreamDevicesSet(),
          blink::mojom::MediaStreamRequestResult::CAPTURE_NOT_ALLOWED_BY_POLICY,
          std::unique_ptr<content::MediaStreamUI>());
      return;
    }
  }

  base::Value::Dict request_info;
  request_info.Set(guest_view::kUrl, request.security_origin.spec());
  web_view_permission_helper()->RequestPermission(
      WEB_VIEW_PERMISSION_TYPE_MEDIA, std::move(request_info),
      base::BindOnce(&ChromeWebViewPermissionHelperDelegate::
                         OnMediaPermissionResponseForControlledFrame,
                     weak_factory_.GetWeakPtr(), source, request,
                     std::move(callback)),
      /*allowed_by_default=*/false);
}

void ChromeWebViewPermissionHelperDelegate::
    OnMediaPermissionResponseForControlledFrame(
        content::WebContents* web_contents,
        const content::MediaStreamRequest& request,
        content::MediaResponseCallback callback,
        bool allow,
        const std::string& user_input) {
  if (!allow) {
    std::move(callback).Run(blink::mojom::StreamDevicesSet(),
                            blink::mojom::MediaStreamRequestResult::
                                PERMISSION_DENIED_BY_EMBEDDER_CONTEXT,
                            std::unique_ptr<content::MediaStreamUI>());
    return;
  }
  MediaCaptureDevicesDispatcher::GetInstance()->ProcessMediaAccessRequest(
      web_contents, request, std::move(callback), /*extension=*/nullptr);
}

bool ChromeWebViewPermissionHelperDelegate::
    CheckMediaAccessPermissionForControlledFrame(
        content::RenderFrameHost* render_frame_host,
        const url::Origin& security_origin,
        blink::mojom::MediaStreamType type) {
  return MediaCaptureDevicesDispatcher::GetInstance()
      ->CheckMediaAccessPermission(render_frame_host, security_origin, type);
}

void ChromeWebViewPermissionHelperDelegate::CanDownload(
    const GURL& url,
    const std::string& request_method,
    base::OnceCallback<void(bool)> callback) {
  base::Value::Dict request_info;
  request_info.Set(guest_view::kUrl, url.spec());
  web_view_permission_helper()->RequestPermission(
      WEB_VIEW_PERMISSION_TYPE_DOWNLOAD, std::move(request_info),
      base::BindOnce(
          &ChromeWebViewPermissionHelperDelegate::OnDownloadPermissionResponse,
          weak_factory_.GetWeakPtr(), std::move(callback)),
      false /* allowed_by_default */);
}

void ChromeWebViewPermissionHelperDelegate::OnDownloadPermissionResponse(
    base::OnceCallback<void(bool)> callback,
    bool allow,
    const std::string& user_input) {
  std::move(callback).Run(allow && web_view_guest()->attached());
}

void ChromeWebViewPermissionHelperDelegate::RequestPointerLockPermission(
    bool user_gesture,
    bool last_unlocked_by_target,
    base::OnceCallback<void(bool)> callback) {
  base::Value::Dict request_info;
  request_info.Set(guest_view::kUserGesture, user_gesture);
  request_info.Set(webview::kLastUnlockedBySelf, last_unlocked_by_target);
  request_info.Set(guest_view::kUrl, web_view_permission_helper()
                                         ->web_view_guest()
                                         ->web_contents()
                                         ->GetLastCommittedURL()
                                         .spec());

  web_view_permission_helper()->RequestPermission(
      WEB_VIEW_PERMISSION_TYPE_POINTER_LOCK, std::move(request_info),
      base::BindOnce(&ChromeWebViewPermissionHelperDelegate::
                         OnPointerLockPermissionResponse,
                     weak_factory_.GetWeakPtr(), std::move(callback)),
      false /* allowed_by_default */);
}

void ChromeWebViewPermissionHelperDelegate::OnPointerLockPermissionResponse(
    base::OnceCallback<void(bool)> callback,
    bool allow,
    const std::string& user_input) {
  if (web_view_guest()->attached() &&
      web_view_guest()->IsOwnedByControlledFrameEmbedder() &&
      !IsContentSettingAllowedInEmbedder(web_view_guest(),
                                         ContentSettingsType::POINTER_LOCK)) {
    std::move(callback).Run(false);
    return;
  }
  std::move(callback).Run(allow && web_view_guest()->attached());
}

void ChromeWebViewPermissionHelperDelegate::RequestGeolocationPermission(
    const GURL& requesting_frame,
    bool user_gesture,
    base::OnceCallback<void(bool)> callback) {
  // Controlled Frame embedders have permissions policy. Permission can
  // only be granted if the embedder's permissions policy allows for both the
  // requesting origin and the embedder origin.
  if (web_view_guest()->attached() &&
      web_view_guest()->IsOwnedByControlledFrameEmbedder() &&
      !IsFeatureEnabledByEmbedderPermissionsPolicy(
          web_view_guest(),
          network::mojom::PermissionsPolicyFeature::kGeolocation,
          url::Origin::Create(requesting_frame))) {
    std::move(callback).Run(false);
    return;
  }

  base::Value::Dict request_info;
  request_info.Set(guest_view::kUrl, requesting_frame.spec());
  request_info.Set(guest_view::kUserGesture, user_gesture);

  WebViewPermissionHelper::PermissionResponseCallback permission_callback =
      base::BindOnce(&ChromeWebViewPermissionHelperDelegate::
                         OnGeolocationPermissionResponse,
                     weak_factory_.GetWeakPtr(), user_gesture,
                     base::BindOnce(&CallbackWrapper, std::move(callback)));
  web_view_permission_helper()->RequestPermission(
      WEB_VIEW_PERMISSION_TYPE_GEOLOCATION, std::move(request_info),
      std::move(permission_callback), false /* allowed_by_default */);
}

void ChromeWebViewPermissionHelperDelegate::OnGeolocationPermissionResponse(
    bool user_gesture,
    base::OnceCallback<void(content::PermissionResult)> callback,
    bool allow,
    const std::string& user_input) {
  if (!allow) {
    std::move(callback).Run(content::PermissionResult(
        blink::mojom::PermissionStatus::DENIED,
        content::PermissionStatusSource::UNSPECIFIED));
    return;
  }

  // The <webview> embedder has responded to the permission request. We now need
  // to make sure that the embedder has geolocation permission.
  RequestEmbedderFramePermission(user_gesture, std::move(callback),
                                 blink::PermissionType::GEOLOCATION);
}

void ChromeWebViewPermissionHelperDelegate::RequestHidPermission(
    const GURL& requesting_frame_url,
    base::OnceCallback<void(bool)> callback) {
  // Controlled Frame embedders have permissions policy. Permission can
  // only be granted if the embedder's permissions policy allows for both the
  // requesting origin and the embedder origin.
  if (web_view_guest()->attached() &&
      web_view_guest()->IsOwnedByControlledFrameEmbedder() &&
      !IsFeatureEnabledByEmbedderPermissionsPolicy(
          web_view_guest(), network::mojom::PermissionsPolicyFeature::kHid,
          url::Origin::Create(requesting_frame_url))) {
    std::move(callback).Run(false);
    return;
  }

  auto request_info =
      base::Value::Dict().Set(guest_view::kUrl, requesting_frame_url.spec());

  WebViewPermissionHelper::PermissionResponseCallback permission_callback =
      base::BindOnce(
          &ChromeWebViewPermissionHelperDelegate::OnHidPermissionResponse,
          weak_factory_.GetWeakPtr(), std::move(callback));
  web_view_permission_helper()->RequestPermission(
      WEB_VIEW_PERMISSION_TYPE_HID, std::move(request_info),
      std::move(permission_callback), false /* allowed_by_default */);
}

void ChromeWebViewPermissionHelperDelegate::OnHidPermissionResponse(
    base::OnceCallback<void(bool)> callback,
    bool allow,
    const std::string& user_input) {

  std::move(callback).Run(allow && web_view_guest()->attached());
}

void ChromeWebViewPermissionHelperDelegate::RequestFileSystemPermission(
    const GURL& url,
    bool allowed_by_default,
    base::OnceCallback<void(bool)> callback) {
  base::Value::Dict request_info;
  request_info.Set(guest_view::kUrl, url.spec());
  web_view_permission_helper()->RequestPermission(
      WEB_VIEW_PERMISSION_TYPE_FILESYSTEM, std::move(request_info),
      base::BindOnce(&ChromeWebViewPermissionHelperDelegate::
                         OnFileSystemPermissionResponse,
                     weak_factory_.GetWeakPtr(), std::move(callback)),
      allowed_by_default);
}

void ChromeWebViewPermissionHelperDelegate::OnFileSystemPermissionResponse(
    base::OnceCallback<void(bool)> callback,
    bool allow,
    const std::string& user_input) {
  std::move(callback).Run(allow && web_view_guest()->attached());
}

void ChromeWebViewPermissionHelperDelegate::RequestFullscreenPermission(
    const url::Origin& requesting_origin,
    WebViewPermissionHelper::PermissionResponseCallback callback) {
  if (web_view_guest()->attached() &&
      web_view_guest()->IsOwnedByControlledFrameEmbedder() &&
      !IsFeatureEnabledByEmbedderPermissionsPolicy(
          web_view_guest(),
          network::mojom::PermissionsPolicyFeature::kFullscreen,
          requesting_origin)) {
    std::move(callback).Run(/*allow=*/false, /*user_input=*/"");
    return;
  }

  base::Value::Dict request_info;
  request_info.Set(webview::kOrigin, requesting_origin.GetURL().spec());
  web_view_permission_helper()->RequestPermission(
      WEB_VIEW_PERMISSION_TYPE_FULLSCREEN, std::move(request_info),
      std::move(callback), /*allowed_by_default=*/false);
}

void ChromeWebViewPermissionHelperDelegate::RequestClipboardReadWritePermission(
    const GURL& requesting_frame_url,
    bool user_gesture,
    base::OnceCallback<void(bool)> callback) {
  // Supported only if all cases true:
  // 1. Owned by controlled Frame.
  // 2. Permissions policy is present for embedder and requesting origin.
  if (!web_view_guest()->IsOwnedByControlledFrameEmbedder() ||
      (web_view_guest()->attached() &&
       !IsFeatureEnabledByEmbedderPermissionsPolicy(
           web_view_guest(),
           network::mojom::PermissionsPolicyFeature::kClipboardRead,
           url::Origin::Create(requesting_frame_url)))) {
    std::move(callback).Run(false);
    return;
  }

  auto request_info = base::Value::Dict()
                          .Set(guest_view::kUrl, requesting_frame_url.spec())
                          .Set(guest_view::kUserGesture, user_gesture);

  WebViewPermissionHelper::PermissionResponseCallback permission_callback =
      base::BindOnce(&ChromeWebViewPermissionHelperDelegate::
                         OnClipboardReadWritePermissionResponse,
                     weak_factory_.GetWeakPtr(),
                     base::BindOnce(&CallbackWrapper, std::move(callback)),
                     user_gesture);
  web_view_permission_helper()->RequestPermission(
      WEB_VIEW_PERMISSION_TYPE_CLIPBOARD_READ_WRITE, std::move(request_info),
      std::move(permission_callback), false /* allowed_by_default */);
}

void ChromeWebViewPermissionHelperDelegate::
    OnClipboardReadWritePermissionResponse(
        base::OnceCallback<void(content::PermissionResult)> callback,
        bool user_gesture,
        bool allow,
        const std::string& user_input) {
  if (!allow) {
    std::move(callback).Run(content::PermissionResult(
        blink::mojom::PermissionStatus::DENIED,
        content::PermissionStatusSource::UNSPECIFIED));
    return;
  }

  // The <webview> embedder has responded to the permission request. We now need
  // to make sure that the embedder has the permission.
  RequestEmbedderFramePermission(user_gesture, std::move(callback),
                                 blink::PermissionType::CLIPBOARD_READ_WRITE);
}

void ChromeWebViewPermissionHelperDelegate::
    RequestClipboardSanitizedWritePermission(
        const GURL& requesting_frame_url,
        base::OnceCallback<void(bool)> callback) {
  // Supported only if all cases true:
  // 1. Owned by controlled Frame.
  // 2. Permissions policy is present for embedder and requesting origin.
  if (!web_view_guest()->IsOwnedByControlledFrameEmbedder() ||
      (web_view_guest()->attached() &&
       !IsFeatureEnabledByEmbedderPermissionsPolicy(
           web_view_guest(),
           network::mojom::PermissionsPolicyFeature::kClipboardWrite,
           url::Origin::Create(requesting_frame_url)))) {
    std::move(callback).Run(false);
    return;
  }

  // This permission request always has user_gesture=true.
  // That's why we don't add respective entry to the dict.
  auto request_info =
      base::Value::Dict().Set(guest_view::kUrl, requesting_frame_url.spec());

  WebViewPermissionHelper::PermissionResponseCallback permission_callback =
      base::BindOnce(&ChromeWebViewPermissionHelperDelegate::
                         OnClipboardSanitizedWritePermissionResponse,
                     weak_factory_.GetWeakPtr(),
                     base::BindOnce(&CallbackWrapper, std::move(callback)));

  web_view_permission_helper()->RequestPermission(
      WEB_VIEW_PERMISSION_TYPE_CLIPBOARD_SANITIZED_WRITE,
      std::move(request_info), std::move(permission_callback),
      false /* allowed_by_default */);
}

void ChromeWebViewPermissionHelperDelegate::
    OnClipboardSanitizedWritePermissionResponse(
        base::OnceCallback<void(content::PermissionResult)> callback,
        bool allow,
        const std::string& user_input) {
  if (!allow) {
    std::move(callback).Run(content::PermissionResult(
        blink::mojom::PermissionStatus::DENIED,
        content::PermissionStatusSource::UNSPECIFIED));
    return;
  }

  // The <webview> embedder has responded to the permission request. We now need
  // to make sure that the embedder has the permission.
  RequestEmbedderFramePermission(
      /*user_gesture=*/true, std::move(callback),
      blink::PermissionType::CLIPBOARD_SANITIZED_WRITE);
}

void ChromeWebViewPermissionHelperDelegate::RequestEmbedderFramePermission(
    bool user_gesture,
    base::OnceCallback<void(content::PermissionResult)> callback,
    blink::PermissionType permission_type) {
  if (!web_view_guest()->attached()) {
    std::move(callback).Run(content::PermissionResult(
        blink::mojom::PermissionStatus::DENIED,
        content::PermissionStatusSource::UNSPECIFIED));
    return;
  }

  web_view_guest()
      ->browser_context()
      ->GetPermissionController()
      ->RequestPermissionFromCurrentDocument(
          web_view_guest()->embedder_rfh(),
          content::PermissionRequestDescription(
              content::PermissionDescriptorUtil::
                  CreatePermissionDescriptorForPermissionType(permission_type),
              user_gesture),
          std::move(callback));
}

bool ChromeWebViewPermissionHelperDelegate::
    ForwardEmbeddedMediaPermissionChecksAsEmbedder(
        const url::Origin& embedder_origin) {
  // Note that it probably makes sense for most WebUIs to forward approved media
  // permissions as the embedder, but historically, the default has been to
  // forward as the embedded origin.
  // TODO(crbug.com/382773576): Change to true for all chrome:// origins, with
  // the exception of chrome://chrome-signin and chrome://oobe, which needs the
  // old behavior.
  return embedder_origin.scheme() == content::kChromeUIScheme &&
         embedder_origin.host() == chrome::kChromeUIGlicHost;
}

std::optional<content::PermissionResult>
ChromeWebViewPermissionHelperDelegate::OverridePermissionResult(
    ContentSettingsType type) {
  const url::Origin& origin =
      web_view_guest()->owner_rfh()->GetLastCommittedOrigin();
  // chrome://glic requires additional permissions, and webview's
  // permissionrequest API does not handle clipboard access or screen wake lock.
  if (origin.scheme() == content::kChromeUIScheme &&
      origin.host() == chrome::kChromeUIGlicHost) {
    switch (type) {
      case ContentSettingsType::CLIPBOARD_READ_WRITE:
      case ContentSettingsType::CLIPBOARD_SANITIZED_WRITE:
      case ContentSettingsType::WAKE_LOCK_SCREEN:
        return content::PermissionResult(
            content::PermissionStatus::GRANTED,
            content::PermissionStatusSource::UNSPECIFIED);
      default:
        break;
    }
  }
  return std::nullopt;
}

}  // namespace extensions
