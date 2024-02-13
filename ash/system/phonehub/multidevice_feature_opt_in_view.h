// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_PHONEHUB_MULTIDEVICE_FEATURE_OPT_IN_VIEW_H_
#define ASH_SYSTEM_PHONEHUB_MULTIDEVICE_FEATURE_OPT_IN_VIEW_H_

#include "ash/ash_export.h"
#include "ash/system/phonehub/sub_feature_opt_in_view.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "chromeos/ash/components/phonehub/multidevice_feature_access_manager.h"
#include "chromeos/ash/components/phonehub/util/histogram_util.h"
#include "ui/base/metadata/metadata_header_macros.h"

namespace ash {

// An additional entry point shown on the Phone Hub bubble for the user to grant
// access or opt out for multidevice feature from the phone.
// Available multidevice features: 1. Notification 2. Camera Roll
class ASH_EXPORT MultideviceFeatureOptInView
    : public SubFeatureOptInView,
      public phonehub::MultideviceFeatureAccessManager::Observer {
  METADATA_HEADER(MultideviceFeatureOptInView, SubFeatureOptInView)

 public:
  explicit MultideviceFeatureOptInView(
      phonehub::MultideviceFeatureAccessManager*
          multidevice_feature_access_manager);

  MultideviceFeatureOptInView(const MultideviceFeatureOptInView&) = delete;
  MultideviceFeatureOptInView& operator=(const MultideviceFeatureOptInView&) =
      delete;
  ~MultideviceFeatureOptInView() override;

  // phonehub::MultideviceFeatureAccessManager::Observer:
  void OnNotificationAccessChanged() override;
  void OnCameraRollAccessChanged() override;

 private:
  void SetUpButtonPressed() override;
  void DismissButtonPressed() override;

  // Calculates whether this view should be visible and updates its visibility
  // accordingly.
  void UpdateVisibility(bool was_visible);
  void ClosePhoneHubBubble();

  raw_ptr<phonehub::MultideviceFeatureAccessManager>
      multidevice_feature_access_manager_;

  base::ScopedObservation<phonehub::MultideviceFeatureAccessManager,
                          phonehub::MultideviceFeatureAccessManager::Observer>
      access_manager_observation_{this};

  // The current setup mode.
  phonehub::util::PermissionsOnboardingSetUpMode setup_mode_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_PHONEHUB_MULTIDEVICE_FEATURE_OPT_IN_VIEW_H_
