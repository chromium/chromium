// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/chrome_extension_host_delegate.h"

#include <memory>
#include <string>

#include "base/functional/bind.h"
#include "chrome/browser/extensions/browser_window_util.h"
#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"
#include "chrome/browser/picture_in_picture/picture_in_picture_window_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/browser_window/public/create_browser_window.h"
#include "chrome/browser/ui/prefs/prefs_tab_helper.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/buildflags/buildflags.h"
#include "ui/base/base_window.h"
#include "ui/base/page_transition_types.h"

#if BUILDFLAG(ENABLE_PLATFORM_APPS)
#include "chrome/browser/apps/platform_apps/audio_focus_web_contents_observer.h"
#endif

// This file contains code shared between Android and non-Android platforms.
static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

namespace extensions {

ChromeExtensionHostDelegate::ChromeExtensionHostDelegate() = default;

ChromeExtensionHostDelegate::~ChromeExtensionHostDelegate() = default;

void ChromeExtensionHostDelegate::OnExtensionHostCreated(
    content::WebContents* web_contents) {
  PrefsTabHelper::CreateForWebContents(web_contents);
#if BUILDFLAG(ENABLE_PLATFORM_APPS)
  apps::AudioFocusWebContentsObserver::CreateForWebContents(web_contents);
#endif
}

void ChromeExtensionHostDelegate::CreateTab(
    std::unique_ptr<content::WebContents> web_contents,
    const GURL& target_url,
    const ExtensionId& extension_id,
    WindowOpenDisposition disposition,
    const blink::mojom::WindowFeatures& window_features,
    bool user_gesture) {
  // Verify that the browser is not shutting down. It can be the case if the
  // call is propagated through a posted task that was already in the queue when
  // shutdown started. See crbug.com/40475418
  if (ExtensionsBrowserClient::Get()->IsShuttingDown()) {
    return;
  }

  CHECK(web_contents);
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  CHECK(profile);
  BrowserWindowInterface* browser =
      browser_window_util::GetLastActiveNormalBrowserWithProfile(
          *profile, /*include_incognito_or_parent=*/false);

  // If we have an existing browser, navigate it.
  if (browser) {
    NavigateBrowser(/*browser_created=*/false, std::move(web_contents),
                    target_url, extension_id, disposition, window_features,
                    user_gesture, browser);
    return;
  }

  // Otherwise we need to create a browser.
  if (GetBrowserWindowCreationStatusForProfile(*profile) !=
      BrowserWindowInterface::CreationStatus::kOk) {
    // No active browser and can't create a new one. Bail.
    return;
  }

  BrowserWindowCreateParams params(BrowserWindowInterface::TYPE_NORMAL,
                                   *profile, user_gesture);
#if BUILDFLAG(IS_ANDROID)
  // Android creates windows asynchronously.
  auto callback = base::BindOnce(
      &ChromeExtensionHostDelegate::NavigateBrowser, weak_factory_.GetWeakPtr(),
      /*browser_created=*/true, std::move(web_contents), target_url,
      extension_id, disposition, window_features, user_gesture);
  CreateBrowserWindow(std::move(params), std::move(callback));
  return;
#else
  // Other platforms create windows synchronously.
  browser = CreateBrowserWindow(std::move(params));
  CHECK(browser);
  NavigateBrowser(/*browser_created=*/true, std::move(web_contents), target_url,
                  extension_id, disposition, window_features, user_gesture,
                  browser);
#endif  // BUILDFLAG(IS_ANDROID)
}

void ChromeExtensionHostDelegate::NavigateBrowser(
    bool browser_created,
    std::unique_ptr<content::WebContents> web_contents,
    GURL target_url,
    ExtensionId extension_id,
    WindowOpenDisposition disposition,
    blink::mojom::WindowFeatures window_features,
    bool user_gesture,
    BrowserWindowInterface* browser) {
  CHECK(browser);
#if BUILDFLAG(IS_ANDROID)
  // Android does not support "navigating" to an existing web contents. Navigate
  // by URL instead. Use transition "link" to match Win/Mac/Linux behavior.
  // NOTE: This effectively reloads the URL, which is wrong. Unfortunately
  // this is the best we can do until NavigateParams::contents_to_insert is
  // supported. See browser_navigator_android.cc or http://crbug.com/441594986.
  NavigateParams params(browser, nullptr);
  if (disposition == WindowOpenDisposition::NEW_FOREGROUND_TAB ||
      disposition == WindowOpenDisposition::NEW_BACKGROUND_TAB) {
    params.contents_to_insert = std::move(web_contents);
  } else {
    params.url = std::move(target_url);
    params.transition = ui::PAGE_TRANSITION_LINK;
  }
#else
  NavigateParams params(browser, std::move(web_contents));
#endif
  // The extension_app_id parameter ends up as app_name in the Browser
  // which causes the Browser to return true for is_app().  This affects
  // among other things, whether the location bar gets displayed.
  // TODO(mpcomplete): This seems wrong. What if the extension content is
  // hosted in a tab?
  if (disposition == WindowOpenDisposition::NEW_POPUP) {
    params.app_id = extension_id;
  }

  params.disposition = disposition;
  params.window_features = window_features;
  params.window_action = NavigateParams::WindowAction::kShowWindow;
  params.user_gesture = user_gesture;
#if BUILDFLAG(IS_ANDROID)
  // Android uses asynchronous navigate in case it creates a new window.
  // Asynchronous is OK because neither CreateTab() nor NavigateBrowser() need
  // to synchronously return a value.
  Navigate(&params, base::DoNothing());
#else
  // Other platforms use synchronous navigate.
  Navigate(&params);
#endif

  // Close the browser if Navigate created a new one.
  if (browser_created && browser != params.browser) {
    browser->GetWindow()->Close();
  }
}

void ChromeExtensionHostDelegate::ProcessMediaAccessRequest(
    content::WebContents* web_contents,
    const content::MediaStreamRequest& request,
    content::MediaResponseCallback callback,
    const Extension* extension) {
  MediaCaptureDevicesDispatcher::GetInstance()->ProcessMediaAccessRequest(
      web_contents, request, std::move(callback), extension);
}

bool ChromeExtensionHostDelegate::CheckMediaAccessPermission(
    content::RenderFrameHost* render_frame_host,
    const url::Origin& security_origin,
    blink::mojom::MediaStreamType type,
    const Extension* extension) {
  return MediaCaptureDevicesDispatcher::GetInstance()
      ->CheckMediaAccessPermission(render_frame_host, security_origin, type,
                                   extension);
}

content::PictureInPictureResult
ChromeExtensionHostDelegate::EnterPictureInPicture(
    content::WebContents* web_contents) {
  return PictureInPictureWindowManager::GetInstance()
      ->EnterVideoPictureInPicture(web_contents);
}

void ChromeExtensionHostDelegate::ExitPictureInPicture() {
  PictureInPictureWindowManager::GetInstance()->ExitPictureInPicture();
}

}  // namespace extensions
