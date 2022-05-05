// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_DESKS_TEMPLATES_SAVED_DESK_ICON_VIEW_H_
#define ASH_WM_DESKS_TEMPLATES_SAVED_DESK_ICON_VIEW_H_

#include <string>

#include "base/memory/weak_ptr.h"
#include "base/task/cancelable_task_tracker.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

namespace gfx {
class ImageSkia;
}  // namespace gfx

namespace views {
class Label;
}  // namespace views

namespace ash {

class RoundedImageView;

// A class for loading and displaying the icon of apps/urls used in a
// SavedDeskItemView. Depending on the `count_` and `icon_identifier_`,
// this View may have only an icon, only a count label, or both.
class SavedDeskIconView : public views::View {
 public:
  METADATA_HEADER(SavedDeskIconView);

  SavedDeskIconView();
  SavedDeskIconView(const SavedDeskIconView&) = delete;
  SavedDeskIconView& operator=(const SavedDeskIconView&) = delete;
  ~SavedDeskIconView() override;

  // The size of the background the icon sits inside of.
  static constexpr int kIconViewSize = 28;

  const std::string& icon_identifier() const { return icon_identifier_; }

  int count() const { return count_; }

  // Sets `icon_identifier_` to `icon_identifier` and `count_` to `count` then
  // based on their values determines what views need to be created and starts
  // loading the icon specified by `icon_identifier`. `show_plus` indicates
  // whether to show "+" before the count for normal overflow icons, or none for
  // all unavailable icons.
  void SetIconIdentifierAndCount(const std::string& icon_identifier,
                                 const std::string& app_id,
                                 const std::string& app_title,
                                 int count,
                                 bool show_plus);

  // Sets `count_` to `count` and updates the `count_label_`.
  void UpdateCount(int count);

  // views::View:
  gfx::Size CalculatePreferredSize() const override;
  void Layout() override;

 private:
  friend class SavedDeskIconViewTestApi;

  // Callbacks for when the app icon/favicon has been fetched. If the result is
  // non-null/empty then we'll set this's image to the result. Otherwise, we'll
  // use a placeholder icon.
  void OnIconLoaded(const gfx::ImageSkia& icon);

  // Loads the default favicon to `icon_view_`. Should be called when we fail to
  // load an icon.
  void LoadDefaultIcon();

  // The identifier for an icon. For a favicon, this will be a url. For an app,
  // this will be an app id.
  std::string icon_identifier_;

  // The number of instances of this icon's respective app/url stored in this's
  // respective SavedDesk.
  int count_ = 0;

  // Owned by the views hierarchy.
  views::Label* count_label_ = nullptr;
  RoundedImageView* icon_view_ = nullptr;

  // Used for favicon loading tasks.
  base::CancelableTaskTracker cancelable_task_tracker_;

  base::WeakPtrFactory<SavedDeskIconView> weak_ptr_factory_{this};
};

BEGIN_VIEW_BUILDER(/* no export */, SavedDeskIconView, views::View)
END_VIEW_BUILDER

}  // namespace ash

DEFINE_VIEW_BUILDER(/* no export */, ash::SavedDeskIconView)

#endif  // ASH_WM_DESKS_TEMPLATES_SAVED_DESK_ICON_VIEW_H_
