// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_PHONEHUB_CAMERA_ROLL_VIEW_H_
#define ASH_SYSTEM_PHONEHUB_CAMERA_ROLL_VIEW_H_

#include "ash/ash_export.h"
#include "ash/components/phonehub/camera_roll_manager.h"
#include "ash/system/phonehub/camera_roll_opt_in_view.h"
#include "base/gtest_prod_util.h"
#include "ui/views/view.h"
#include "ui/views/view_model.h"

namespace ash {

namespace phonehub {
class UserActionRecorder;
}

// A view in Phone Hub bubble that allows user view and access recently taken
// photos and videos from a connected device.
// Contains the header and one or more lines of clickable thumbnails.
// This view will automatically hide if no Camera Roll items are available.
class ASH_EXPORT CameraRollView : public views::View,
                                  public phonehub::CameraRollManager::Observer {
 public:
  CameraRollView(phonehub::CameraRollManager* camera_roll_manager,
                 phonehub::UserActionRecorder* user_action_recorder);
  ~CameraRollView() override;
  CameraRollView(CameraRollView&) = delete;
  CameraRollView operator=(CameraRollView&) = delete;

  // phonehub::CameraRollManager::Observer:
  void OnCameraRollViewUiStateUpdated() override;

  // views::View:
  const char* GetClassName() const override;

 protected:
  bool should_disable_annimator_timer_for_test_ = false;

 private:
  FRIEND_TEST_ALL_PREFIXES(CameraRollViewTest, DisplayOptInView);
  FRIEND_TEST_ALL_PREFIXES(CameraRollViewTest, OptInAlready);
  FRIEND_TEST_ALL_PREFIXES(CameraRollViewTest, RightAfterOptIn);
  FRIEND_TEST_ALL_PREFIXES(CameraRollViewTest, ViewLayout);

  class CameraRollItemsView : public views::View {
   public:
    CameraRollItemsView();
    ~CameraRollItemsView() override;
    CameraRollItemsView(CameraRollItemsView&) = delete;
    CameraRollItemsView operator=(CameraRollItemsView&) = delete;

    void AddCameraRollItem(views::View* camera_roll_item);
    void AddLoadingAnimatedItem(bool disable_repeated_animation_for_test);
    void Reset();

    // views::View:
    gfx::Size CalculatePreferredSize() const override;
    void Layout() override;
    const char* GetClassName() const override;

   private:
    FRIEND_TEST_ALL_PREFIXES(CameraRollViewTest, ViewLayout);

    gfx::Point GetCameraRollItemPosition(int index);
    void CalculateIdealBounds();

    views::ViewModelT<views::View> camera_roll_items_;
  };

  // Update the camera roll section to display the latest items.
  void Update();

  phonehub::CameraRollManager* camera_roll_manager_ = nullptr;
  phonehub::UserActionRecorder* user_action_recorder_ = nullptr;
  CameraRollOptInView* opt_in_view_ = nullptr;
  CameraRollItemsView* items_view_ = nullptr;
};

}  // namespace ash

#endif  // ASH_SYSTEM_PHONEHUB_CAMERA_ROLL_VIEW_H_
