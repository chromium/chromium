// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_PHONEHUB_SUB_FEATURE_OPT_IN_VIEW_H_
#define ASH_SYSTEM_PHONEHUB_SUB_FEATURE_OPT_IN_VIEW_H_

#include "ash/ash_export.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/phonehub/phone_hub_view_ids.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "chromeos/ash/components/phonehub/util/histogram_util.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

namespace views {
class Label;
class LabelButton;
}  // namespace views

namespace ash {

using ash::phonehub::util::PermissionsOnboardingSetUpMode;

// An additional entry point shown on the Phone Hub bubble for the user to grant
// access or opt out for phone hub sub feature.
class ASH_EXPORT SubFeatureOptInView : public views::View {
  METADATA_HEADER(SubFeatureOptInView, views::View)

 public:
  SubFeatureOptInView(const SubFeatureOptInView&) = delete;
  SubFeatureOptInView& operator=(const SubFeatureOptInView&) = delete;
  ~SubFeatureOptInView() override;

 protected:
  SubFeatureOptInView(PhoneHubViewID view_id,
                      PermissionsOnboardingSetUpMode permission_setup_mode);
  void SetSetUpMode(PermissionsOnboardingSetUpMode setup_mode);

 private:
  void InitLayout();
  void SetStringIds();
  void UpdateLabels();

  virtual void SetUpButtonPressed() = 0;
  virtual void DismissButtonPressed() = 0;

  // View and string IDs
  PhoneHubViewID view_id_;
  int description_string_id_;
  int set_up_button_accessible_name_string_id_;
  int dismiss_button_accessible_name_string_id_;

  // Component state
  PermissionsOnboardingSetUpMode setup_mode_;

  // Main components of this view. Owned by view hierarchy.
  raw_ptr<views::Label> text_label_ = nullptr;
  raw_ptr<views::LabelButton> set_up_button_ = nullptr;
  raw_ptr<views::LabelButton> dismiss_button_ = nullptr;
};

}  // namespace ash

#endif  // ASH_SYSTEM_PHONEHUB_SUB_FEATURE_OPT_IN_VIEW_H_
