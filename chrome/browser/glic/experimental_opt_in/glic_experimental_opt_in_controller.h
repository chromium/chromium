// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_EXPERIMENTAL_OPT_IN_GLIC_EXPERIMENTAL_OPT_IN_CONTROLLER_H_
#define CHROME_BROWSER_GLIC_EXPERIMENTAL_OPT_IN_GLIC_EXPERIMENTAL_OPT_IN_CONTROLLER_H_

#include <memory>
#include <vector>

#include "base/callback_list.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "ui/views/widget/widget.h"

class Profile;
class GURL;

namespace base {
class TickClock;
}

namespace content {
class WebContents;
}

namespace views {
class Widget;
}

namespace tabs {
class TabInterface;
}

namespace glic {

class GlicExperimentalOptInDialogView;

// Controller for the experimental triggering opt-in flow.
class GlicExperimentalOptInController {
 public:
  explicit GlicExperimentalOptInController(Profile* profile);
  GlicExperimentalOptInController(const GlicExperimentalOptInController&) =
      delete;
  GlicExperimentalOptInController& operator=(
      const GlicExperimentalOptInController&) = delete;
  ~GlicExperimentalOptInController();

  // Shows the opt-in dialog. Returns the widget, or null if the dialog could
  // not be shown. `callback` is fired on dialog close, with true if the opt in
  // was accepted, and false otherwise. If the dialog could not be shown,
  // callback is fired immediately with true if the dialog was not needed
  // because the user is already opted in, and false otherwise.
  // If a dialog is already showing, the existing dialog is returned, and
  // `callback` will fire on closing of the existing dialog.
  views::Widget* ShowDialog(content::WebContents* web_contents,
                            base::OnceCallback<void(bool)> callback);
  void CloseDialog(bool accepted);
  void OpenLinkInNewTab(const GURL& url);

  // Gets, or creates, a web contents suitable for showing the experimental
  // opt-in dialog over. Callers can then use the returned web contents with
  // ShowDialog().
  content::WebContents* GetOrCreateSuitableWebContents();

  GlicExperimentalOptInDialogView* GetDialogViewForTesting() {
    return dialog_view_.get();
  }

  void SetTickClockForTesting(const base::TickClock* clock) {
    tick_clock_ = clock;
  }

 private:
  void CloseWidget(views::Widget::ClosedReason reason);
  void TabDidBecomeVisible(tabs::TabInterface* tab_interface);
  void TabWillBecomeHidden(tabs::TabInterface* tab_interface);

  raw_ptr<Profile> profile_;
  raw_ptr<const base::TickClock> tick_clock_;
  base::WeakPtr<tabs::TabInterface> tab_interface_;
  std::unique_ptr<GlicExperimentalOptInDialogView> dialog_view_;
  std::unique_ptr<views::Widget> dialog_widget_;

  std::vector<base::OnceCallback<void(bool)>> callbacks_;
  std::vector<base::CallbackListSubscription> tab_subscriptions_;
  base::TimeTicks dialog_open_time_;
  base::TimeTicks visibility_start_time_;
  base::TimeDelta visible_duration_;

  base::WeakPtrFactory<GlicExperimentalOptInController> weak_ptr_factory_{this};
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_EXPERIMENTAL_OPT_IN_GLIC_EXPERIMENTAL_OPT_IN_CONTROLLER_H_
