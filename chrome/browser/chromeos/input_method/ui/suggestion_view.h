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
  explicit SuggestionView(PressedCallback callback);
  ~SuggestionView() override;

  void SetView(const SuggestionDetails& details);

  void SetViewWithIndex(const base::string16& index,
                        const base::string16& text);

  void SetHighlighted(bool highlighted);
  void SetMinWidth(int width);

 private:
  friend class SuggestionWindowViewTest;
  FRIEND_TEST_ALL_PREFIXES(SuggestionWindowViewTest, ShortcutSettingTest);

  // Overridden from View:
  const char* GetClassName() const override;
  void Layout() override;
  gfx::Size CalculatePreferredSize() const override;
  void OnThemeChanged() override;

  std::unique_ptr<views::View> CreateAnnotationLabel();

  // Views created in the class will be part of tree of |this|, so these
  // child views will be deleted when |this| is deleted.

  void SetSuggestionText(const base::string16& text,
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

  DISALLOW_COPY_AND_ASSIGN(SuggestionView);
};

}  // namespace ime
}  // namespace ui

#endif  // CHROME_BROWSER_CHROMEOS_INPUT_METHOD_UI_SUGGESTION_VIEW_H_
