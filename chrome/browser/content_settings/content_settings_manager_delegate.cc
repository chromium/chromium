// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/content_settings/content_settings_manager_delegate.h"

#include "base/feature_list.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/content_settings/browser/page_specific_content_settings.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/content_features.h"
#include "extensions/buildflags/buildflags.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "extensions/browser/guest_view/web_view/web_view_permission_helper.h"
#include "extensions/browser/guest_view/web_view/web_view_renderer_state.h"
#endif

ContentSettingsManagerDelegate::ContentSettingsManagerDelegate() = default;

ContentSettingsManagerDelegate::~ContentSettingsManagerDelegate() = default;

scoped_refptr<content_settings::CookieSettings>
ContentSettingsManagerDelegate::GetCookieSettings(
    content::BrowserContext* browser_context) {
  return CookieSettingsFactory::GetForProfile(
      Profile::FromBrowserContext(browser_context));
}

bool ContentSettingsManagerDelegate::AllowStorageAccess(
    content::RenderFrameHost* render_frame_host,
    content_settings::mojom::ContentSettingsManager::StorageType storage_type,
    const GURL& url,
    bool allowed,
    base::OnceCallback<void(bool)> callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
#if BUILDFLAG(ENABLE_EXTENSIONS)
  if (storage_type == content_settings::mojom::ContentSettingsManager::
                          StorageType::FILE_SYSTEM &&
      extensions::WebViewRendererState::GetInstance()->IsGuest(
          render_frame_host->GetProcess()->GetDeprecatedID())) {
    extensions::WebViewPermissionHelper* web_view_permission_helper =
        extensions::WebViewPermissionHelper::FromRenderFrameHost(
            render_frame_host);
    auto continuation =
        base::BindOnce(
            [](const content::GlobalRenderFrameHostToken& frame_token,
               const blink::StorageKey& storage_key, bool allowed) {
              content_settings::PageSpecificContentSettings::StorageAccessed(
                  content_settings::mojom::ContentSettingsManager::StorageType::
                      FILE_SYSTEM,
                  frame_token, storage_key, !allowed);
              return allowed;
            },
            render_frame_host->GetGlobalFrameToken(),
            render_frame_host->GetStorageKey())
            .Then(std::move(callback));
    if (!web_view_permission_helper) {
      std::move(continuation).Run(allowed);
    } else {
      web_view_permission_helper->RequestFileSystemPermission(
          url, allowed, std::move(continuation));
    }
    return true;
  }
#endif
  return false;
}

std::unique_ptr<content_settings::ContentSettingsManagerImpl::Delegate>
ContentSettingsManagerDelegate::Clone() {
  return std::make_unique<ContentSettingsManagerDelegate>();
}
