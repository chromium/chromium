// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_PHONEHUB_SUB_FEATURE_OPT_IN_VIEW_H_
#define ASH_SYSTEM_PHONEHUB_SUB_FEATURE_OPT_IN_VIEW_H_

#include "ash/ash_export.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/phonehub/phone_hub_view_ids.h"
#include "base/scoped_observation.h"
#include "phone_hub_view_ids.h"
#include "ui/views/view.h"

namespace views {
class Label;
class LabelButton;
}  // namespace views

namespace ash {

// An additional entry point shown on the Phone Hub bubble for the user to grant
// access or opt out for phone hub sub feature.
class ASH_EXPORT SubFeatureOptInView : public views::View {
 public:
  SubFeatureOptInView(const SubFeatureOptInView&) = delete;
  SubFeatureOptInView& operator=(const SubFeatureOptInView&) = delete;
  ~SubFeatureOptInView() override;

 protected:
  SubFeatureOptInView(PhoneHubViewID view_id,
                      int description_string_id,
                      int set_up_button_string_id);
  void RefreshDescription(int description_string_id);

 private:
  void InitLayout();

  virtual void SetUpButtonPressed() = 0;
  virtual void DismissButtonPressed() = 0;

  // View and string IDs
  PhoneHubViewID view_id_;
  int description_string_id_;
  int set_up_button_string_id_;

  // Main components of this view. Owned by view hierarchy.
  views::Label* text_label_ = nullptr;
  views::LabelButton* set_up_button_ = nullptr;
  views::LabelButton* dismiss_button_ = nullptr;
};

}  // namespace ash

#endif  // ASH_SYSTEM_PHONEHUB_SUB_FEATURE_OPT_IN_VIEW_H_
