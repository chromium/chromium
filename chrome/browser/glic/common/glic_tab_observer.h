// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_COMMON_GLIC_TAB_OBSERVER_H_
#define CHROME_BROWSER_GLIC_COMMON_GLIC_TAB_OBSERVER_H_

#include <memory>
#include <variant>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"

class Profile;

namespace tabs {
class TabInterface;
}  // namespace tabs

enum class TabCreationType {
  kUserInitiated,
  kFromLink,
  kUnknown,
};

// Event fired when a new tab is inserted into the tab strip.
struct TabCreationEvent {
  // The newly created tab. Only valid for the duration of the callback.
  raw_ptr<tabs::TabInterface> new_tab = nullptr;

  // The previously active tab, if any. Only valid for the duration of the
  // callback.
  raw_ptr<tabs::TabInterface> old_tab = nullptr;

  // The tab that opened the new tab, if known. Only valid for the duration of
  // the callback.
  raw_ptr<tabs::TabInterface> opener = nullptr;

  TabCreationType creation_type = TabCreationType::kUnknown;
};

// Event fired when a tab is removed, moved, replaced, navigated, or favicon
// changed.
struct TabMutationEvent {};

// Event fired when the active tab changes in a window.
struct TabActivationEvent {
  // The newly active tab.
  raw_ptr<tabs::TabInterface> new_active_tab = nullptr;

  // The previously active tab.
  raw_ptr<tabs::TabInterface> old_active_tab = nullptr;
};

using GlicTabEvent =
    std::variant<TabCreationEvent, TabMutationEvent, TabActivationEvent>;

// Observes tab events across all windows for a specific profile.
class GlicTabObserver {
 public:
  using EventCallback = base::RepeatingCallback<void(const GlicTabEvent&)>;

  virtual ~GlicTabObserver() = default;

  // Returns a platform-specific implementation.
  static std::unique_ptr<GlicTabObserver> Create(Profile* profile,
                                                 EventCallback callback);

 protected:
  GlicTabObserver() = default;
};

#endif  // CHROME_BROWSER_GLIC_COMMON_GLIC_TAB_OBSERVER_H_
