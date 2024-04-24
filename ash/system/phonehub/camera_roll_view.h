// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_PHONEHUB_CAMERA_ROLL_VIEW_H_
#define ASH_SYSTEM_PHONEHUB_CAMERA_ROLL_VIEW_H_

#include "ash/ash_export.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "chromeos/ash/components/phonehub/camera_roll_manager.h"
#include "ui/base/metadata/metadata_header_macros.h"
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
  METADATA_HEADER(CameraRollView, views::View)

 public:
  CameraRollView(phonehub::CameraRollManager* camera_roll_manager,
                 phonehub::UserActionRecorder* user_action_recorder);
  ~CameraRollView() override;
  CameraRollView(CameraRollView&) = delete;
  CameraRollView operator=(CameraRollView&) = delete;

  // phonehub::CameraRollManager::Observer:
  void OnCameraRollViewUiStateUpdated() override;

 private:
  friend class CameraRollViewTest;
  FRIEND_TEST_ALL_PREFIXES(CameraRollViewTest, OptInAlready);
  FRIEND_TEST_ALL_PREFIXES(CameraRollViewTest, ViewLayout);
  FRIEND_TEST_ALL_PREFIXES(CameraRollViewTest, ImageThumbnail);
  FRIEND_TEST_ALL_PREFIXES(CameraRollViewTest, VideoThumbnail);

  class CameraRollItemsView : public views::View {
    METADATA_HEADER(CameraRollItemsView, views::View)

   public:
    CameraRollItemsView();
    ~CameraRollItemsView() override;
    CameraRollItemsView(CameraRollItemsView&) = delete;
    CameraRollItemsView operator=(CameraRollItemsView&) = delete;

    void AddCameraRollItem(views::View* camera_roll_item);
    void Reset();

    // views::View:
    gfx::Size CalculatePreferredSize(
        const views::SizeBounds& available_size) const override;
    void Layout(PassKey) override;

   private:
    FRIEND_TEST_ALL_PREFIXES(CameraRollViewTest, ViewLayout);
    FRIEND_TEST_ALL_PREFIXES(CameraRollViewTest, ImageThumbnail);
    FRIEND_TEST_ALL_PREFIXES(CameraRollViewTest, VideoThumbnail);

    gfx::Point GetCameraRollItemPosition(int index);
    void CalculateIdealBounds();

    views::ViewModelT<views::View> camera_roll_items_;
  };

  // Update the camera roll section to display the latest items.
  void Update();

  raw_ptr<phonehub::CameraRollManager> camera_roll_manager_ = nullptr;
  raw_ptr<phonehub::UserActionRecorder> user_action_recorder_ = nullptr;
  raw_ptr<CameraRollItemsView> items_view_ = nullptr;
  bool content_present_metric_emitted_ = false;
};

}  // namespace ash

#endif  // ASH_SYSTEM_PHONEHUB_CAMERA_ROLL_VIEW_H_
