// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_FIRST_RUN_FIRST_RUN_CONTROLLER_H_
#define CHROME_BROWSER_CHROMEOS_FIRST_RUN_FIRST_RUN_CONTROLLER_H_

#include <stddef.h>

#include <memory>
#include <string>
#include <vector>

#include "ash/public/cpp/shelf_types.h"
#include "base/callback.h"
#include "base/macros.h"
#include "base/time/time.h"
#include "chrome/browser/ui/webui/chromeos/first_run/first_run_actor.h"
#include "ui/gfx/geometry/size.h"

class Profile;

namespace ash {
class FirstRunHelper;
}

namespace content {
class WebContents;
}

namespace views {
class Widget;
}

namespace chromeos {

class FirstRunUIBrowserTest;

namespace first_run {
class Step;
}

// FirstRunController creates and manages first-run tutorial.
// Object manages its lifetime and deletes itself after completion of the
// tutorial.
class FirstRunController : public FirstRunActor::Delegate {
 public:
  ~FirstRunController() override;

  // Creates first-run UI and starts tutorial.
  static void Start();

  // Finalizes first-run tutorial and destroys UI.
  static void Stop();

  // Returns the size of the semi-transparent overlay window in DIPs.
  gfx::Size GetOverlaySize() const;

  // Returns the shelf alignment on the primary display.
  ash::ShelfAlignment GetShelfAlignment() const;

  // Stops the tutorial and records early cancellation metrics.
  void Cancel();

  ash::FirstRunHelper* first_run_helper() { return first_run_helper_.get(); }

 private:
  friend class FirstRunUIBrowserTest;

  FirstRunController();
  void Init();
  void Finalize();

  static FirstRunController* GetInstanceForTest();

  // Overriden from FirstRunActor::Delegate.
  void OnActorInitialized() override;
  void OnNextButtonClicked(const std::string& step_name) override;
  void OnHelpButtonClicked() override;
  void OnStepShown(const std::string& step_name) override;
  void OnStepHidden(const std::string& step_name) override;
  void OnActorFinalized() override;
  void OnActorDestroyed() override;

  void OnCancelled();

  void RegisterSteps();
  void ShowNextStep();
  void AdvanceStep();
  first_run::Step* GetCurrentStep() const;

  // The object providing interface to UI layer. It's not directly owned by
  // FirstRunController.
  FirstRunActor* actor_;

  std::unique_ptr<ash::FirstRunHelper> first_run_helper_;

  // List of all tutorial steps.
  std::vector<std::unique_ptr<first_run::Step>> steps_;

  // Index of step that is currently shown.
  size_t current_step_index_;

  // Profile used for webui and help app.
  Profile* user_profile_;

  // The work that should be made after actor has been finalized.
  base::Closure on_actor_finalized_;

  // Widget containing the first-run webui.
  std::unique_ptr<views::Widget> widget_;

  // Web contents of WebUI.
  content::WebContents* web_contents_for_tests_;

  // Time when tutorial was started.
  base::Time start_time_;

  DISALLOW_COPY_AND_ASSIGN(FirstRunController);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_FIRST_RUN_FIRST_RUN_CONTROLLER_H_
