// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROME_BROWSER_GLIC_BROWSER_UI_GLIC_NUDGE_CONTROLLER_H_
#define CHROME_BROWSER_GLIC_BROWSER_UI_GLIC_NUDGE_CONTROLLER_H_

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/glic/browser_ui/glic_nudge_delegate.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "chrome/browser/tab_list/tab_list_interface_observer.h"
#include "components/keyed_service/core/keyed_service.h"

class TabListInterface;
class BrowserWindowInterface;
class ScopedCallToActionLock;

namespace content {
class WebContents;
}  // namespace content

namespace glic {

// Enumerates the various user action for the Glic nudge.
enum class GlicNudgeActivity {
  kNudgeShown,
  kNudgeClicked,
  kNudgeDismissed,
  kNudgeNotShownWebContents,
  kNudgeIgnoredActiveTabChanged,
  kNudgeIgnoredNavigation,
  kNudgeNotShownWindowCallToActionUI,
  kNudgeIgnoredOpenedContextualTasksSidePanel,
  kNudgeIgnoredOmniboxContextMenuInteraction,
};

// Controller that mediates Glic Nudges and ensures that only the active tab is
// targeted.
class GlicNudgeController : public TabListInterfaceObserver {
 public:
  using GlicNudgeActivityCallback =
      base::RepeatingCallback<void(GlicNudgeActivity)>;

  explicit GlicNudgeController(BrowserWindowInterface* browser_window_interface,
                               TabListInterface* tab_list);
  GlicNudgeController(const GlicNudgeController&) = delete;
  GlicNudgeController& operator=(const GlicNudgeController& other) = delete;
  ~GlicNudgeController() override;

  void SetTabStripDelegate(GlicNudgeDelegate* delegate) {
    tab_strip_delegate_ = delegate;
  }
  void SetToolbarDelegate(GlicNudgeDelegate* delegate) {
    toolbar_delegate_ = delegate;
  }

  // Updates the `nudge_label` for `web_contents`, if the WebContents is active.
  // The nudge will be removed from `web_contents` if `nudge_label` is empty.
  // `activity` must be supplied iff. `nudge_label` is empty, to identify the
  // reason of nudge removal.
  virtual void UpdateNudgeLabel(content::WebContents* web_contents,
                                const std::string& nudge_label,
                                std::optional<std::string> prompt_suggestion,
                                const std::string& anchored_message_text,
                                std::optional<GlicNudgeActivity> activity,
                                GlicNudgeActivityCallback callback);

  void OnNudgeActivity(GlicNudgeActivity activity);

  void SetNudgeActivityCallbackForTesting();

  std::optional<std::string> GetPromptSuggestion() {
    return prompt_suggestion_;
  }

  void ClearPromptSuggestion() { prompt_suggestion_.reset(); }

 private:
  // TabListInterfaceObserver:
  void OnActiveTabChanged(TabListInterface& tab_list,
                          tabs::TabInterface* tab) override;

  GlicNudgeDelegate* GetActiveDelegate();

  const raw_ptr<BrowserWindowInterface> browser_window_interface_;
  const raw_ptr<TabListInterface> tab_list_;

  raw_ptr<GlicNudgeDelegate> tab_strip_delegate_ = nullptr;
  raw_ptr<GlicNudgeDelegate> toolbar_delegate_ = nullptr;

#if !BUILDFLAG(IS_ANDROID)
  std::unique_ptr<GlicNudgeDelegate> anchored_nudge_controller_;
#endif

  // The suggested prompt associated with the nudge label.
  std::optional<std::string> prompt_suggestion_;

  // Callback to invoke for user actions on the nudge.
  GlicNudgeActivityCallback nudge_activity_callback_;

  base::ScopedObservation<TabListInterface, TabListInterfaceObserver>
      tab_list_observation_{this};
  std::unique_ptr<ScopedCallToActionLock> scoped_call_to_action_lock_;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_BROWSER_UI_GLIC_NUDGE_CONTROLLER_H_
