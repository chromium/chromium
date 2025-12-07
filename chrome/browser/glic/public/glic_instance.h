// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_PUBLIC_GLIC_INSTANCE_H_
#define CHROME_BROWSER_GLIC_PUBLIC_GLIC_INSTANCE_H_

#include "base/callback_list.h"
#include "base/functional/callback.h"
#include "base/observer_list_types.h"
#include "base/scoped_observation_traits.h"
#include "base/uuid.h"
#include "chrome/browser/glic/host/glic.mojom.h"

class Browser;
namespace views {
class Widget;
}  // namespace views

namespace glic {

class Host;
class GlicInstanceMetrics;

// A type alias for the Glic instance identifier.
using InstanceId = base::Uuid;

struct PanelStateContext {
  // Provided only when kGlicMultiInstance is off.
  raw_ptr<Browser> attached_browser = nullptr;
  // Provided only when kGlicMultiInstance is off.
  raw_ptr<views::Widget> glic_widget = nullptr;
};

// Observes the state of the glic panel.
class PanelStateObserver : public base::CheckedObserver {
 public:
  virtual void PanelStateChanged(const mojom::PanelState& panel_state,
                                 const PanelStateContext& context) = 0;
};

namespace glic_instance_internal {

// Interface for UI methods that can be called on the instance.
class UIDelegate {
 public:
  virtual ~UIDelegate() = default;

  virtual bool IsShowing() const = 0;

  virtual bool IsActive() = 0;

  // Whether the instance's active embedder is attached to a chrome window.
  virtual bool IsAttached() = 0;

  virtual void AddStateObserver(PanelStateObserver* observer) = 0;
  virtual void RemoveStateObserver(PanelStateObserver* observer) = 0;

  // Returns the current panel state.
  virtual mojom::PanelState GetPanelState() = 0;

  // Register for this callback to detect UI changes to the instance.
  using StateChangeCallback =
      base::RepeatingCallback<void(bool, mojom::CurrentView view)>;
  virtual base::CallbackListSubscription RegisterStateChange(
      StateChangeCallback callback) = 0;
};

}  // namespace glic_instance_internal

// Public interface for one instance of the glic web client.
class GlicInstance : public glic_instance_internal::UIDelegate {
 public:
  // Exposes the UIDelegate interface on GlicInstance::UIDelegate.
  using UIDelegate = glic_instance_internal::UIDelegate;

  // Get this instance's Host which manages the chrome://glic WebContents.
  virtual Host& host() = 0;

  // Gets the window size of the active embedder.
  virtual gfx::Size GetPanelSize() = 0;

  // Get this instance's unique identifier.
  virtual const InstanceId& id() const = 0;

  // Get the current conversation ID for this instance.
  virtual std::optional<std::string> conversation_id() const = 0;

  virtual base::TimeTicks GetLastActiveTime() const = 0;

  virtual GlicInstanceMetrics* instance_metrics() = 0;
};

}  // namespace glic

namespace base {

template <>
struct ScopedObservationTraits<glic::GlicInstance, glic::PanelStateObserver> {
  static void AddObserver(glic::GlicInstance* source,
                          glic::PanelStateObserver* observer) {
    source->AddStateObserver(observer);
  }
  static void RemoveObserver(glic::GlicInstance* source,
                             glic::PanelStateObserver* observer) {
    source->RemoveStateObserver(observer);
  }
};

}  // namespace base

#endif  // CHROME_BROWSER_GLIC_PUBLIC_GLIC_INSTANCE_H_
