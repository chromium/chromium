// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_QUICK_APP_ACCESS_MODEL_H_
#define ASH_APP_LIST_QUICK_APP_ACCESS_MODEL_H_

#include <optional>
#include <string>

#include "ash/app_list/model/app_list_item_observer.h"
#include "ash/public/cpp/app_list/app_list_controller_observer.h"
#include "base/observer_list.h"
#include "base/scoped_observation.h"

namespace base {
class TimeTicks;
}  // namespace base

namespace gfx {
class ImageSkia;
class Size;
}  // namespace gfx

namespace ash {

class AppListItem;
class AppListController;

// The model which holds information on which app is currently set as the quick
// app. Shelf home buttons observe changes to this model and will show/hide the
// quick app button accordingly.
class QuickAppAccessModel : public AppListItemObserver,
                            public AppListControllerObserver {
 public:
  class Observer : public base::CheckedObserver {
   public:
    ~Observer() override = default;

    // Called when the quick app shown state changes.
    virtual void OnQuickAppShouldShowChanged(bool show_quick_app) = 0;

    // Called when the default icon for the quick app icon changes.
    virtual void OnQuickAppIconChanged() = 0;
  };
  QuickAppAccessModel();

  QuickAppAccessModel(const QuickAppAccessModel&) = delete;
  QuickAppAccessModel& operator=(const QuickAppAccessModel&) = delete;

  ~QuickAppAccessModel() override;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Set the quick app what will be shown next to home buttons in the shelf.
  // Returns true when the quick app was changed to a valid `app_id` or reset
  // using an empty `app_id`.
  bool SetQuickApp(const std::string& app_id);

  // Set the quick app as activated and update the quick app shown state.
  void SetQuickAppActivated();

  // Returns the quick app's icon as an image, sized to 'icon_size'.
  gfx::ImageSkia GetAppIcon(gfx::Size icon_size);

  // Returns the quick app's display name.
  const std::u16string GetAppName() const;

  const std::string& quick_app_id() { return quick_app_id_; }
  bool quick_app_should_show_state() { return quick_app_should_show_state_; }

 private:
  // AppListItemObserver:
  void ItemDefaultIconChanged() override;
  void ItemIconVersionChanged() override;
  void ItemBeingDestroyed() override;

  // AppListControllerObserver:
  void OnAppListVisibilityChanged(bool shown, int64_t display_id) override;

  AppListItem* GetQuickAppItem() const;

  // Checks if the should show state of the quick app has changed, and notifies
  // observers when the state does change.
  void UpdateQuickAppShouldShowState();

  // Calculates and returns whether the quick app should show.
  bool ShouldShowQuickApp();

  // Reset the quick app id and other associated variables to their default
  // values.
  void ClearQuickApp();

  // The time that the icon load is requested.
  std::optional<base::TimeTicks> icon_load_start_time_;

  base::ObserverList<Observer> observers_;

  base::ScopedObservation<AppListItem, AppListItemObserver> item_observation_{
      this};
  base::ScopedObservation<AppListController, AppListControllerObserver>
      app_list_controller_observer_{this};

  // The app id for the quick app.
  std::string quick_app_id_;

  // Whether the quick app is in a state such that it should be shown.
  bool quick_app_should_show_state_ = false;
};

}  // namespace ash

#endif  // ASH_APP_LIST_QUICK_APP_ACCESS_MODEL_H_
