// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_VIEWS_APP_LIST_TOAST_VIEW_H_
#define ASH_APP_LIST_VIEWS_APP_LIST_TOAST_VIEW_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/style/pill_button.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
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

    // Method for setting image model for the toast.
    Builder& SetIcon(const ui::ImageModel& icon);
    Builder& SetIconSize(int icon_size);
    Builder& SetIconBackground(bool has_icon_background);

    Builder& SetSubtitle(const std::u16string subtitle);
    Builder& SetSubtitleMultiline(bool multiline);
    Builder& SetButton(std::u16string button_text,
                       views::Button::PressedCallback button_callback);
    Builder& SetCloseButton(
        views::Button::PressedCallback close_button_callback);
    Builder& SetStyleForTabletMode(bool style_for_tablet_mode);

    Builder& SetViewDelegate(AppListViewDelegate* delegate);

   private:
    std::u16string title_;
    absl::optional<std::u16string> subtitle_;
    bool is_subtitle_multiline_ = false;
    absl::optional<std::u16string> button_text_;
    absl::optional<ui::ImageModel> icon_;
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

  void SetButton(std::u16string button_text,
                 views::Button::PressedCallback button_callback);
  void SetCloseButton(views::Button::PressedCallback close_button_callback);

  void SetIcon(const ui::ImageModel& icon);
  void SetIconSize(int icon_size);
  void SetTitle(const std::u16string title);
  void SetSubtitle(const std::u16string subtitle);
  void SetSubtitleMultiline(bool multiline);
  void UpdateInteriorMargins(const gfx::Insets& margins);

  void SetViewDelegate(AppListViewDelegate* delegate);

  // Sets whether the icon for the toast should have a background.
  void AddIconBackground();

  views::ImageView* icon() const { return icon_; }
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
    METADATA_HEADER(ToastPillButton);

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

  // The icon for the toast.
  absl::optional<ui::ImageModel> default_icon_;

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
