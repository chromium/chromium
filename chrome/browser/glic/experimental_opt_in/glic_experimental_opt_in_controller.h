// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_EXPERIMENTAL_OPT_IN_GLIC_EXPERIMENTAL_OPT_IN_CONTROLLER_H_
#define CHROME_BROWSER_GLIC_EXPERIMENTAL_OPT_IN_GLIC_EXPERIMENTAL_OPT_IN_CONTROLLER_H_

#include <memory>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "chrome/browser/glic/experimental_opt_in/glic_experimental_opt_in_ui_host.h"

class Profile;
class GURL;

namespace base {
class TickClock;
}

namespace content {
class WebContents;
}

#if !BUILDFLAG(IS_ANDROID)
namespace views {
class Widget;
}
#endif

namespace glic {

#if !BUILDFLAG(IS_ANDROID)
class GlicExperimentalOptInDialogView;
#endif

// Controller for the experimental triggering opt-in flow.
class GlicExperimentalOptInController
    : public GlicExperimentalOptInUIHost::Delegate {
 public:
  explicit GlicExperimentalOptInController(Profile* profile);
  GlicExperimentalOptInController(const GlicExperimentalOptInController&) =
      delete;
  GlicExperimentalOptInController& operator=(
      const GlicExperimentalOptInController&) = delete;
  ~GlicExperimentalOptInController() override;

  // Shows the opt-in UI. `callback` is fired on UI close, with true if the opt
  // in was accepted, and false otherwise. If the UI could not be shown,
  // callback is fired immediately with true if the UI was not needed
  // because the user is already opted in, and false otherwise.
  // If the UI is already showing, `callback` will fire on closing of the
  // existing UI.
  void ShowDialog(content::WebContents* web_contents,
                  base::OnceCallback<void(bool)> callback);
  void CloseDialog(bool accepted);
  void OpenLinkInNewTab(const GURL& url);

  // Gets, or creates, a web contents suitable for showing the experimental
  // opt-in UI over. Callers can then use the returned web contents with
  // ShowDialog().
  content::WebContents* GetOrCreateSuitableWebContents();

#if !BUILDFLAG(IS_ANDROID)
  views::Widget* GetDialogWidgetForTesting();
  GlicExperimentalOptInDialogView* GetDialogViewForTesting();
#endif

  void SetTickClockForTesting(const base::TickClock* clock);

  // GlicExperimentalOptInUIHost::Delegate:
  void OnUIClosed(bool accepted) override;

 private:
  raw_ptr<Profile> profile_;
  std::unique_ptr<GlicExperimentalOptInUIHost> host_;
  std::vector<base::OnceCallback<void(bool)>> callbacks_;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_EXPERIMENTAL_OPT_IN_GLIC_EXPERIMENTAL_OPT_IN_CONTROLLER_H_
