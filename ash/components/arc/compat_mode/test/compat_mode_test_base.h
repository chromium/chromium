// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_ARC_COMPAT_MODE_TEST_COMPAT_MODE_TEST_BASE_H_
#define ASH_COMPONENTS_ARC_COMPAT_MODE_TEST_COMPAT_MODE_TEST_BASE_H_

#include <memory>
#include <optional>
#include <string>

#include "ash/components/arc/compat_mode/arc_resize_lock_pref_delegate.h"
#include "ui/display/test/test_screen.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace arc {

class CompatModeTestBase : public views::ViewsTestBase {
 public:
  CompatModeTestBase();
  ~CompatModeTestBase() override;

  // test::Test:
  void SetUp() override;
  void TearDown() override;

  std::unique_ptr<views::Widget> CreateWidget(bool show = true);
  std::unique_ptr<views::Widget> CreateArcWidget(
      std::optional<std::string> app_id,
      bool show = true);

  void SetDisplayWorkArea(const gfx::Rect& work_area);

  void LeftClickOnView(const views::Widget* widget,
                       const views::View* view) const;

  // Emulates the round-trip between Android and Chrome.
  void SyncResizeLockPropertyWithMojoState(const views::Widget* widget);

  ArcResizeLockPrefDelegate* pref_delegate() { return pref_delegate_.get(); }

 private:
  std::unique_ptr<ArcResizeLockPrefDelegate> pref_delegate_;
  display::test::TestScreen test_screen_;
};

}  // namespace arc

#endif  // ASH_COMPONENTS_ARC_COMPAT_MODE_TEST_COMPAT_MODE_TEST_BASE_H_
