// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_PUBLIC_GLIC_SIDE_PANEL_COORDINATOR_H_
#define CHROME_BROWSER_GLIC_PUBLIC_GLIC_SIDE_PANEL_COORDINATOR_H_

#include <memory>

#include "base/callback_list.h"
#include "base/functional/callback.h"
#include "base/supports_user_data.h"
#include "build/build_config.h"
#include "chrome/browser/glic/public/glic_close_options.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

namespace tabs {
class TabInterface;
}  // namespace tabs

namespace content {
class WebContents;
}  // namespace content

#if !BUILDFLAG(IS_ANDROID)
namespace views {
class View;
}  // namespace views
#endif

namespace glic {

// GlicSidePanelCoordinator handles the creation and registration of the
// glic SidePanelEntry.
class GlicSidePanelCoordinator {
 public:
  DECLARE_USER_DATA(GlicSidePanelCoordinator);
  virtual ~GlicSidePanelCoordinator();

  // Returns true if the Glic side panel is the currently active side panel
  // entry for `tab`. This means it will be shown if `tab` is foregrounded, or
  // is currently visible if `tab` is already foregrounded.
  static bool IsGlicSidePanelActive(tabs::TabInterface* tab);

  static GlicSidePanelCoordinator* GetForTab(tabs::TabInterface* tab);

  // The current state of the Glic side panel.
  enum class State {
    // The side panel is showing in the foreground.
    kShown,
    // The side panel is in the background, but it will show if its tab becomes
    // active.
    kBackgrounded,
    // The side panel is closed and will only be shown if explicitly requested.
    kClosed,
  };

  // Show the Glic side panel.
  virtual void Show(bool suppress_animations) = 0;
  void Show() { Show(false); }

  // Close the Glic side panel.
  virtual void Close(const CloseOptions& options) = 0;
  void Close() { Close({}); }

  // Returns true if the Glic side panel is currently the active entry.
  virtual bool IsShowing() const = 0;

  virtual State state() = 0;

  // Registers `callback` to be called when panel visibility is updated.
  virtual base::CallbackListSubscription AddStateCallback(
      base::RepeatingCallback<void(State state)> callback) = 0;

#if !BUILDFLAG(IS_ANDROID)
  // Sets the content view for the Glic side panel.
  virtual void SetContentsView(std::unique_ptr<views::View> contents_view) = 0;
#else
  // Sets the web contents for the Glic side panel.
  virtual void SetWebContents(content::WebContents* web_contents) = 0;
#endif

  // Returns preferred side panel width. Not guaranteed to be used if user
  // manually set a different width.
  virtual int GetPreferredWidth() = 0;

  // Returns true if the Glic side panel is the currently active side panel
  // entry.
  virtual bool IsGlicSidePanelActive() = 0;

 protected:
  explicit GlicSidePanelCoordinator(tabs::TabInterface* tab);

 private:
  ui::ScopedUnownedUserData<GlicSidePanelCoordinator> scoped_user_data_;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_PUBLIC_GLIC_SIDE_PANEL_COORDINATOR_H_
