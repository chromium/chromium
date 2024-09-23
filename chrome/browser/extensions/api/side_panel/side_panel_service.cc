// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/side_panel/side_panel_service.h"

#include <cstddef>
#include <memory>
#include <optional>

#include "base/no_destructor.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/extensions/extension_side_panel_utils.h"
#include "chrome/common/extensions/api/side_panel.h"
#include "chrome/common/extensions/api/side_panel/side_panel_info.h"
#include "components/sessions/core/session_id.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/pref_types.h"

namespace extensions {

namespace {

// Key corresponding to whether the extension's side panel entry (if one exists)
// should be opened when its icon is clicked in the toolbar.
constexpr PrefMap kOpenSidePanelOnIconClickPref = {
    "open_side_panel_on_icon_click", PrefType::kBool,
    PrefScope::kExtensionSpecific};

api::side_panel::PanelOptions GetPanelOptionsFromManifest(
    const Extension& extension) {
  auto path = SidePanelInfo::GetDefaultPath(&extension);
  api::side_panel::PanelOptions options;
  if (!path.empty()) {
    options.path = std::string(path);
    options.enabled = true;
  }
  return options;
}

}  // namespace

SidePanelService::~SidePanelService() = default;

SidePanelService::SidePanelService(content::BrowserContext* context)
    : browser_context_(context) {
  extensions::ExtensionRegistry* extension_registry =
      extensions::ExtensionRegistry::Get(context);
  extension_registry_observation_.Observe(extension_registry);
}

bool SidePanelService::HasSidePanelActionForTab(const Extension& extension,
                                                TabId tab_id) {
  if (!OpenSidePanelOnIconClick(extension.id())) {
    return false;
  }

  return HasSidePanelAvailableForTab(extension, tab_id);
}

bool SidePanelService::HasSidePanelContextMenuActionForTab(
    const Extension& extension,
    TabId tab_id) {
  return HasSidePanelAvailableForTab(extension, tab_id);
}

bool SidePanelService::HasSidePanelAvailableForTab(const Extension& extension,
                                                   TabId tab_id) {
  api::side_panel::PanelOptions options = GetOptions(extension, tab_id);
  return options.enabled.has_value() && *options.enabled &&
         options.path.has_value();
}

api::side_panel::PanelOptions SidePanelService::GetOptions(
    const Extension& extension,
    std::optional<TabId> id) {
  auto extension_panel_options = panels_.find(extension.id());

  // Get default path from manifest if nothing was stored in this service for
  // the calling extension.
  if (extension_panel_options == panels_.end()) {
    return GetPanelOptionsFromManifest(extension);
  }

  TabId default_tab_id = SessionID::InvalidValue().id();
  TabId tab_id = id.has_value() ? id.value() : default_tab_id;
  TabPanelOptions& tab_panel_options = extension_panel_options->second;

  // The specific `tab_id` may have already been saved.
  if (tab_id != default_tab_id) {
    auto specific_tab_options = tab_panel_options.find(tab_id);
    if (specific_tab_options != tab_panel_options.end())
      return specific_tab_options->second.Clone();
  }

  // Fall back to the default tab if no tab ID was specified or entries for the
  // specific tab weren't found.
  auto default_options = tab_panel_options.find(default_tab_id);
  if (default_options != tab_panel_options.end()) {
    return default_options->second.Clone();
  }

  // Fall back to the manifest-specified options as a last resort.
  return GetPanelOptionsFromManifest(extension);
}

api::side_panel::PanelOptions SidePanelService::GetSpecificOptionsForTab(
    const Extension& extension,
    TabId tab_id) {
  auto extension_panel_options = panels_.find(extension.id());
  if (extension_panel_options == panels_.end()) {
    return api::side_panel::PanelOptions();
  }

  TabPanelOptions& tab_panel_options = extension_panel_options->second;
  auto specific_tab_options = tab_panel_options.find(tab_id);
  return specific_tab_options == tab_panel_options.end()
             ? api::side_panel::PanelOptions()
             : specific_tab_options->second.Clone();
}

// Upsert to merge `panels_[extension_id][tab_id]` with `set_options`.
void SidePanelService::SetOptions(const Extension& extension,
                                  api::side_panel::PanelOptions options) {
  auto update_existing_options =
      [&options](api::side_panel::PanelOptions& existing_options) {
        if (options.path) {
          existing_options.path = std::move(options.path);
        }
        if (options.enabled) {
          existing_options.enabled = std::move(options.enabled);
        }
      };

  TabId tab_id = SessionID::InvalidValue().id();
  if (options.tab_id)
    tab_id = *options.tab_id;
  TabPanelOptions& extension_panel_options = panels_[extension.id()];
  auto it = extension_panel_options.find(tab_id);

  // Create the options if they don't exist, otherwise update them.
  if (it != extension_panel_options.end()) {
    update_existing_options(it->second);
  } else {
    // The default value for the optional enabled option is true. This default
    // is applied when the supplied option is being inserted for the first time.
    if (!options.enabled.has_value()) {
      options.enabled = true;
    }

    // If there is no entry for the default tab, merge `options` into the
    // manifest-specified options.
    if (tab_id == SessionID::InvalidValue().id()) {
      extension_panel_options[tab_id] = GetPanelOptionsFromManifest(extension);
      update_existing_options(extension_panel_options[tab_id]);
    } else {
      // Update an existing option.
      extension_panel_options[tab_id] = std::move(options);
    }
  }

  for (auto& observer : observers_) {
    observer.OnPanelOptionsChanged(extension.id(),
                                   extension_panel_options[tab_id]);
  }
}

bool SidePanelService::HasExtensionPanelOptionsForTest(const ExtensionId& id) {
  return panels_.count(id) != 0;
}

// static
BrowserContextKeyedAPIFactory<SidePanelService>*
SidePanelService::GetFactoryInstance() {
  static base::NoDestructor<BrowserContextKeyedAPIFactory<SidePanelService>>
      instance;
  return instance.get();
}

// static
SidePanelService* SidePanelService::Get(content::BrowserContext* context) {
  return BrowserContextKeyedAPIFactory<SidePanelService>::Get(context);
}

void SidePanelService::RemoveExtensionOptions(const ExtensionId& id) {
  panels_.erase(id);
}

bool SidePanelService::OpenSidePanelOnIconClick(
    const ExtensionId& extension_id) {
  bool open_side_panel_on_icon_click = false;
  // TODO(tjudkins): This should be taking in a browser context to read the pref
  // on, rather than using the one the service was created with.
  ExtensionPrefs::Get(browser_context_)
      ->ReadPrefAsBoolean(extension_id, kOpenSidePanelOnIconClickPref,
                          &open_side_panel_on_icon_click);
  return open_side_panel_on_icon_click;
}

void SidePanelService::SetOpenSidePanelOnIconClick(
    const ExtensionId& extension_id,
    bool open_side_panel_on_icon_click) {
  // TODO(tjudkins): This should be taking in a browser context to set the pref
  // on, rather than using the one the service was created with.
  ExtensionPrefs::Get(browser_context_)
      ->SetBooleanPref(extension_id, kOpenSidePanelOnIconClickPref,
                       open_side_panel_on_icon_click);
}

base::expected<bool, std::string> SidePanelService::OpenSidePanelForWindow(
    const Extension& extension,
    content::BrowserContext* context,
    int window_id,
    bool include_incognito_information) {
  std::string error;
  WindowController* window_controller =
      ExtensionTabUtil::GetControllerInProfileWithId(
          Profile::FromBrowserContext(context), window_id,
          include_incognito_information, &error);
  if (!window_controller) {
    return base::unexpected(error);
  }

  auto global_options = GetOptions(extension, std::nullopt);
  if (!global_options.path || !global_options.enabled.has_value() ||
      !(*global_options.enabled)) {
    return base::unexpected(
        base::StringPrintf("No active side panel for windowId: %d", window_id));
  }

  side_panel_util::OpenGlobalExtensionSidePanel(
      *window_controller->GetBrowser(), /*web_contents=*/nullptr,
      extension.id());
  return true;
}

base::expected<bool, std::string> SidePanelService::OpenSidePanelForTab(
    const Extension& extension,
    content::BrowserContext* context,
    int tab_id,
    std::optional<int> window_id,
    bool include_incognito_information) {
  // First, find the corresponding tab.
  Browser* browser = nullptr;
  content::WebContents* web_contents = nullptr;
  if (!ExtensionTabUtil::GetTabById(tab_id, context,
                                    include_incognito_information, &browser,
                                    nullptr, &web_contents, nullptr)) {
    return base::unexpected(
        base::StringPrintf("No tab with tabId: %d", tab_id));
  }

  CHECK(browser);

  // If both `tab_id` and `window_id` were provided, ensure the tab is in
  // the specified window.
  if (window_id && window_id != ExtensionTabUtil::GetWindowId(browser)) {
    return base::unexpected(
        "The specified tab does not belong to the specified window.");
  }

  // Next, determine if we an active side panel (contextual or global) for that
  // tab.
  api::side_panel::PanelOptions panel_options = GetOptions(extension, tab_id);
  if (!panel_options.path || !panel_options.enabled.has_value() ||
      !(*panel_options.enabled)) {
    return base::unexpected(
        base::StringPrintf("No active side panel for tabId: %d", tab_id));
  }

  // If we do have an active panel, check if it's a contextual panel.
  bool has_contextual_panel = false;
  auto panels_iter = panels_.find(extension.id());
  if (panels_iter != panels_.end()) {
    auto tab_panels_iter = panels_iter->second.find(tab_id);
    if (tab_panels_iter != panels_iter->second.end()) {
      auto& options = tab_panels_iter->second;
      CHECK(options.path);
      CHECK(options.enabled.has_value());
      CHECK(options.enabled.value());
      has_contextual_panel = true;
    }
  }

  // Open the appropriate panel.
  if (has_contextual_panel) {
    side_panel_util::OpenContextualExtensionSidePanel(*browser, *web_contents,
                                                      extension.id());
  } else {
    side_panel_util::OpenGlobalExtensionSidePanel(*browser, web_contents,
                                                  extension.id());
  }

  return true;
}

void SidePanelService::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void SidePanelService::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void SidePanelService::OnExtensionUnloaded(
    content::BrowserContext* browser_context,
    const Extension* extension,
    UnloadedExtensionReason reason) {
  RemoveExtensionOptions(extension->id());
}

void SidePanelService::OnExtensionUninstalled(
    content::BrowserContext* browser_context,
    const Extension* extension,
    UninstallReason reason) {
  RemoveExtensionOptions(extension->id());
}

void SidePanelService::Shutdown() {
  for (auto& observer : observers_) {
    observer.OnSidePanelServiceShutdown();
  }
}

}  // namespace extensions
