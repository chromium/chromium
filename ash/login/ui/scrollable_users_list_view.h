// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_LOGIN_UI_SCROLLABLE_USERS_LIST_VIEW_H_
#define ASH_LOGIN_UI_SCROLLABLE_USERS_LIST_VIEW_H_

#include <vector>

#include "ash/ash_export.h"
#include "ash/login/ui/login_display_style.h"
#include "ash/login/ui/login_user_view.h"
#include "ash/public/cpp/wallpaper/wallpaper_controller.h"
#include "ash/public/cpp/wallpaper/wallpaper_controller_observer.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/scroll_view.h"

namespace views {
class View;
class BoxLayout;
}  // namespace views

namespace ash {

// Scrollable list of the users. Stores the list of login user views. Can be
// styled with GradientParams that define gradient tinting at the top and at the
// bottom. Can be styled with LayoutParams that define spacing and sizing.
class ASH_EXPORT ScrollableUsersListView : public views::ScrollView,
                                           public WallpaperControllerObserver {
  METADATA_HEADER(ScrollableUsersListView, views::ScrollView)

 public:
  // TestApi is used for tests to get internal implementation details.
  class ASH_EXPORT TestApi {
   public:
    explicit TestApi(ScrollableUsersListView* view);
    ~TestApi();

    const std::vector<raw_ptr<LoginUserView, VectorExperimental>>& user_views()
        const;

   private:
    const raw_ptr<ScrollableUsersListView> view_;
  };

  // TODO(jdufault): Pass AccountId or LoginUserView* instead of index.
  using ActionWithUser = base::RepeatingCallback<void(int)>;

  // Initializes users list with rows for all |users|. The |display_style| is
  // used to determine layout and sizings. |on_user_view_tap| callback is
  // invoked whenever user row is tapped.
  ScrollableUsersListView(const std::vector<LoginUserInfo>& users,
                          const ActionWithUser& on_tap_user,
                          LoginDisplayStyle display_style);

  ScrollableUsersListView(const ScrollableUsersListView&) = delete;
  ScrollableUsersListView& operator=(const ScrollableUsersListView&) = delete;

  ~ScrollableUsersListView() override;

  // Returns user view at |index| if it exists or nullptr otherwise.
  int user_count() const { return static_cast<int>(user_views_.size()); }
  LoginUserView* user_view_at(int index) {
    DCHECK_GE(index, 0);
    DCHECK_LT(index, user_count());
    return user_views_[index];
  }

  // Returns user view with |account_id| if it exists or nullptr otherwise.
  LoginUserView* GetUserView(const AccountId& account_id);

  // Updates the insets for the `user_view_host_layout_` based on whether the
  // view is in landscape or portrait mode.
  void UpdateUserViewHostLayoutInsets();

  // views::View:
  void Layout(PassKey) override;
  void OnPaintBackground(gfx::Canvas* canvas) override;
  void OnThemeChanged() override;

  // WallpaperControllerObserver:
  void OnWallpaperColorsChanged() override;
  void OnWallpaperBlurChanged() override;

 private:
  struct GradientParams {
    static GradientParams BuildForStyle(LoginDisplayStyle style,
                                        views::View* view);

    // Start color for drawing linear gradient.
    SkColor color_from = SK_ColorTRANSPARENT;
    // End color for drawing linear gradient.
    SkColor color_to = SK_ColorTRANSPARENT;
    // Height of linear gradient.
    SkScalar height = 0;
  };

  // Display style to determine layout and sizing of users list.
  const LoginDisplayStyle display_style_;

  // The view which contains all of the user views.
  raw_ptr<views::View> user_view_host_ = nullptr;

  // Layout for |user_view_host_|.
  raw_ptr<views::BoxLayout> user_view_host_layout_ = nullptr;

  std::vector<raw_ptr<LoginUserView, VectorExperimental>> user_views_;

  GradientParams gradient_params_;

  base::ScopedObservation<WallpaperController, WallpaperControllerObserver>
      observation_{this};
};

}  // namespace ash

#endif  // ASH_LOGIN_UI_SCROLLABLE_USERS_LIST_VIEW_H_
