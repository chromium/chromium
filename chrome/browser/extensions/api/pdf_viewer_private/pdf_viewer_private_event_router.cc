// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/pdf_viewer_private/pdf_viewer_private_event_router.h"

#include "base/functional/bind.h"
#include "chrome/common/extensions/api/pdf_viewer_private.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"

namespace extensions {

// static
PdfViewerPrivateEventRouter* PdfViewerPrivateEventRouter::Create(
    content::BrowserContext* context) {
  DCHECK(context);
  Profile* profile = Profile::FromBrowserContext(context);
  return new PdfViewerPrivateEventRouter(profile);
}

PdfViewerPrivateEventRouter::PdfViewerPrivateEventRouter(Profile* profile)
    : profile_(profile) {
  DCHECK(profile_);
  user_prefs_registrar_.Init(profile_->GetPrefs());
  EventRouter::Get(profile_)->RegisterObserver(
      this, api::pdf_viewer_private::OnPdfOcrPrefChanged::kEventName);
  StartOrStopListeningForPdfOcrPrefChanges();
}

PdfViewerPrivateEventRouter::~PdfViewerPrivateEventRouter() {
  DCHECK(user_prefs_registrar_.IsEmpty());
}

void PdfViewerPrivateEventRouter::Shutdown() {
  EventRouter::Get(profile_)->UnregisterObserver(this);
  if (!user_prefs_registrar_.IsEmpty()) {
    user_prefs_registrar_.Remove(prefs::kAccessibilityPdfOcrAlwaysActive);
  }
}

void PdfViewerPrivateEventRouter::OnListenerAdded(
    const EventListenerInfo& details) {
  // Start listening to events from the user registrar for the PDF OCR pref.
  StartOrStopListeningForPdfOcrPrefChanges();
}

void PdfViewerPrivateEventRouter::OnListenerRemoved(
    const EventListenerInfo& details) {
  // Stop listening to events from the user registrar for the PDF OCR pref
  // if there are no more listeners.
  StartOrStopListeningForPdfOcrPrefChanges();
}

void PdfViewerPrivateEventRouter::StartOrStopListeningForPdfOcrPrefChanges() {
  EventRouter* event_router = EventRouter::Get(profile_);
  bool should_listen = event_router->HasEventListener(
      api::pdf_viewer_private::OnPdfOcrPrefChanged::kEventName);

  if (should_listen && user_prefs_registrar_.IsEmpty()) {
    // base::Unretained() is safe since `this` will be destroyed after this
    // listener is removed.
    user_prefs_registrar_.Add(
        prefs::kAccessibilityPdfOcrAlwaysActive,
        base::BindRepeating(
            &PdfViewerPrivateEventRouter::OnPdfOcrPreferenceChanged,
            base::Unretained(this)));
  } else if (!should_listen && !user_prefs_registrar_.IsEmpty()) {
    user_prefs_registrar_.Remove(prefs::kAccessibilityPdfOcrAlwaysActive);
  }
}

void PdfViewerPrivateEventRouter::OnPdfOcrPreferenceChanged() {
  EventRouter* event_router = EventRouter::Get(profile_);
  if (!event_router->HasEventListener(
          api::pdf_viewer_private::OnPdfOcrPrefChanged::kEventName)) {
    return;
  }
  // Send the changed value of the PDF OCR pref to observers.
  base::Value::List event_arg;
  bool is_pdf_ocr_always_active =
      profile_->GetPrefs()->GetBoolean(prefs::kAccessibilityPdfOcrAlwaysActive);
  event_arg.Append(is_pdf_ocr_always_active);
  event_router->BroadcastEvent(std::make_unique<Event>(
      events::PDF_VIEWER_PRIVATE_ON_PDF_OCR_PREF_CHANGED,
      api::pdf_viewer_private::OnPdfOcrPrefChanged::kEventName,
      std::move(event_arg)));
}

}  // namespace extensions
