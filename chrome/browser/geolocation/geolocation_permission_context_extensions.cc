// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/geolocation/geolocation_permission_context_extensions.h"

#include <variant>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "components/content_settings/core/common/features.h"
#include "components/permissions/permission_decision.h"
#include "components/permissions/permission_prompt_decision.h"
#include "components/permissions/resolvers/permission_prompt_options.h"
#include "content/public/browser/permission_result.h"
#include "extensions/buildflags/buildflags.h"

#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
#include "chrome/browser/profiles/profile.h"
#include "components/permissions/permission_request_id.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/guest_view/web_view/web_view_permission_helper.h"
#include "extensions/browser/process_map.h"
#include "extensions/browser/suggest_permission_util.h"
#include "extensions/browser/view_type_utils.h"
#include "extensions/common/extension.h"
#include "extensions/common/mojom/view_type.mojom.h"

using extensions::APIPermission;
using extensions::ExtensionRegistry;
#endif

namespace {

#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
void CallbackPermissionStatusWrapper(
    base::OnceCallback<void(content::PermissionResult)> callback,
    bool allowed) {
  std::move(callback).Run(content::PermissionResult(
      allowed ? PermissionStatus::GRANTED : PermissionStatus::DENIED,
      content::PermissionStatusSource::UNSPECIFIED));
}
#endif  // BUILDFLAG(ENABLE_EXTENSIONS_CORE)

}  // anonymous namespace

GeolocationPermissionContextExtensions::GeolocationPermissionContextExtensions(
    Profile* profile)
#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
    : profile_(profile)
#endif  // BUILDFLAG(ENABLE_EXTENSIONS_CORE)
{
}

GeolocationPermissionContextExtensions::
    ~GeolocationPermissionContextExtensions() = default;

std::optional<GeolocationPermissionContextExtensions::Decision>
GeolocationPermissionContextExtensions::DecidePermission(
    const permissions::PermissionRequestID& request_id,
    const GURL& requesting_frame,
    bool user_gesture,
    base::OnceCallback<void(content::PermissionResult)>* callback) {
#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)

  content::RenderFrameHost* rfh = content::RenderFrameHost::FromID(
      request_id.global_render_frame_host_id());

  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(rfh);

  GURL requesting_frame_origin = requesting_frame.DeprecatedGetOriginAsURL();

  extensions::WebViewPermissionHelper* web_view_permission_helper =
      extensions::WebViewPermissionHelper::FromRenderFrameHost(rfh);
  if (web_view_permission_helper) {
    web_view_permission_helper->RequestGeolocationPermission(
        requesting_frame, user_gesture,
        base::BindOnce(&CallbackPermissionStatusWrapper, std::move(*callback)));
    return Decision{.permission_set = false};
  }

  ExtensionRegistry* extension_registry = ExtensionRegistry::Get(profile_);
  if (extension_registry) {
    const extensions::Extension* extension =
        extension_registry->enabled_extensions().GetExtensionOrAppByURL(
            requesting_frame_origin);
    if (IsExtensionWithPermissionOrSuggestInConsole(
            extensions::mojom::APIPermissionID::kGeolocation, extension,
            web_contents->GetPrimaryMainFrame())) {
      // Make sure the extension is in the calling process.
      // TODO(crbug.com/379869738) Remove GetUnsafeValue.
      if (extensions::ProcessMap::Get(profile_)->Contains(
              extension->id(), request_id.global_render_frame_host_id()
                                   .child_id.GetUnsafeValue())) {
        return Decision{
            .permission_set = true,
            .decision = permissions::PermissionPromptDecision{
                .overall_decision = PermissionDecision::kAllow,
                // TODO(https://crbug.com/475096920): For now, extensions are
                // only granted precise location. Potentially implement support
                // for a granular approximate geolocation permission for
                // extensions in the future.
                .prompt_options = base::FeatureList::IsEnabled(
                                      content_settings::features::
                                          kApproximateGeolocationPermission)
                                      ? PromptOptions(GeolocationPromptOptions{
                                            .selected_accuracy =
                                                GeolocationAccuracy::kPrecise})
                                      : std::monostate(),
                .is_final = true}};
      }
    }
  }

  extensions::mojom::ViewType view_type = extensions::GetViewType(web_contents);
  if (view_type != extensions::mojom::ViewType::kTabContents &&
      view_type != extensions::mojom::ViewType::kInvalid) {
    // The tab may have gone away, or the request may not be from a tab at all.
    // TODO(mpcomplete): the request could be from a background page or
    // extension popup (web_contents will have a different ViewType). But why do
    // we care? Shouldn't we still put an infobar up in the current tab?
    LOG(WARNING) << "Attempt to use geolocation tabless renderer: "
                 << request_id.ToString()
                 << " (can't prompt user without a visible tab)";
    return Decision{.permission_set = true,
                    .decision = permissions::PermissionPromptDecision{
                        .overall_decision = PermissionDecision::kDeny,
                        .prompt_options = std::monostate(),
                        .is_final = true}};
  }
#endif  // BUILDFLAG(ENABLE_EXTENSIONS_CORE)
  return std::nullopt;
}
