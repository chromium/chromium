// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_MAGIC_BOOST_MAGIC_BOOST_DISCLAIMER_VIEW_H_
#define ASH_SYSTEM_MAGIC_BOOST_MAGIC_BOOST_DISCLAIMER_VIEW_H_

#include "ash/ash_export.h"
#include "base/memory/weak_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

namespace views {

class Label;
class MdTextButton;
class StyledLabel;
class UniqueWidgetPtr;

}  // namespace views

namespace ash {

// A bubble style view to show the disclaimer view. It contains an accept button
// and a decline button, which will call the corresponding passed in callbacks
// when they are on pressed.
class ASH_EXPORT MagicBoostDisclaimerView : public views::View {
  METADATA_HEADER(MagicBoostDisclaimerView, views::View)

 public:
  MagicBoostDisclaimerView(
      base::RepeatingClosure press_accept_button_callback,
      base::RepeatingClosure press_decline_button_callback,
      base::RepeatingClosure press_terms_of_service_callback,
      base::RepeatingClosure press_learn_more_link_callback);
  MagicBoostDisclaimerView(const MagicBoostDisclaimerView&) = delete;
  MagicBoostDisclaimerView& operator=(const MagicBoostDisclaimerView&) = delete;
  ~MagicBoostDisclaimerView() override;

  // views::View:
  void RequestFocus() override;
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;

  // Creates a widget that contains a `DisclaimerView`, shown in the middle of
  // the screen.
  static views::UniqueWidgetPtr CreateWidget(
      int64_t display_id,
      base::RepeatingClosure press_accept_button_callback,
      base::RepeatingClosure press_decline_button_callback,
      base::RepeatingClosure press_terms_of_service_callback,
      base::RepeatingClosure press_learn_more_link_callback);

  // Returns the host widget's name.
  static const char* GetWidgetName();

 private:
  // Owned by the views hierarchy.
  raw_ptr<views::MdTextButton> accept_button_ = nullptr;
  raw_ptr<views::Label> title_ = nullptr;
  raw_ptr<views::StyledLabel> paragraph_one_ = nullptr;
  raw_ptr<views::StyledLabel> paragraph_two_ = nullptr;
  raw_ptr<views::StyledLabel> paragraph_three_ = nullptr;
  raw_ptr<views::StyledLabel> paragraph_four_ = nullptr;

  base::WeakPtrFactory<MagicBoostDisclaimerView> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_MAGIC_BOOST_MAGIC_BOOST_DISCLAIMER_VIEW_H_
