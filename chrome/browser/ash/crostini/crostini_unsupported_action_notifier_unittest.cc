// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crostini/crostini_unsupported_action_notifier.h"

#include <memory>
#include <string>
#include <tuple>

#include "ash/constants/ash_features.h"
#include "base/test/scoped_feature_list.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/display/tablet_state.h"

namespace crostini {

using ::testing::_;
using ::testing::Bool;
using ::testing::Combine;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::Truly;

class MockDelegate : public CrostiniUnsupportedActionNotifier::Delegate {
 public:
  MOCK_METHOD(bool, IsInTabletMode, (), (override));
  MOCK_METHOD(bool, IsFocusedWindowCrostini, (), (override));
  MOCK_METHOD(bool, IsVirtualKeyboardVisible, (), (override));
  MOCK_METHOD(void, ShowToast, (ash::ToastData toast_data), (override));
  MOCK_METHOD(base::TimeDelta, ToastTimeout, (), (override));
  MOCK_METHOD(void,
              AddFocusObserver,
              (aura::client::FocusChangeObserver * observer),
              (override));
  MOCK_METHOD(void,
              RemoveFocusObserver,
              (aura::client::FocusChangeObserver * observer),
              (override));
  MOCK_METHOD(void,
              AddDisplayObserver,
              (display::DisplayObserver * observer),
              (override));
  MOCK_METHOD(void,
              RemoveDisplayObserver,
              (display::DisplayObserver * observer),
              (override));
  MOCK_METHOD(void,
              AddKeyboardControllerObserver,
              (ash::KeyboardControllerObserver * observer),
              (override));
  MOCK_METHOD(void,
              RemoveKeyboardControllerObserver,
              (ash::KeyboardControllerObserver * observer),
              (override));
};

class CrostiniUnsupportedActionNotifierTest
    : public testing::TestWithParam<std::tuple<bool, bool, bool, bool>> {
 public:
  CrostiniUnsupportedActionNotifierTest()
      : notifier(std::make_unique<NiceMock<MockDelegate>>()) {}
  ~CrostiniUnsupportedActionNotifierTest() override = default;

  MockDelegate& get_delegate() {
    auto* ptr = notifier.get_delegate_for_testing();
    DCHECK(ptr);

    // Our delegate is always a mock delegate in these tests, but we don't have
    // RTTI so have to use a static cast.
    return static_cast<NiceMock<MockDelegate>&>(*ptr);
  }

  bool is_tablet_mode() const { return std::get<0>(GetParam()); }
  bool is_crostini_focused() const { return std::get<1>(GetParam()); }
  bool is_vk_visible() const { return std::get<2>(GetParam()); }

  static bool IsVKToast(const ash::ToastData& data) {
    return data.id.compare(0, 2, "VK") == 0;
  }

  void SetExpectations(bool show_tablet_toast, bool show_vk_toast) {
    // We show the same toast for tablet mode and manually triggered virtual
    // keyboard, so showing either counts as showing both.
    int num_tablet_toasts = (show_tablet_toast || show_vk_toast) ? 1 : 0;

    EXPECT_CALL(get_delegate(), IsInTabletMode)
        .WillRepeatedly(Return(is_tablet_mode()));
    EXPECT_CALL(get_delegate(), IsVirtualKeyboardVisible)
        .WillRepeatedly(Return(is_vk_visible()));
    EXPECT_CALL(get_delegate(), IsFocusedWindowCrostini)
        .WillRepeatedly(Return(is_crostini_focused()));

    EXPECT_CALL(get_delegate(), ShowToast(Truly(IsVKToast)))
        .Times(num_tablet_toasts);
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  CrostiniUnsupportedActionNotifier notifier;
};

TEST_P(CrostiniUnsupportedActionNotifierTest,
       ToastShownOnceOnlyWhenEnteringTabletMode) {
  bool show_tablet_toast = is_tablet_mode() && is_crostini_focused();
  // Since tablet and vk toasts are the same we can trigger the toast
  // OnTabletModeStarted even if not in tablet mode.
  bool show_vk_toast = is_vk_visible() && is_crostini_focused();

  SetExpectations(show_tablet_toast, show_vk_toast);

  notifier.OnDisplayTabletStateChanged(display::TabletState::kInTabletMode);
  notifier.OnDisplayTabletStateChanged(display::TabletState::kInTabletMode);
}

TEST_P(CrostiniUnsupportedActionNotifierTest,
       ToastShownOnceOnlyWhenShowingVirtualKeyboard) {
  // Since tablet and vk toasts are the same we can trigger the toast
  // OnKeyboardVisibilityChanged even if the virtual keyboard isn't visible.
  bool show_tablet_toast = is_tablet_mode() && is_crostini_focused();
  bool show_vk_toast = is_vk_visible() && is_crostini_focused();

  SetExpectations(show_tablet_toast, show_vk_toast);

  notifier.OnKeyboardVisibilityChanged(true);
  notifier.OnKeyboardVisibilityChanged(false);
  notifier.OnKeyboardVisibilityChanged(true);
}

TEST_P(CrostiniUnsupportedActionNotifierTest,
       ToastsShownOnceOnlyWhenFocusingCrostiniApp) {
  bool show_tablet_toast = is_tablet_mode() && is_crostini_focused();
  bool show_vk_toast = is_vk_visible() && is_crostini_focused();

  SetExpectations(show_tablet_toast, show_vk_toast);

  notifier.OnWindowFocused({}, {});
  notifier.OnWindowFocused({}, {});
}

INSTANTIATE_TEST_SUITE_P(CrostiniUnsupportedActionNotifierTestCombination,
                         CrostiniUnsupportedActionNotifierTest,
                         Combine(Bool(), Bool(), Bool(), Bool()));

}  // namespace crostini
