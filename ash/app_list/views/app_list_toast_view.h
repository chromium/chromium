// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_VIEWS_APP_LIST_TOAST_VIEW_H_
#define ASH_APP_LIST_VIEWS_APP_LIST_TOAST_VIEW_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/style/pill_button.h"
#include "base/memory/raw_ptr.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/view.h"

namespace views {
class BoxLayout;
class Button;
class Label;
class LabelButton;
class ImageView;
}  // namespace views

namespace gfx {
struct VectorIcon;
}  // namespace gfx

namespace ash {

class AppListViewDelegate;

// A view specific to AppList that displays an icon, two labels and a button.
// The view can be built with all or some of the elements mentioned before, but
// always will have to include at least a title. The view has a grid-like layout
// with 3 columns where the icon will be on the first column, the button on the
// last column and the title and subtitle will share the middle column, with the
// title over the subtitle.
class ASH_EXPORT AppListToastView : public views::View {
 public:
  class Builder {
   public:
    explicit Builder(const std::u16string title);
    virtual ~Builder();

    std::unique_ptr<AppListToastView> Build();

    // Methods for setting vector icons for the toast.
    // Vector icons would change appearance with theming by default.
    // Nevertheless there might be a case when different icons need to be used
    // with dark/light mode (i.e. non-monochromatic icons) and a single icon is
    // not enough. For this case, use SetThemingIcons().
    Builder& SetIcon(const gfx::VectorIcon* icon);
    Builder& SetThemingIcons(const gfx::VectorIcon* dark_icon,
                             const gfx::VectorIcon* light_icon);
    Builder& SetIconSize(int icon_size);
    Builder& SetIconBackground(bool has_icon_background);

    Builder& SetSubtitle(const std::u16string subtitle);
    Builder& SetButton(std::u16string button_text,
                       views::Button::PressedCallback button_callback);
    Builder& SetCloseButton(
        views::Button::PressedCallback close_button_callback);
    Builder& SetStyleForTabletMode(bool style_for_tablet_mode);

    Builder& SetViewDelegate(AppListViewDelegate* delegate);

   private:
    std::u16string title_;
    absl::optional<std::u16string> subtitle_;
    absl::optional<std::u16string> button_text_;
    raw_ptr<const gfx::VectorIcon, ExperimentalAsh> icon_ = nullptr;
    raw_ptr<const gfx::VectorIcon, ExperimentalAsh> dark_icon_ = nullptr;
    raw_ptr<const gfx::VectorIcon, ExperimentalAsh> light_icon_ = nullptr;
    absl::optional<int> icon_size_;
    views::Button::PressedCallback button_callback_;
    views::Button::PressedCallback close_button_callback_;
    bool style_for_tablet_mode_ = false;
    bool has_icon_background_ = false;
    raw_ptr<AppListViewDelegate, ExperimentalAsh> view_delegate_ = nullptr;
  };

  // Whether `view` is a ToastPillButton.
  static bool IsToastButton(views::View* view);

  AppListToastView(const std::u16string title, bool style_for_tablet_mode);
  AppListToastView(const AppListToastView&) = delete;
  AppListToastView& operator=(const AppListToastView&) = delete;
  ~AppListToastView() override;

  // views::View:
  gfx::Size GetMaximumSize() const override;
  gfx::Size GetMinimumSize() const override;
  gfx::Size CalculatePreferredSize() const override;
  void OnThemeChanged() override;

  void SetButton(std::u16string button_text,
                 views::Button::PressedCallback button_callback);
  void SetCloseButton(views::Button::PressedCallback close_button_callback);

  void SetIcon(const gfx::VectorIcon* icon);
  void SetThemingIcons(const gfx::VectorIcon* dark_icon,
                       const gfx::VectorIcon* light_icon);
  void SetIconSize(int icon_size);
  void SetTitle(const std::u16string title);
  void SetSubtitle(const std::u16string subtitle);

  void UpdateInteriorMargins(const gfx::Insets& margins);

  void SetViewDelegate(AppListViewDelegate* delegate);

  // Sets whether the icon for the toast should have a background.
  void AddIconBackground();

  views::LabelButton* toast_button() const { return toast_button_; }
  views::Button* close_button() const { return close_button_; }

  // TODO(b/274524838): Sets the maximum width of the `title_label_`.
  // When any of the values in the `GetExpandedTitleLabelWidth()` changes, need
  // to recalculate the width.
  // It is possible that this view automatically recalculate the width when
  // detect any changes. But for simplicity, the caller needs to call this
  // method after set the button or icon.
  void SetTitleLabelMaximumWidth();

  views::Label* GetTitleLabelForTesting() const { return title_label_; }

 private:
  class ToastPillButton : public PillButton {
   public:
    ToastPillButton(AppListViewDelegate* view_delegate,
                    PressedCallback callback,
                    const std::u16string& text,
                    Type type,
                    const gfx::VectorIcon* icon);

    // views::View:
    void OnFocus() override;
    void OnBlur() override;

    raw_ptr<AppListViewDelegate, ExperimentalAsh> view_delegate_ = nullptr;
  };

  // Attach the icon to the toast based on theming and available icons.
  void UpdateIconImage();
  // Creates an ImageView for the icon and inserts it in the toast view.
  void CreateIconView();

  // Get the available space for `title_label_` width.
  int GetExpandedTitleLabelWidth();

  // Vector icons to use with dark/light mode.
  raw_ptr<const gfx::VectorIcon, ExperimentalAsh> dark_icon_ = nullptr;
  raw_ptr<const gfx::VectorIcon, ExperimentalAsh> light_icon_ = nullptr;

  // Vector icon to use if there are not dark or light mode specific icons.
  raw_ptr<const gfx::VectorIcon, ExperimentalAsh> default_icon_ = nullptr;

  absl::optional<int> icon_size_;

  // Whether the toast icon should be styled with a background.
  bool has_icon_background_ = false;

  raw_ptr<AppListViewDelegate, ExperimentalAsh> view_delegate_ = nullptr;

  // Toast icon view.
  raw_ptr<views::ImageView, DanglingUntriaged | ExperimentalAsh> icon_ =
      nullptr;
  // Label with the main text for the toast.
  raw_ptr<views::Label, ExperimentalAsh> title_label_ = nullptr;
  // Label with the subtext for the toast.
  raw_ptr<views::Label, ExperimentalAsh> subtitle_label_ = nullptr;
  // The button for the toast.
  raw_ptr<ToastPillButton, ExperimentalAsh> toast_button_ = nullptr;
  // The close button for the toast.
  raw_ptr<views::Button, ExperimentalAsh> close_button_ = nullptr;
  // Helper view to layout labels.
  raw_ptr<views::View, ExperimentalAsh> label_container_ = nullptr;
  // Layout manager for the view.
  raw_ptr<views::BoxLayout, ExperimentalAsh> layout_manager_ = nullptr;
};

}  // namespace ash

#endif  // ASH_APP_LIST_VIEWS_APP_LIST_TOAST_VIEW_H_
