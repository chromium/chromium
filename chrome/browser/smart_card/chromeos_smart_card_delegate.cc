// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/smart_card/chromeos_smart_card_delegate.h"

#include "base/check.h"
#include "base/check_deref.h"
#include "base/time/time.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/smart_card/get_smart_card_context_factory.h"
#include "chrome/browser/smart_card/smart_card_permission_context.h"
#include "chrome/browser/smart_card/smart_card_permission_context_factory.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "components/content_settings/browser/page_specific_content_settings.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/common/features.h"

namespace {

// kSmartCardAllowedReconnectTime is amount of time after which a new connection
// to a smart card reader stops being deemed a reconnection (and isn't possible
// from the background).
BASE_FEATURE_PARAM(base::TimeDelta,
                   kSmartCardAllowedReconnectTime,
                   &blink::features::kSmartCard,
                   "allowed_reconnect_time",
                   base::Seconds(15));

// The check whether the window has focus is enough here, as this is for IWAs
// only, which appear in standalone windows dedicated to only them.
bool WindowHasFocus(content::RenderFrameHost& render_frame_host) {
  content::WebContents* tab_web_contents =
      content::WebContents::FromRenderFrameHost(&render_frame_host)
          ->GetOutermostWebContents();
  CHECK(tab_web_contents);
  // For the case when this is called in permission revoke handler called after
  // closing the last windows of the origin (and thus expiring the one-time
  // permission). Calling `tabs::TabInterface::GetFromContents` when the tab is
  // closing will crash.
  if (tab_web_contents->IsBeingDestroyed()) {
    return false;
  }

  return tabs::TabInterface::GetFromContents(tab_web_contents)
      ->GetBrowserWindowInterface()
      ->IsActive();
}

bool RecentlyHadConnection(content::RenderFrameHost& render_frame_host) {
  auto& pscs =
      CHECK_DEREF(content_settings::PageSpecificContentSettings::GetForFrame(
          &render_frame_host));
  return pscs.GetLastUsedTime(ContentSettingsType::SMART_CARD_GUARD) >=
             base::Time::Now() - kSmartCardAllowedReconnectTime.Get() ||
         pscs.IsInUse(ContentSettingsType::SMART_CARD_GUARD);
}

bool HasFocusOrRecentlyHadConnection(
    content::RenderFrameHost& render_frame_host) {
  return WindowHasFocus(render_frame_host) ||
         RecentlyHadConnection(render_frame_host);
}

}  // namespace

ChromeOsSmartCardDelegate::ChromeOsSmartCardDelegate() = default;
ChromeOsSmartCardDelegate::~ChromeOsSmartCardDelegate() = default;

mojo::PendingRemote<device::mojom::SmartCardContextFactory>
ChromeOsSmartCardDelegate::GetSmartCardContextFactory(
    content::BrowserContext& browser_context) {
  return ::GetSmartCardContextFactory(browser_context);
}

bool ChromeOsSmartCardDelegate::IsPermissionBlocked(
    content::RenderFrameHost& render_frame_host) {
  auto& profile = CHECK_DEREF(
      Profile::FromBrowserContext(render_frame_host.GetBrowserContext()));

  auto& permission_context =
      SmartCardPermissionContextFactory::GetForProfile(profile);

  const url::Origin& origin =
      render_frame_host.GetMainFrame()->GetLastCommittedOrigin();

  return !permission_context.CanRequestObjectPermission(origin) &&
         !permission_context.IsAllowlistedByPolicy(origin);
}

bool ChromeOsSmartCardDelegate::HasReaderPermission(
    content::RenderFrameHost& render_frame_host,
    const std::string& reader_name) {
  auto& profile = CHECK_DEREF(
      Profile::FromBrowserContext(render_frame_host.GetBrowserContext()));

  auto& permission_context =
      SmartCardPermissionContextFactory::GetForProfile(profile);

  return HasFocusOrRecentlyHadConnection(render_frame_host) &&
         permission_context.HasReaderPermission(render_frame_host, reader_name);
}

void ChromeOsSmartCardDelegate::RequestReaderPermission(
    content::RenderFrameHost& render_frame_host,
    const std::string& reader_name,
    RequestReaderPermissionCallback callback) {
  // Never allow requesting permissions from the background.
  if (!WindowHasFocus(render_frame_host)) {
    std::move(callback).Run(false);
    return;
  }

  auto& profile = CHECK_DEREF(
      Profile::FromBrowserContext(render_frame_host.GetBrowserContext()));

  auto& permission_context =
      SmartCardPermissionContextFactory::GetForProfile(profile);

  permission_context.RequestReaderPermisssion(render_frame_host, reader_name,
                                              std::move(callback));
}

void ChromeOsSmartCardDelegate::NotifyConnectionUsed(
    content::RenderFrameHost& render_frame_host) {
  CHECK_DEREF(content_settings::PageSpecificContentSettings::GetForFrame(
                  &render_frame_host))
      .OnDeviceUsed(
          content_settings::mojom::ContentSettingsType::SMART_CARD_GUARD);
}

void ChromeOsSmartCardDelegate::NotifyLastConnectionLost(
    content::RenderFrameHost& render_frame_host) {
  CHECK_DEREF(content_settings::PageSpecificContentSettings::GetForFrame(
                  &render_frame_host))
      .OnLastDeviceConnectionLost(
          content_settings::mojom::ContentSettingsType::SMART_CARD_GUARD);
}

void ChromeOsSmartCardDelegate::AddObserver(
    content::RenderFrameHost& render_frame_host,
    PermissionObserver* observer) {
  SmartCardPermissionContextFactory::GetForProfile(
      CHECK_DEREF(
          Profile::FromBrowserContext(render_frame_host.GetBrowserContext())))
      .AddObserver(observer);
}

void ChromeOsSmartCardDelegate::RemoveObserver(
    content::RenderFrameHost& render_frame_host,
    PermissionObserver* observer) {
  SmartCardPermissionContextFactory::GetForProfile(
      CHECK_DEREF(
          Profile::FromBrowserContext(render_frame_host.GetBrowserContext())))
      .RemoveObserver(observer);
}
