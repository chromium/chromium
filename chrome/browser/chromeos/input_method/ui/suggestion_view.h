// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_INPUT_METHOD_UI_SUGGESTION_VIEW_H_
#define CHROME_BROWSER_CHROMEOS_INPUT_METHOD_UI_SUGGESTION_VIEW_H_

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "ui/chromeos/ui_chromeos_export.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/metadata/metadata_header_macros.h"
#include "ui/views/metadata/view_factory.h"
#include "ui/views/view.h"

namespace views {
class ImageView;
}  // namespace views

namespace ui {
namespace ime {

struct SuggestionDetails;

// Font-related constants
constexpr char kFontStyle[] = "Roboto";
constexpr int kSuggestionFontSize = 13;
constexpr int kAnnotationFontSize = 10;
constexpr int kIndexFontSize = 10;

// Style-related constants
constexpr int kAnnotationBorderThickness = 1;
constexpr int kAnnotationCornerRadius = 2;
constexpr int kPadding = 8;
constexpr int kAnnotationPaddingHeight = 6;
constexpr char kTabKey[] = "tab";
constexpr SkColor kConfirmedTextColor = gfx::kGoogleGrey900;
constexpr SkColor kSuggestionColor = gfx::kGoogleGrey700;
constexpr SkColor kButtonHighlightColor =
    SkColorSetA(SK_ColorBLACK, 0x0F);  // 6% Black.

// SuggestionView renders a suggestion.
class UI_CHROMEOS_EXPORT SuggestionView : public views::Button {
 public:
  METADATA_HEADER(SuggestionView);
  explicit SuggestionView(PressedCallback callback);
  SuggestionView(const SuggestionView&) = delete;
  SuggestionView& operator=(const SuggestionView&) = delete;
  ~SuggestionView() override;

  void SetView(const SuggestionDetails& details);

  void SetViewWithIndex(const std::u16string& index,
                        const std::u16string& text);

  void SetHighlighted(bool highlighted);
  void SetMinWidth(int width);

 private:
  friend class SuggestionWindowViewTest;
  FRIEND_TEST_ALL_PREFIXES(SuggestionWindowViewTest, ShortcutSettingTest);

  void Layout() override;
  gfx::Size CalculatePreferredSize() const override;
  void OnThemeChanged() override;

  std::unique_ptr<views::View> CreateAnnotationLabel();

  // Views created in the class will be part of tree of |this|, so these
  // child views will be deleted when |this| is deleted.

  void SetSuggestionText(const std::u16string& text,
                         const size_t confirmed_length);

  views::Label* index_label_ = nullptr;
  // The suggestion label renders suggestions.
  views::StyledLabel* suggestion_label_ = nullptr;
  // The annotation view renders annotations.
  views::View* annotation_label_ = nullptr;
  views::ImageView* down_icon_ = nullptr;
  views::ImageView* arrow_icon_ = nullptr;

  int suggestion_width_ = 0;
  int index_width_ = 0;
  int min_width_ = 0;
  bool highlighted_ = false;
};

BEGIN_VIEW_BUILDER(UI_CHROMEOS_EXPORT, SuggestionView, views::Button)
VIEW_BUILDER_PROPERTY(const SuggestionDetails&, View)
VIEW_BUILDER_PROPERTY(bool, Highlighted)
VIEW_BUILDER_PROPERTY(int, MinWidth)
END_VIEW_BUILDER

}  // namespace ime
}  // namespace ui

DEFINE_VIEW_BUILDER(UI_CHROMEOS_EXPORT, ui::ime::SuggestionView)

#endif  // CHROME_BROWSER_CHROMEOS_INPUT_METHOD_UI_SUGGESTION_VIEW_H_
