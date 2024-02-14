// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_WINDOW_RESTORE_PINE_CONTROLLER_H_
#define ASH_WM_WINDOW_RESTORE_PINE_CONTROLLER_H_

#include "ash/ash_export.h"
#include "base/memory/weak_ptr.h"
#include "ui/gfx/image/image_skia.h"

namespace ash {

struct PineContentsData;

// Controls showing the pine dialog. Receives data from the full restore
// service.
class ASH_EXPORT PineController {
 public:
  PineController();
  PineController(const PineController&) = delete;
  PineController& operator=(const PineController&) = delete;
  ~PineController();

  PineContentsData* pine_contents_data() { return pine_contents_data_.get(); }
  const PineContentsData* pine_contents_data() const {
    return pine_contents_data_.get();
  }

  // Starts an overview session with the pine contents view if certain
  // conditions are met. Uses fake for testing only data.
  // TODO(hewer): Remove this temporary function.
  void MaybeStartPineOverviewSessionDevAccelerator();

  // Starts an overview session with the pine contents view if certain
  // conditions are met. Triggered by developer accelerator or on login.
  // `pine_contents_data` is stored in `pine_contents_data_` as we will support
  // re-entering the pine session if no windows have opened for example. It will
  // be populated with a screenshot if possible and then referenced when an
  // overview pine session is entered.
  void MaybeStartPineOverviewSession(
      std::unique_ptr<PineContentsData> pine_contents_data);

  // Ends the overview session if it is active and deletes
  // `pine_contents_data_`.
  void MaybeEndPineOverviewSession();

  // TODO(sammiequon): Entering overview normally should show the pine dialog if
  // `pine_contents_data_` is not null.

 private:
  // Callback function for when the pine image is finished decoding.
  void OnPineImageDecoded(const gfx::ImageSkia& pine_image);

  void StartPineOverviewSession();

  // Stores the data needed to display the pine dialog. Created on login, and
  // deleted after the user interacts with the dialog. If the user exits
  // overview, this will persist until a window is opened.
  // TODO(sammiequon): Delete this object when an app window is created.
  std::unique_ptr<PineContentsData> pine_contents_data_;

  base::WeakPtrFactory<PineController> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_WM_WINDOW_RESTORE_PINE_CONTROLLER_H_
