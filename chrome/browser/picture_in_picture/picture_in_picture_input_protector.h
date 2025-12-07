// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PICTURE_IN_PICTURE_PICTURE_IN_PICTURE_INPUT_PROTECTOR_H_
#define CHROME_BROWSER_PICTURE_IN_PICTURE_PICTURE_IN_PICTURE_INPUT_PROTECTOR_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/picture_in_picture/picture_in_picture_occlusion_observer.h"
#include "chrome/browser/picture_in_picture/scoped_picture_in_picture_occlusion_observation.h"
#include "ui/views/widget/widget_observer.h"

namespace views {
class Widget;
class DialogDelegate;
}  // namespace views

// PictureInPictureInputProtector is a helper class that can be used to protect
// a Widget from being interacted with when it is occluded by a
// picture-in-picture window. This protection prevents clickjacking attacks,
// where a picture-in-picture window could be maliciously placed over a security
// sensitive UI for exploitation.
class PictureInPictureInputProtector : public PictureInPictureOcclusionObserver,
                                       public views::WidgetObserver {
 public:
  explicit PictureInPictureInputProtector(
      views::DialogDelegate* dialog_delegate);

  PictureInPictureInputProtector(const PictureInPictureInputProtector&) =
      delete;
  PictureInPictureInputProtector& operator=(
      const PictureInPictureInputProtector&) = delete;
  ~PictureInPictureInputProtector() override;

  // WidgetObserver:
  void OnWidgetDestroying(views::Widget* widget) override;

  // Returns true whenever the `dialog_delegate_` Widget is occluded by
  // Picture-in-Picture windows, false otherwise.
  bool OccludedByPictureInPicture() const;

  // Simulates Picture-in-Picture occlussion changed for testing.
  void SimulateOcclusionStateChangedForTesting(bool occluded);

 private:
  // PictureInPictureOcclusionObserver:
  void OnOcclusionStateChanged(bool occluded) override;

  ScopedPictureInPictureOcclusionObservation occlusion_observation_{this};
  base::ScopedObservation<views::Widget, views::WidgetObserver>
      widget_observation_{this};
  bool occluded_by_picture_in_picture_ = false;
  raw_ptr<views::DialogDelegate> dialog_delegate_;
};

#endif  // CHROME_BROWSER_PICTURE_IN_PICTURE_PICTURE_IN_PICTURE_INPUT_PROTECTOR_H_
