// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/content_settings/content_settings_manager_impl.h"

#include "base/memory/ptr_util.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/content_settings/tab_specific_content_settings.h"
#include "chrome/browser/profiles/profile.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/page_load_metrics/browser/metrics_web_contents_observer.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "extensions/buildflags/buildflags.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "extensions/browser/guest_view/web_view/web_view_permission_helper.h"
#include "extensions/browser/guest_view/web_view/web_view_renderer_state.h"
#endif

namespace chrome {
namespace {

void OnDomStorageAccessed(int process_id,
                          int frame_id,
                          const GURL& origin_url,
                          const GURL& top_origin_url,
                          bool local,
                          bool blocked_by_policy) {
  content::RenderFrameHost* frame =
      content::RenderFrameHost::FromID(process_id, frame_id);
  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(frame);
  if (!web_contents)
    return;

  TabSpecificContentSettings* tab_settings =
      TabSpecificContentSettings::FromWebContents(web_contents);
  if (tab_settings)
    tab_settings->OnDomStorageAccessed(origin_url, local, blocked_by_policy);

  page_load_metrics::MetricsWebContentsObserver* metrics_observer =
      page_load_metrics::MetricsWebContentsObserver::FromWebContents(
          web_contents);
  if (metrics_observer)
    metrics_observer->OnDomStorageAccessed(origin_url, top_origin_url, local,
                                           blocked_by_policy);
}

#if BUILDFLAG(ENABLE_EXTENSIONS)
void OnFileSystemAccessedInGuestViewContinuation(
    int render_process_id,
    int render_frame_id,
    const GURL& url,
    base::OnceCallback<void(bool)> callback,
    bool allowed) {
  TabSpecificContentSettings::FileSystemAccessed(
      render_process_id, render_frame_id, url, !allowed);
  std::move(callback).Run(allowed);
}

void OnFileSystemAccessedInGuestView(int render_process_id,
                                     int render_frame_id,
                                     const GURL& url,
                                     bool allowed,
                                     base::OnceCallback<void(bool)> callback) {
  extensions::WebViewPermissionHelper* web_view_permission_helper =
      extensions::WebViewPermissionHelper::FromFrameID(render_process_id,
                                                       render_frame_id);
  auto continuation = base::BindOnce(
      &OnFileSystemAccessedInGuestViewContinuation, render_process_id,
      render_frame_id, url, std::move(callback));
  if (!web_view_permission_helper) {
    std::move(continuation).Run(allowed);
    return;
  }
  web_view_permission_helper->RequestFileSystemPermission(
      url, allowed, std::move(continuation));
}
#endif

}  // namespace

ContentSettingsManagerImpl::~ContentSettingsManagerImpl() = default;

// static
void ContentSettingsManagerImpl::Create(
    content::RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<mojom::ContentSettingsManager> receiver) {
  mojo::MakeSelfOwnedReceiver(
      base::WrapUnique(new ContentSettingsManagerImpl(render_frame_host)),
      std::move(receiver));
}

void ContentSettingsManagerImpl::Clone(
    mojo::PendingReceiver<mojom::ContentSettingsManager> receiver) {
  mojo::MakeSelfOwnedReceiver(
      base::WrapUnique(new ContentSettingsManagerImpl(*this)),
      std::move(receiver));
}

void ContentSettingsManagerImpl::AllowStorageAccess(
    StorageType storage_type,
    const url::Origin& origin,
    const GURL& site_for_cookies,
    const url::Origin& top_frame_origin,
    base::OnceCallback<void(bool)> callback) {
  GURL url = origin.GetURL();

  bool allowed = cookie_settings_->IsCookieAccessAllowed(url, site_for_cookies,
                                                         top_frame_origin);

  switch (storage_type) {
    case StorageType::DATABASE:
      TabSpecificContentSettings::WebDatabaseAccessed(
          render_process_id_, render_frame_id_, url, !allowed);
      break;
    case StorageType::LOCAL_STORAGE:
      OnDomStorageAccessed(render_process_id_, render_frame_id_, url,
                           top_frame_origin.GetURL(), true, !allowed);
      break;
    case StorageType::SESSION_STORAGE:
      OnDomStorageAccessed(render_process_id_, render_frame_id_, url,
                           top_frame_origin.GetURL(), false, !allowed);
      break;
    case StorageType::FILE_SYSTEM:
#if BUILDFLAG(ENABLE_EXTENSIONS)
      if (extensions::WebViewRendererState::GetInstance()->IsGuest(
              render_process_id_)) {
        OnFileSystemAccessedInGuestView(render_process_id_, render_frame_id_,
                                        url, allowed, std::move(callback));
        return;
      }
#endif
      TabSpecificContentSettings::FileSystemAccessed(
          render_process_id_, render_frame_id_, url, !allowed);
      break;
    case StorageType::INDEXED_DB:
      TabSpecificContentSettings::IndexedDBAccessed(
          render_process_id_, render_frame_id_, url, !allowed);
      break;
    case StorageType::CACHE:
      TabSpecificContentSettings::CacheStorageAccessed(
          render_process_id_, render_frame_id_, url, !allowed);
      break;
    case StorageType::WEB_LOCKS:
      // State not persisted, no need to record anything.
      break;
  }

  std::move(callback).Run(allowed);
}

void ContentSettingsManagerImpl::OnContentBlocked(ContentSettingsType type) {
  content::RenderFrameHost* frame =
      content::RenderFrameHost::FromID(render_process_id_, render_frame_id_);
  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(frame);
  if (!web_contents)
    return;

  TabSpecificContentSettings* tab_settings =
      TabSpecificContentSettings::FromWebContents(web_contents);
  if (tab_settings)
    tab_settings->OnContentBlocked(type);
}

ContentSettingsManagerImpl::ContentSettingsManagerImpl(
    content::RenderFrameHost* render_frame_host)
    : render_process_id_(render_frame_host->GetProcess()->GetID()),
      render_frame_id_(render_frame_host->GetRoutingID()),
      cookie_settings_(
          CookieSettingsFactory::GetForProfile(Profile::FromBrowserContext(
              render_frame_host->GetProcess()->GetBrowserContext()))) {}

ContentSettingsManagerImpl::ContentSettingsManagerImpl(
    const ContentSettingsManagerImpl& other)
    : render_process_id_(other.render_process_id_),
      render_frame_id_(other.render_frame_id_),
      cookie_settings_(other.cookie_settings_) {}

}  // namespace chrome
