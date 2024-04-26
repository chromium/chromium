// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_SIDE_PANEL_SIDE_PANEL_SERVICE_H_
#define CHROME_BROWSER_EXTENSIONS_API_SIDE_PANEL_SIDE_PANEL_SERVICE_H_

#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/scoped_observation.h"
#include "base/types/expected.h"
#include "chrome/common/extensions/api/side_panel.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_observer.h"
#include "extensions/common/extension_id.h"

namespace extensions {

// The single responsibility of this service is to be the source of truth for
// side panel options. Extensions can interact with this service using the API
// and side panel UI updates can rely on the response of GetOptions(tab_id).
class SidePanelService : public BrowserContextKeyedAPI,
                         public ExtensionRegistryObserver {
 public:
  class Observer : public base::CheckedObserver {
   public:
    virtual void OnPanelOptionsChanged(
        const ExtensionId& extension_id,
        const api::side_panel::PanelOptions& updated_options) = 0;
    virtual void OnSidePanelServiceShutdown() = 0;
  };

  explicit SidePanelService(content::BrowserContext* context);

  SidePanelService(const SidePanelService&) = delete;
  SidePanelService& operator=(const SidePanelService&) = delete;

  ~SidePanelService() override;

  // Convenience method to get the SidePanelService for a profile.
  static SidePanelService* Get(content::BrowserContext* context);

  // BrowserContextKeyedAPI implementation.
  static BrowserContextKeyedAPIFactory<SidePanelService>* GetFactoryInstance();

  using TabId = int;

  // Returns if there is an action to toggle the side panel for the given
  // `extension` and `tab_id`.
  bool HasSidePanelActionForTab(const Extension& extension, TabId tab_id);

  // Returns if there is an action to toggle the side panel from the extension
  // context menu for the given `extension` and `tab_id`.
  bool HasSidePanelContextMenuActionForTab(const Extension& extension,
                                           TabId tab_id);

  // Get options for `tab_id`. Options are loaded in order first from service
  // storage, manifest, or an empty object will be returned, if they're unset.
  api::side_panel::PanelOptions GetOptions(const Extension& extension,
                                           std::optional<TabId> tab_id);

  // Get options that were set for `tab_id`. If no options were specifically
  // set, returns an empty object instead of falling back to default options.
  api::side_panel::PanelOptions GetSpecificOptionsForTab(
      const Extension& extension,
      TabId tab_id);

  // Set options for tab_id if specified. Otherwise set default options.
  void SetOptions(const Extension& extension,
                  api::side_panel::PanelOptions set_options);

  // Determine if panel options have been set for extension id. Used in tests.
  bool HasExtensionPanelOptionsForTest(const ExtensionId& id);

  // Returns whether the extension will open its side panel entry when its icon
  // in the toolbar is clicked.
  bool OpenSidePanelOnIconClick(const ExtensionId& extension_id);

  // Updates whether the extension will open its side panel entry when its icon
  // in the toolbar is clicked.
  void SetOpenSidePanelOnIconClick(const ExtensionId& extension_id,
                                   bool open_side_panel_on_icon_click);

  // Opens the `extension`'s side panel for the specified `tab_id` and profile
  // specified by `context`. Handles properly determining if the side panel to
  // be opened is a global or contextual panel. `include_incognito_information`
  // indicates whether the registry should allow crossing incognito contexts
  // when looking up `tab_id`. If `window_id` is specified, checks that the
  // given `tab_id` belongs to the `window_id`. Returns true on success; returns
  // an error string on failure.
  // TODO(crbug.com/40064601): Return an enum here to indicate if the
  // panel was newly-opened vs already-opened in order to support waiting for
  // the panel to open?
  base::expected<bool, std::string> OpenSidePanelForTab(
      const Extension& extension,
      content::BrowserContext* context,
      int tab_id,
      std::optional<int> window_id,
      bool include_incognito_information);

  // Opens the `extension`'s side panel for the specified `window_id` and
  // profile specified by `context`. This is only valid if the extension has a
  // registered global side panel. This will not override any contextual panels
  // in the window. `include_incognito_information` indicates whether the
  // registry should allow crossing incognito contexts when looking up `tab_id`.
  // Returns true on success; returns an error string on failure.
  // TODO(crbug.com/40064601): Return an enum here to indicate if the
  // panel was newly-opened vs already-opened in order to support waiting for
  // the panel to open?
  base::expected<bool, std::string> OpenSidePanelForWindow(
      const Extension& extension,
      content::BrowserContext* context,
      int window_id,
      bool include_incognito_information);

  // Adds or removes observers.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 private:
  friend class BrowserContextKeyedAPIFactory<SidePanelService>;

  const raw_ptr<content::BrowserContext> browser_context_;

  // BrowserContextKeyedAPI implementation.
  static const char* service_name() { return "SidePanelService"; }
  static const bool kServiceRedirectedInIncognito = true;
  static const bool kServiceIsNULLWhileTesting = true;

  // Returns if there is an extension side panel for `tab_id`.
  bool HasSidePanelAvailableForTab(const Extension& extension, TabId tab_id);

  // Remove extension id and associated options from `panels_`.
  void RemoveExtensionOptions(const ExtensionId& id);

  // ExtensionRegistry:
  void OnExtensionUnloaded(content::BrowserContext* browser_context,
                           const Extension* extension,
                           UnloadedExtensionReason reason) override;

  // ExtensionRegistry:
  void OnExtensionUninstalled(content::BrowserContext* browser_context,
                              const Extension* extension,
                              UninstallReason reason) override;

  // KeyedService implementation.
  void Shutdown() override;

  // The associated observers.
  base::ObserverList<Observer> observers_;

  // ExtensionRegistry observer.
  base::ScopedObservation<extensions::ExtensionRegistry,
                          extensions::ExtensionRegistryObserver>
      extension_registry_observation_{this};

  // Extension and tab panel options.
  using TabPanelOptions = base::flat_map<TabId, api::side_panel::PanelOptions>;
  using ExtensionPanelOptions = base::flat_map<ExtensionId, TabPanelOptions>;
  ExtensionPanelOptions panels_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_SIDE_PANEL_SIDE_PANEL_SERVICE_H_
