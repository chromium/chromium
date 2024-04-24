// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_VIEWS_TOP_ICON_ANIMATION_VIEW_H_
#define ASH_APP_LIST_VIEWS_TOP_ICON_ANIMATION_VIEW_H_

#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/views/view.h"

namespace views {
class ImageView;
class Label;
}  // namespace views

namespace ash {

class AppsGridView;
class TopIconAnimationView;

// Observer for top icon animation completion.
class TopIconAnimationObserver : public base::CheckedObserver {
 public:
  // Called when top icon animation completes.
  virtual void OnTopIconAnimationsComplete(TopIconAnimationView* view) {}

 protected:
  ~TopIconAnimationObserver() override = default;
};

// Transitional view used for top item icons animation when opening or closing
// a folder. Owns itself.
class TopIconAnimationView : public views::View,
                             public ui::ImplicitAnimationObserver {
  METADATA_HEADER(TopIconAnimationView, views::View)

 public:
  // |grid|: The apps grid to which the icon animation view belongs.
  // |icon|: The icon image of the item icon of full scale size.
  // |badge_icon|: The icon image of the item's badge icon of full scale size.
  //    May be null if the app item is not badged - currently existence of the
  //    badge implies an app shortcut item.
  // |title|: The title of the item.
  // |scaled_rect|: Bounds of the small icon inside folder icon.
  // |open_folder|: Specify open/close folder animation to perform.
  // |item_in_folder_icon|: True if the item is inside folder icon.
  // The view will be self-cleaned by the end of animation.
  TopIconAnimationView(AppsGridView* grid,
                       const gfx::ImageSkia& icon,
                       const gfx::ImageSkia& badge_icon,
                       const std::u16string& title,
                       const gfx::Rect& scaled_rect,
                       bool open_folder,
                       bool item_in_folder_icon);

  TopIconAnimationView(const TopIconAnimationView&) = delete;
  TopIconAnimationView& operator=(const TopIconAnimationView&) = delete;

  ~TopIconAnimationView() override;

  void AddObserver(TopIconAnimationObserver* observer);
  void RemoveObserver(TopIconAnimationObserver* observer);

  // When opening a folder, transform the top item icon from the small icon
  // inside folder icon to the full scale icon at the target location.
  // When closing a folder, transform the full scale item icon from its
  // location to the small icon inside the folder icon.
  void TransformView(base::TimeDelta duration);

 private:
  // views::View overrides:
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;
  void Layout(PassKey) override;
  void OnThemeChanged() override;

  // ui::ImplicitAnimationObserver overrides:
  void OnImplicitAnimationsCompleted() override;
  bool RequiresNotificationWhenAnimatorDestroyed() const override;

  raw_ptr<const AppsGridView, DanglingUntriaged>
      grid_;  // Owned by views hierarchy.
  gfx::Size icon_size_;
  raw_ptr<views::View> icon_background_;
  raw_ptr<views::ImageView> icon_;
  raw_ptr<views::View> badge_container_;
  raw_ptr<views::Label> title_;
  // Rect of the scaled down top item icon inside folder icon's ink bubble.
  gfx::Rect scaled_rect_;
  // true: opening folder; false: closing folder.
  bool open_folder_;

  bool item_in_folder_icon_;

  base::ObserverList<TopIconAnimationObserver> observers_;
};

}  // namespace ash

#endif  // ASH_APP_LIST_VIEWS_TOP_ICON_ANIMATION_VIEW_H_
