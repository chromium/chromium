// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEW_TAB_PAGE_MODULES_SAFE_BROWSING_SAFE_BROWSING_HANDLER_H_
#define CHROME_BROWSER_NEW_TAB_PAGE_MODULES_SAFE_BROWSING_SAFE_BROWSING_HANDLER_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/new_tab_page/modules/safe_browsing/safe_browsing.mojom.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/core/browser/safe_browsing_metrics_collector.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

class Profile;

using ::safe_browsing::SafeBrowsingMetricsCollector;

namespace ntp {

// Handles requests of safe browsing module sent from JS.
class SafeBrowsingHandler
    : public ntp::safe_browsing::mojom::SafeBrowsingHandler {
 public:
  SafeBrowsingHandler(
      mojo::PendingReceiver<ntp::safe_browsing::mojom::SafeBrowsingHandler>
          handler,
      Profile* profile);
  ~SafeBrowsingHandler() override;

  // ntp::safe_browsing::mojom::SafeBrowsingHandler:
  using CanShowModuleCallback =
      ntp::safe_browsing::mojom::SafeBrowsingHandler::CanShowModuleCallback;
  // Check if the module can be shown to the user. Implements checks for
  // cooldown.
  void CanShowModule(CanShowModuleCallback callback) override;
  // Processes a module click. Start cooldown if necessary.
  void ProcessModuleClick() override;
  // Dismisses a module. This will immediately start cooldown.
  void DismissModule() override;
  // Restore the module, undoing the changes done in dismiss module.
  void RestoreModule() override;

  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

 private:
  friend class SafeBrowsingHandlerTest;

  mojo::Receiver<ntp::safe_browsing::mojom::SafeBrowsingHandler> handler_;
  // Unowned copy of SafeBrowsingMetricsCollector, to log metrics and read/write
  // security sensitive events.
  raw_ptr<SafeBrowsingMetricsCollector, DanglingUntriaged> metrics_collector_;
  // Unowned copy of PrefService, to read/write prefs.
  raw_ptr<PrefService, DanglingUntriaged> pref_service_;
  // Save value of last cooldown start time, in case dismissed module is
  // restored.
  int64_t saved_last_cooldown_start_time_;
  // Save value of module shown count, in case dismissed module is restored.
  int saved_module_shown_count_;
};

}  // namespace ntp

#endif  // CHROME_BROWSER_NEW_TAB_PAGE_MODULES_SAFE_BROWSING_SAFE_BROWSING_HANDLER_H_
