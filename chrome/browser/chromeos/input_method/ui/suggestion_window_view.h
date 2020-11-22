// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_INPUT_METHOD_UI_SUGGESTION_WINDOW_VIEW_H_
#define CHROME_BROWSER_CHROMEOS_INPUT_METHOD_UI_SUGGESTION_WINDOW_VIEW_H_

#include <stddef.h>

#include <memory>

#include "base/containers/flat_map.h"
#include "ui/chromeos/ui_chromeos_export.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

namespace chromeos {
struct AssistiveWindowProperties;
}  // namespace chromeos

namespace views {
class ImageButton;
class Link;
}  // namespace views

namespace ui {
namespace ime {

class AssistiveDelegate;
struct AssistiveWindowButton;
struct SuggestionDetails;
class SuggestionView;

// SuggestionWindowView is the main container of the suggestion window UI.
class UI_CHROMEOS_EXPORT SuggestionWindowView
    : public views::BubbleDialogDelegateView {
 public:
  METADATA_HEADER(SuggestionWindowView);

  // Creates a bubble widget containing a SuggestionWindowView.  Returns a
  // pointer to the contained view.
  static SuggestionWindowView* Create(gfx::NativeView parent,
                                      AssistiveDelegate* delegate);

  // views::BubbleDialogDelegateView:
  std::unique_ptr<views::NonClientFrameView> CreateNonClientFrameView(
      views::Widget* widget) override;

  void Show(const SuggestionDetails& details);

  void ShowMultipleCandidates(
      const chromeos::AssistiveWindowProperties& properties);

  // Sets |button|'s highlight state to |highlighted|. At most one button with
  // the same id will be highlighted at any given time.
  void SetButtonHighlighted(const AssistiveWindowButton& button,
                            bool highlighted);

  views::View* candidate_area_for_testing() { return candidate_area_; }
  views::Link* setting_link_for_testing() { return setting_link_; }
  views::ImageButton* learn_more_button_for_testing() {
    return learn_more_button_;
  }

 protected:
  // views::BubbleDialogDelegateView:
  void OnThemeChanged() override;

 private:
  SuggestionWindowView(gfx::NativeView parent, AssistiveDelegate* delegate);
  SuggestionWindowView(const SuggestionWindowView&) = delete;
  SuggestionWindowView& operator=(const SuggestionWindowView&) = delete;
  ~SuggestionWindowView() override;

  // Sets the number of candidates (i.e. the number of children of
  // |candidate_area_|) to |size|.
  void ResizeCandidateArea(size_t size);

  void MakeVisible();

  // Sets |candidate|'s highlight state to |highlighted|. At most one candidate
  // will be highlighted at any given time.
  void SetCandidateHighlighted(SuggestionView* candidate, bool highlighted);

  // The delegate to handle events from this class.
  AssistiveDelegate* const delegate_;

  // The view containing all the suggestions.
  views::View* candidate_area_;

  // The setting link, positioned below candidate_area_.
  // TODO(crbug/1102175): Rename setting to settings since there can be multiple
  // things to set.
  views::Link* setting_link_;

  views::ImageButton* learn_more_button_;

  // The currently-highlighted candidate, if any.
  SuggestionView* highlighted_candidate_ = nullptr;

  // TODO(crbug/1099062): Add tests for mouse hovered and pressed.
  base::flat_map<views::View*, base::CallbackListSubscription> subscriptions_;
};

}  // namespace ime
}  // namespace ui

#endif  // CHROME_BROWSER_CHROMEOS_INPUT_METHOD_UI_SUGGESTION_WINDOW_VIEW_H_
