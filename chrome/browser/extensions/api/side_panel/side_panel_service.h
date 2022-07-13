// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_SIDE_PANEL_SIDE_PANEL_SERVICE_H_
#define CHROME_BROWSER_EXTENSIONS_API_SIDE_PANEL_SIDE_PANEL_SERVICE_H_

#include "base/containers/flat_map.h"
#include "chrome/common/extensions/api/side_panel.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/common/extension_id.h"

namespace extensions {

// The single responsibility of this service is to be the source of truth for
// side panel options. Extensions can interact with this service using the API
// and side panel UI updates can rely on the response of GetOptions(tab_id).
class SidePanelService : public BrowserContextKeyedAPI {
 public:
  explicit SidePanelService(content::BrowserContext* context);

  SidePanelService(const SidePanelService&) = delete;
  SidePanelService& operator=(const SidePanelService&) = delete;

  ~SidePanelService() override;

  // Convenience method to get the SidePanelService for a profile.
  static SidePanelService* Get(content::BrowserContext* context);

  // BrowserContextKeyedAPI implementation.
  static BrowserContextKeyedAPIFactory<SidePanelService>* GetFactoryInstance();

  // Get options for tab_id. Options are loaded in order first from service
  // storage, manifest, or an empty object will be returned, if they're unset.
  using TabId = int;
  api::side_panel::PanelOptions GetOptions(const Extension& extension,
                                           absl::optional<TabId> tab_id);

  // Set options for tab_id if specified. Otherwise set default options.
  void SetOptions(const Extension& extension,
                  api::side_panel::PanelOptions set_options);

 private:
  // TODO(crbug.com/1328645): Remove options for matching ExtensionId on
  // uninstallation.
  friend class BrowserContextKeyedAPIFactory<SidePanelService>;

  content::BrowserContext* const browser_context_;

  // BrowserContextKeyedAPI implementation.
  static const char* service_name() { return "SidePanelService"; }
  static const bool kServiceRedirectedInIncognito = true;
  static const bool kServiceIsNULLWhileTesting = true;

  using TabPanelOptions = base::flat_map<TabId, api::side_panel::PanelOptions>;
  using ExtensionPanelOptions = base::flat_map<ExtensionId, TabPanelOptions>;

  ExtensionPanelOptions panels_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_SIDE_PANEL_SIDE_PANEL_SERVICE_H_
