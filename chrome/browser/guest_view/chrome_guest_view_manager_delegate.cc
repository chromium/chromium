// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/guest_view/chrome_guest_view_manager_delegate.h"

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/task_manager/web_contents_tags.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/app_mode/app_session_ash.h"
#include "chrome/browser/ash/app_mode/kiosk_app_manager.h"
#endif

namespace extensions {

ChromeGuestViewManagerDelegate::ChromeGuestViewManagerDelegate(
    content::BrowserContext* context)
    : ExtensionsGuestViewManagerDelegate(context) {
}

ChromeGuestViewManagerDelegate::~ChromeGuestViewManagerDelegate() {
}

void ChromeGuestViewManagerDelegate::OnGuestAdded(
    content::WebContents* guest_web_contents) const {
  ExtensionsGuestViewManagerDelegate::OnGuestAdded(guest_web_contents);

  // Attaches the task-manager-specific tag for the GuestViews to its
  // |guest_web_contents| so that their corresponding tasks show up in the task
  // manager.
  task_manager::WebContentsTags::CreateForGuestContents(guest_web_contents);

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Notifies kiosk session about the added guest.
  ash::AppSessionAsh* app_session = ash::KioskAppManager::Get()->app_session();
  if (app_session)
    app_session->OnGuestAdded(guest_web_contents);
#endif
}

}  // namespace extensions
