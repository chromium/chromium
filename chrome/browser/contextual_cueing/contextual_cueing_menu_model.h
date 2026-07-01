// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXTUAL_CUEING_CONTEXTUAL_CUEING_MENU_MODEL_H_
#define CHROME_BROWSER_CONTEXTUAL_CUEING_CONTEXTUAL_CUEING_MENU_MODEL_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/contextual_cueing/cue_target.h"
#include "ui/menus/simple_menu_model.h"

class Profile;

namespace contextual_cueing {

class ContextualCueingController;
class ContextualCueingService;

// Menu model for the contextual cueing anchored message menu.
class ContextualCueingMenuModel : public ui::SimpleMenuModel,
                                  public ui::SimpleMenuModel::Delegate {
 public:
  ContextualCueingMenuModel(
      Profile* profile,
      base::WeakPtr<ContextualCueingController> controller,
      CueTargetType cue_type,
      std::string cuj,
      CueActionData data,
      std::string cue_id);
  ContextualCueingMenuModel(const ContextualCueingMenuModel&) = delete;
  ContextualCueingMenuModel& operator=(const ContextualCueingMenuModel&) =
      delete;
  ~ContextualCueingMenuModel() override;

  // ui::SimpleMenuModel::Delegate overrides:
  void ExecuteCommand(int command_id, int event_flags) override;

 private:
  raw_ptr<Profile> profile_;
  raw_ptr<ContextualCueingService> contextual_cueing_service_;
  base::WeakPtr<ContextualCueingController> controller_;
  CueTargetType cue_type_;
  std::string cuj_;
  CueActionData data_;
  std::string cue_id_;
};

}  // namespace contextual_cueing

#endif  // CHROME_BROWSER_CONTEXTUAL_CUEING_CONTEXTUAL_CUEING_MENU_MODEL_H_
