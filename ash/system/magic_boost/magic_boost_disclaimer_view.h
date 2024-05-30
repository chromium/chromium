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
class UniqueWidgetPtr;
}  // namespace views

namespace ash {

// A bubble style view to show the disclaimer view.
class ASH_EXPORT MagicBoostDisclaimerView : public views::View {
  METADATA_HEADER(MagicBoostDisclaimerView, views::View)

 public:
  MagicBoostDisclaimerView();
  MagicBoostDisclaimerView(const MagicBoostDisclaimerView&) = delete;
  MagicBoostDisclaimerView& operator=(const MagicBoostDisclaimerView&) = delete;
  ~MagicBoostDisclaimerView() override;

  // Creates a widget that contains a `DisclaimerView`, shown in the middle of
  // the screen.
  static views::UniqueWidgetPtr CreateWidget();

  // Returns the host widget's name.
  static const char* GetWidgetName();

 private:
  base::WeakPtrFactory<MagicBoostDisclaimerView> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_MAGIC_BOOST_MAGIC_BOOST_DISCLAIMER_VIEW_H_
