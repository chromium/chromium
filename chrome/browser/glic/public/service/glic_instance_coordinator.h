// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_PUBLIC_SERVICE_GLIC_INSTANCE_COORDINATOR_H_
#define CHROME_BROWSER_GLIC_PUBLIC_SERVICE_GLIC_INSTANCE_COORDINATOR_H_

#include <optional>
#include <vector>

#include "base/callback_list.h"
#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/scoped_observation.h"
#include "base/scoped_observation_traits.h"
#include "build/build_config.h"
#include "chrome/browser/glic/host/glic_web_client_access.h"
#include "chrome/browser/glic/host/host.h"
#include "chrome/browser/glic/public/context/glic_sharing_manager.h"
#include "chrome/browser/glic/public/glic_close_options.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/glic/public/glic_instance.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_features.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/gfx/native_ui_types.h"

class Browser;

namespace content {
class RenderFrameHost;
}
namespace gfx {
class Point;
}  // namespace gfx

namespace tabs {
class TabInterface;
}

namespace glic {
DECLARE_CUSTOM_ELEMENT_EVENT_TYPE(kGlicWidgetAttached);

class GlicWidget;

// GlicInstanceCoordinator is the interface for managing Glic instances.
class GlicInstanceCoordinator {
 public:
  using StateObserver = PanelStateObserver;
  GlicInstanceCoordinator(const GlicInstanceCoordinator&) = delete;
  GlicInstanceCoordinator& operator=(const GlicInstanceCoordinator&) = delete;
  GlicInstanceCoordinator() = default;
  virtual ~GlicInstanceCoordinator() = default;

  virtual bool IsAnyPanelShowing() const = 0;
  virtual GlicInstance* GetInstanceForTab(
      const tabs::TabInterface* tab) const = 0;
  virtual void CreateNewConversationForTabs(
      const std::vector<tabs::TabInterface*>& tabs) = 0;
  virtual void ShowInstanceForTabs(const std::vector<tabs::TabInterface*>& tabs,
                                   const InstanceId& instance_id) = 0;
  virtual std::vector<ConversationInfo> GetRecentlyActiveInstances(
      size_t limit) = 0;

  virtual bool IsTabPinnedToAnyInstance(
      const tabs::TabHandle& tab_handle) const = 0;

  virtual void UnpinTabsFromAllInstances(
      base::span<const tabs::TabHandle> tab_handles,
      GlicUnpinTrigger trigger) = 0;

  // Show, summon, or activate the panel if needed, or close it if it's already
  // active and prevent_close is false.
  virtual void Toggle(
      BrowserWindowInterface* bwi,
      bool prevent_close,
      mojom::InvocationSource source,
      std::optional<std::string> deprecated_prompt_suggestion) = 0;

  // Readies glic to show.
  virtual void EnsurePreload() = 0;

  // Destroy the glic panel and its web contents.
  virtual void Shutdown() = 0;

  // Close the panel but keep the glic WebContents alive in the background.
  virtual void Close(const CloseOptions& options) = 0;
  // Closes the active embedder of an instance with matching render_frame_host
  // without resetting webcontents.
  virtual void CloseInstanceWithFrame(
      content::RenderFrameHost* render_frame_host) = 0;
  // Closes the active embedder of an instance with matching render_frame_host
  // with resetting webcontents.
  virtual void CloseAndShutdownInstanceWithFrame(
      content::RenderFrameHost* render_frame_host) = 0;
  virtual void ArchiveInstanceWithFrame(
      content::RenderFrameHost* render_frame_host) = 0;
  // Returns wehether or not the glic window is currently showing detached.
  // When True |GetGlicWidget| will return a valid ptr.
  virtual bool IsDetached() const = 0;

  // Returns whether the given browser is showing a glic panel for its active
  // tab.
  virtual bool IsPanelShowingForBrowser(
      const BrowserWindowInterface& bwi) const = 0;

  // Registers a callback to be run when any instance opens or closes.
  virtual base::CallbackListSubscription AddGlobalShowHideCallback(
      base::RepeatingClosure callback) = 0;

  // Reloads the glic web contents or the FRE's web contents (depending on
  // which is currently visible).
  virtual void Reload(content::RenderFrameHost* render_frame_host) = 0;

  using ActiveInstanceChangedCallback =
      base::RepeatingCallback<void(GlicInstance* new_instance)>;
  virtual base::CallbackListSubscription
  AddActiveInstanceChangedCallbackAndNotifyImmediately(
      ActiveInstanceChangedCallback callback) = 0;
  virtual GlicInstance* GetActiveInstance() = 0;

  // Registers a handler to observe experimental triggering related updates.
  virtual void GetExperimentalTriggeringUpdates(
      mojo::PendingRemote<mojom::ExperimentalTriggeringUpdatesHandler> handler,
      base::OnceCallback<void(bool)> success_status_callback) = 0;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_PUBLIC_SERVICE_GLIC_INSTANCE_COORDINATOR_H_
