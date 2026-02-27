// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/guest_view/chrome_guest_view_manager_delegate.h"

#include "chrome/browser/glic/host/guest_util.h"
#include "chrome/browser/task_manager/web_contents_tags.h"
#include "chrome/common/buildflags.h"
#include "components/guest_view/browser/guest_view_manager.h"
#include "components/guest_view/browser/guest_view_manager_delegate.h"
#include "components/guest_view/browser/slim_web_view/slim_web_view_guest.h"
#include "components/guest_view/browser/slim_web_view/slim_web_view_page_handler.h"
#include "content/public/browser/web_contents.h"

namespace android {
ChromeGuestViewManagerDelegate::ChromeGuestViewManagerDelegate() = default;
ChromeGuestViewManagerDelegate::~ChromeGuestViewManagerDelegate() = default;

void ChromeGuestViewManagerDelegate::OnGuestAdded(
    content::WebContents* guest_web_contents) const {
  guest_view::GuestViewManagerDelegate::OnGuestAdded(guest_web_contents);

  // Attaches the task-manager-specific tag for the GuestViews to its
  // `guest_web_contents` so that their corresponding tasks show up in the task
  // manager.
  task_manager::WebContentsTags::CreateForGuestContents(guest_web_contents);

  // Check if guest belongs to glic and apply specific customizations if so.
  glic::OnGuestAdded(guest_web_contents);
}

void ChromeGuestViewManagerDelegate::DispatchEvent(
    const std::string& event_name,
    base::DictValue args,
    guest_view::GuestViewBase* guest,
    int instance_id) {
  // We only have one guest type for now.
  CHECK(guest->GetViewType() == guest_view::SlimWebViewGuest::Type);
  auto* rfh = guest->owner_rfh();
  if (!rfh) {
    return;
  }
  auto* handler =
      guest_view::SlimWebViewPageHandler::GetForCurrentDocument(rfh);
  CHECK(handler);
  handler->DispatchEvent(event_name, std::move(args), instance_id);
}

bool ChromeGuestViewManagerDelegate::IsGuestAvailableToContext(
    const guest_view::GuestViewBase* guest) const {
  // For now, ensure that this is a WebUI.
  // TODO(crbug.com/460804848): Implement more specific restriction logic.
  return guest->owner_rfh()->GetMainFrame()->GetWebUI() != nullptr;
}

bool ChromeGuestViewManagerDelegate::IsOwnedByExtension(
    const guest_view::GuestViewBase* guest) {
  return false;
}

bool ChromeGuestViewManagerDelegate::IsOwnedByControlledFrameEmbedder(
    const guest_view::GuestViewBase* guest) {
  return false;
}

void ChromeGuestViewManagerDelegate::RegisterAdditionalGuestViewTypes(
    guest_view::GuestViewManager* manager) {
  manager->RegisterGuestViewType(
      guest_view::SlimWebViewGuest::Type,
      base::BindRepeating(&guest_view::SlimWebViewGuest::Create),
      base::NullCallback());
}

}  // namespace android
