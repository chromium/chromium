// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/crostini/crostini_unsupported_action_notifier.h"

#include <memory>
#include <string>
#include <tuple>

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace crostini {

using ::testing::_;
using ::testing::Bool;
using ::testing::Combine;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::Truly;

using chromeos::input_method::InputMethodDescriptor;

class MockDelegate : public CrostiniUnsupportedActionNotifier::Delegate {
 public:
  MOCK_METHOD(bool, IsInTabletMode, (), (override));
  MOCK_METHOD(bool, IsFocusedWindowCrostini, (), (override));
  MOCK_METHOD(bool, IsVirtualKeyboardVisible, (), (override));
  MOCK_METHOD(void, ShowToast, (const ash::ToastData& toast_data), (override));
  MOCK_METHOD(std::string,
              GetLocalizedDisplayName,
              (const InputMethodDescriptor& descriptor),
              (override));
  MOCK_METHOD(InputMethodDescriptor, GetCurrentInputMethod, (), (override));
  MOCK_METHOD(int, ToastTimeoutMs, (), (override));
  MOCK_METHOD(void,
              AddFocusObserver,
              (aura::client::FocusChangeObserver * observer),
              (override));
  MOCK_METHOD(void,
              RemoveFocusObserver,
              (aura::client::FocusChangeObserver * observer),
              (override));
  MOCK_METHOD(void,
              AddTabletModeObserver,
              (ash::TabletModeObserver * observer),
              (override));
  MOCK_METHOD(void,
              RemoveTabletModeObserver,
              (ash::TabletModeObserver * observer),
              (override));
  MOCK_METHOD(void,
              AddInputMethodObserver,
              (chromeos::input_method::InputMethodManager::Observer * observer),
              (override));
  MOCK_METHOD(void,
              RemoveInputMethodObserver,
              (chromeos::input_method::InputMethodManager::Observer * observer),
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

namespace {
constexpr char supported_ime_id[] =
    "_comp_ime_jkghodnilhceideoidjikpgommlajknkxkb:am:phonetic:arm";
constexpr char unsupported_ime_id[] =
    "_comp_ime_jkghodnilhceideoidjikpgommlajknkvkd_ethi";
const InputMethodDescriptor
    supported(supported_ime_id, {}, {}, {}, {}, {}, {}, {});
const InputMethodDescriptor
    unsupported(unsupported_ime_id, {}, {}, {}, {}, {}, {}, {});
}  // namespace

class CrostiniUnsupportedActionNotifierTest
    : public testing::TestWithParam<std::tuple<bool, bool, bool, bool>> {
 public:
  CrostiniUnsupportedActionNotifierTest()
      : notifier(std::make_unique<NiceMock<MockDelegate>>()) {}
  virtual ~CrostiniUnsupportedActionNotifierTest() = default;

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
  bool is_ime_unsupported() const { return std::get<3>(GetParam()); }
  InputMethodDescriptor ime_descriptor() const {
    return is_ime_unsupported() ? unsupported : supported;
  }

  static bool IsIMEToast(const ash::ToastData& data) {
    return data.id.compare(0, 3, "IME") == 0;
  }

  static bool IsVKToast(const ash::ToastData& data) {
    return data.id.compare(0, 2, "VK") == 0;
  }

  void SetExpectations(bool show_tablet_toast,
                       bool show_vk_toast,
                       bool show_ime_toast) {
    // We show the same toast for tablet mode and manually triggered virtual
    // keyboard, so showing either counts as showing both.
    int num_tablet_toasts = (show_tablet_toast || show_vk_toast) ? 1 : 0;
    int num_ime_toasts = show_ime_toast ? 1 : 0;

    EXPECT_CALL(get_delegate(), IsInTabletMode)
        .WillRepeatedly(Return(is_tablet_mode()));
    EXPECT_CALL(get_delegate(), IsVirtualKeyboardVisible)
        .WillRepeatedly(Return(is_vk_visible()));
    EXPECT_CALL(get_delegate(), IsFocusedWindowCrostini)
        .WillRepeatedly(Return(is_crostini_focused()));
    EXPECT_CALL(get_delegate(), GetCurrentInputMethod)
        .WillRepeatedly(Return(ime_descriptor()));
    ON_CALL(get_delegate(), GetLocalizedDisplayName).WillByDefault(Return(""));

    EXPECT_CALL(get_delegate(), ShowToast(Truly(IsVKToast)))
        .Times(num_tablet_toasts);
    EXPECT_CALL(get_delegate(), ShowToast(Truly(IsIMEToast)))
        .Times(num_ime_toasts);
  }
  CrostiniUnsupportedActionNotifier notifier;
};

TEST_P(CrostiniUnsupportedActionNotifierTest,
       ToastShownOnceOnlyWhenEnteringTabletMode) {
  bool show_tablet_toast = is_tablet_mode() && is_crostini_focused();
  // Since tablet and vk toasts are the same we can trigger the toast
  // OnTabletModeStarted even if not in tablet mode.
  bool show_vk_toast = is_vk_visible() && is_crostini_focused();
  bool show_ime_toast = false;

  SetExpectations(show_tablet_toast, show_vk_toast, show_ime_toast);

  notifier.OnTabletModeStarted();
  notifier.OnTabletModeStarted();
}

TEST_P(CrostiniUnsupportedActionNotifierTest,
       ToastShownOnceOnlyWhenShowingVirtualKeyboard) {
  // Since tablet and vk toasts are the same we can trigger the toast
  // OnKeyboardVisibilityChanged even if the virtual keyboard isn't visible.
  bool show_tablet_toast = is_tablet_mode() && is_crostini_focused();
  bool show_vk_toast = is_vk_visible() && is_crostini_focused();
  bool show_ime_toast = false;

  SetExpectations(show_tablet_toast, show_vk_toast, show_ime_toast);

  notifier.OnKeyboardVisibilityChanged(true);
  notifier.OnKeyboardVisibilityChanged(false);
  notifier.OnKeyboardVisibilityChanged(true);
}

TEST_P(CrostiniUnsupportedActionNotifierTest,
       ToastShownOnceOnlyWhenChangingIME) {
  bool show_tablet_toast = false;
  bool show_vk_toast = false;
  bool show_ime_toast = is_ime_unsupported() && is_crostini_focused();

  SetExpectations(show_tablet_toast, show_vk_toast, show_ime_toast);

  notifier.InputMethodChanged({}, {}, {});
  notifier.InputMethodChanged({}, {}, {});
}

TEST_P(CrostiniUnsupportedActionNotifierTest,
       ToastsShownOnceOnlyWhenFocusingCrostiniApp) {
  bool show_tablet_toast = is_tablet_mode() && is_crostini_focused();
  bool show_vk_toast = is_vk_visible() && is_crostini_focused();
  bool show_ime_toast = is_ime_unsupported() && is_crostini_focused();

  SetExpectations(show_tablet_toast, show_vk_toast, show_ime_toast);

  notifier.OnWindowFocused({}, {});
  notifier.OnWindowFocused({}, {});
}

INSTANTIATE_TEST_CASE_P(CrostiniUnsupportedActionNotifierTestCombination,
                        CrostiniUnsupportedActionNotifierTest,
                        Combine(Bool(), Bool(), Bool(), Bool()));

}  // namespace crostini
