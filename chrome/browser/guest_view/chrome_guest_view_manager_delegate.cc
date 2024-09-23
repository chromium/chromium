// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/guest_view/chrome_guest_view_manager_delegate.h"

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/task_manager/web_contents_tags.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/app_mode/kiosk_chrome_app_manager.h"
#include "chrome/browser/ash/app_mode/kiosk_controller.h"
#include "chrome/browser/ash/app_mode/kiosk_system_session.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

namespace extensions {

ChromeGuestViewManagerDelegate::ChromeGuestViewManagerDelegate() = default;

ChromeGuestViewManagerDelegate::~ChromeGuestViewManagerDelegate() = default;

void ChromeGuestViewManagerDelegate::OnGuestAdded(
    content::WebContents* guest_web_contents) const {
  ExtensionsGuestViewManagerDelegate::OnGuestAdded(guest_web_contents);

  // Attaches the task-manager-specific tag for the GuestViews to its
  // `guest_web_contents` so that their corresponding tasks show up in the task
  // manager.
  task_manager::WebContentsTags::CreateForGuestContents(guest_web_contents);

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Notifies Kiosk controller about the added guest.
  ash::KioskController::Get().OnGuestAdded(guest_web_contents);
#endif
}

// ExtensionsGuestViewManagerDelegate::IsGuestAvailableToContextWithFeature()
// will check for the availability of the feature provided by |guest|. If the
// API feature provided is "controlledFrameInternal", the controlled_frame.cc's
// AvailabilityCheck will be run to verify the associated RenderFrameHost is
// isolated and that it's only exposed in the expected schemes / feature modes.
bool ChromeGuestViewManagerDelegate::IsOwnedByControlledFrameEmbedder(
    const guest_view::GuestViewBase* guest) {
  return ExtensionsGuestViewManagerDelegate::
      IsGuestAvailableToContextWithFeature(guest, "controlledFrameInternal");
}

}  // namespace extensions
