// Copyright 2021 The Chromium Authors
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

  // Create an icon view for an app. Sets `icon_identifier_` to
  // `icon_identifier` and `count_` to `count` then based on their values
  // determines what views need to be created and starts loading the icon
  // specified by `icon_identifier`. `show_plus` indicates whether to show "+"
  // before the count for normal overflow icons, or none for all unavailable
  // icons. `on_icon_loaded` is the callback for updating the icon container.
  SavedDeskIconView(const ui::ColorProvider* incognito_window_color_provider,
                    const std::string& icon_identifier,
                    const std::string& app_title,
                    int count,
                    bool show_plus,
                    base::OnceCallback<void()> on_icon_loaded);

  // Create an icon view that only has a count and an optional plus.
  SavedDeskIconView(int count, bool show_plus);

  SavedDeskIconView(const SavedDeskIconView&) = delete;
  SavedDeskIconView& operator=(const SavedDeskIconView&) = delete;
  ~SavedDeskIconView() override;

  const std::string& icon_identifier() const { return icon_identifier_; }

  int count() const {
    DCHECK((is_overflow_icon() && count_ >= 0) ||
           (!is_overflow_icon() && count_ >= 1));
    return count_;
  }

  // Visible count. For overflow icon, this will be `count_`, for non-overflow
  // icon, this will be `count_` - 1.
  int visible_count() const {
    int visible_count = is_overflow_icon() ? count_ : count_ - 1;
    DCHECK(visible_count >= 0);
    return visible_count;
  }

  bool is_showing_default_icon() const { return is_showing_default_icon_; }

  bool is_overflow_icon() const { return icon_identifier_.empty(); }

  // Sets `count_` to `count` and updates the `count_label_`. Please note,
  // currently it does not support update on non-overflow icon.
  void UpdateCount(int count);

  // views::View:
  gfx::Size CalculatePreferredSize() const override;
  void Layout() override;
  void OnThemeChanged() override;

 private:
  friend class SavedDeskIconViewTestApi;

  // Creates the child views for this icon view. Will start the asynchronous
  // task of loading icons if necessary.
  void CreateChildViews(
      const ui::ColorProvider* incognito_window_color_provider,
      const std::string& app_title,
      bool show_plus);

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

  // True if this icon view is showing the default (fallback) icon.
  bool is_showing_default_icon_ = false;

  // Callback from the icon container that updates the icon order and overflow
  // icon.
  base::OnceCallback<void()> on_icon_loaded_;

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
