// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_DESKS_TEMPLATES_SAVED_DESK_ICON_VIEW_H_
#define ASH_WM_DESKS_TEMPLATES_SAVED_DESK_ICON_VIEW_H_

#include <cstdint>
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

// The base class of SavedDeskRegularIconView and SavedDeskOverflowIconView.
// A class for loading and displaying the icon of apps/urls used in a
// SavedDeskItemView. Depending on the `count_` and `icon_identifier_`,
// the SavedDeskRegularIconView may have only an icon, or an icon with a count
// label; while the SavedDeskOverflowIconView has only a count label.
class SavedDeskIconView : public views::View {
 public:
  METADATA_HEADER(SavedDeskIconView);

  // Create an icon view for an app. Sets `icon_identifier_` to
  // `icon_identifier` and `count_` to `count` then based on their values
  // determines what views need to be created and starts loading the icon
  // specified by `icon_identifier`. `sorting_key` is the key that is used for
  // sorting by the icon container.
  SavedDeskIconView(const std::string& icon_identifier,
                    int count,
                    size_t sorting_key);

  SavedDeskIconView(const SavedDeskIconView&) = delete;
  SavedDeskIconView& operator=(const SavedDeskIconView&) = delete;
  ~SavedDeskIconView() override;

  // TODO(b/256224473): Remove this function and `icon_identifier_`. It seems
  // that we just use it for unit tests. We could be passing icon_identifier
  // directly from `SavedDeskRegularIconView` constructor to
  // `CreateChildViews()` and then we wouldn't need to hold on to this string.
  const std::string& icon_identifier() const { return icon_identifier_; }

  bool is_overflow_icon() const { return icon_identifier_.empty(); }

  // views::View:
  gfx::Size CalculatePreferredSize() const override;
  void Layout() override;

  // Sets `count_` to `count` and updates the `count_label_`. Please note,
  // currently it does not support update on regular icon.
  virtual void UpdateCount(int count);

  // Sorting key that is used by the container for sorting all icons. Icon with
  // higher keys will be displayed at the end in the icon container.
  // Values are designed as follows:
  //   - Non-default icon: index from its original order, starting from 0
  //   - Default icon:  `kDefaultIconSortingKey`
  //   - Overflow icon: `kOverflowIconSortingKey`
  virtual size_t GetSortingKey() const = 0;

  virtual int GetCount() const = 0;

  // The count number will be shown on a label view. For the regular icon view,
  // with or without default icon image, this should be `count_` - 1; while for
  // the overflow icon view, this should be `count_`.
  virtual int GetCountToShow() const = 0;

 protected:
  // Creates the child view for the count label.
  void CreateCountLabelChildView(bool show_plus, int inset_size);

  // The identifier for an icon. For a favicon, this will be a url. For an app,
  // this will be an app id. For an overflow icon, it'll be an empty string.
  std::string icon_identifier_;

  // The number of instances of this icon's respective app/url stored in this's
  // respective SavedDesk.
  int count_ = 0;

  // Sorting key that is used for sorting icons in the container.
  size_t sorting_key_;

  // Owned by the views hierarchy.
  views::Label* count_label_ = nullptr;

  // TODO(b/256224473): It seems like we can make `icon_view_` private in
  // `SavedDeskRegularIconView`, because only regular icons have this view.
  RoundedImageView* icon_view_ = nullptr;

 private:
  friend class SavedDeskIconViewTestApi;

  base::WeakPtrFactory<SavedDeskIconView> weak_ptr_factory_{this};
};

class SavedDeskRegularIconView : public SavedDeskIconView {
 public:
  METADATA_HEADER(SavedDeskRegularIconView);

  // `on_icon_loaded` is the callback for updating the icon container.
  SavedDeskRegularIconView(
      const ui::ColorProvider* incognito_window_color_provider,
      const std::string& icon_identifier,
      const std::string& app_title,
      int count,
      size_t sorting_key,
      base::OnceCallback<void(views::View*)> on_icon_loaded);

  SavedDeskRegularIconView(const SavedDeskRegularIconView&) = delete;
  SavedDeskRegularIconView& operator=(const SavedDeskRegularIconView&) = delete;
  ~SavedDeskRegularIconView() override;

  bool is_showing_default_icon() const { return is_showing_default_icon_; }

  // SavedDeskIconView:
  void OnThemeChanged() override;
  size_t GetSortingKey() const override;
  int GetCount() const override;
  int GetCountToShow() const override;

 private:
  // Creates the child views for this icon view. Will start the asynchronous
  // task of loading icons if necessary.
  void CreateChildViews(
      const ui::ColorProvider* incognito_window_color_provider,
      const std::string& app_title);

  // Callbacks for when the app icon/favicon has been fetched. If the result is
  // non-null/empty then we'll set this's image to the result. Otherwise, we'll
  // use a placeholder icon.
  void OnIconLoaded(const gfx::ImageSkia& icon);

  // Loads the default favicon to `icon_view_`. Should be called when we fail to
  // load an icon.
  void LoadDefaultIcon();

  // True if this icon view is showing the default (fallback) icon.
  bool is_showing_default_icon_ = false;

  // Callback from the icon container that updates the icon order and overflow
  // icon.
  base::OnceCallback<void(views::View*)> on_icon_loaded_;

  // Used for favicon loading tasks.
  base::CancelableTaskTracker cancelable_task_tracker_;

  base::WeakPtrFactory<SavedDeskRegularIconView> weak_ptr_factory_{this};
};

class SavedDeskOverflowIconView : public SavedDeskIconView {
 public:
  METADATA_HEADER(SavedDeskOverflowIconView);

  // Create an icon view that only has a count and an optional plus.
  SavedDeskOverflowIconView(int count, bool show_plus);

  SavedDeskOverflowIconView(const SavedDeskOverflowIconView&) = delete;
  SavedDeskOverflowIconView& operator=(const SavedDeskOverflowIconView&) =
      delete;
  ~SavedDeskOverflowIconView() override;

  // SavedDeskIconView:
  void UpdateCount(int count) override;
  size_t GetSortingKey() const override;
  int GetCount() const override;
  int GetCountToShow() const override;
};

}  // namespace ash

#endif  // ASH_WM_DESKS_TEMPLATES_SAVED_DESK_ICON_VIEW_H_