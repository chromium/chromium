// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_PDF_VIEWER_PRIVATE_PDF_VIEWER_PRIVATE_EVENT_ROUTER_H_
#define CHROME_BROWSER_EXTENSIONS_API_PDF_VIEWER_PRIVATE_PDF_VIEWER_PRIVATE_EVENT_ROUTER_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_change_registrar.h"
#include "extensions/browser/event_router.h"

namespace content {
class BrowserContext;
}

namespace extensions {

class PdfViewerPrivateEventRouter : public KeyedService,
                                    public EventRouter::Observer {
 public:
  static std::unique_ptr<PdfViewerPrivateEventRouter> Create(
      content::BrowserContext* context);

  explicit PdfViewerPrivateEventRouter(Profile* profile);

  PdfViewerPrivateEventRouter(const PdfViewerPrivateEventRouter&) = delete;
  PdfViewerPrivateEventRouter& operator=(const PdfViewerPrivateEventRouter&) =
      delete;

  ~PdfViewerPrivateEventRouter() override;

  // KeyedService overrides:
  void Shutdown() override;

  // EventRouter::Observer overrides:
  void OnListenerAdded(const EventListenerInfo& details) override;
  void OnListenerRemoved(const EventListenerInfo& details) override;

 private:
  // Decide if we should listen for pref changes or not. If there are any
  // JavaScript listeners registered for the onPdfOcrPrefChanged event, then we
  // want to register for change notification from the user registrar.
  // Otherwise, we want to unregister and not be listening for pref changes.
  void StartOrStopListeningForPdfOcrPrefChanges();

  // Sends a pref change to any listeners (if they exist; no-ops otherwise).
  void OnPdfOcrPreferenceChanged();

  const raw_ptr<Profile> profile_ = nullptr;

  // This registrar monitors for user prefs changes.
  PrefChangeRegistrar user_prefs_registrar_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_PDF_VIEWER_PRIVATE_PDF_VIEWER_PRIVATE_EVENT_ROUTER_H_
