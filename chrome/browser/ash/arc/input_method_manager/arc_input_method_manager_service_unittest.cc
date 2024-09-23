// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/input_method_manager/arc_input_method_manager_service.h"

#include <memory>
#include <tuple>
#include <utility>
#include <vector>

#include "ash/components/arc/session/arc_service_manager.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/public/cpp/keyboard/arc/arc_input_method_bounds_tracker.h"
#include "ash/public/cpp/keyboard/keyboard_switches.h"
#include "ash/wm/tablet_mode/tablet_mode_controller_test_api.h"
#include "base/containers/contains.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_command_line.h"
#include "chrome/browser/ash/arc/input_method_manager/test_input_method_manager_bridge.h"
#include "chrome/browser/ui/ash/keyboard/chrome_keyboard_controller_client_test_helper.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ui/base/app_types.h"
#include "chromeos/ui/base/window_properties.h"
#include "components/crx_file/id_util.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/test/test_windows.h"
#include "ui/base/ime/ash/extension_ime_util.h"
#include "ui/base/ime/ash/ime_bridge.h"
#include "ui/base/ime/ash/mock_ime_input_context_handler.h"
#include "ui/base/ime/ash/mock_input_method_manager.h"
#include "ui/base/ime/dummy_text_input_client.h"
#include "ui/base/ime/mock_input_method.h"
#include "ui/display/test/test_screen.h"
#include "ui/views/widget/widget.h"

namespace arc {
namespace {

namespace im = ::ash::input_method;
using ::ash::AccessibilityNotificationType;

mojom::ImeInfoPtr GenerateImeInfo(const std::string& id,
                                  const std::string& name,
                                  const std::string& url,
                                  bool enabled,
                                  bool always_allowed) {
  mojom::ImeInfoPtr info = mojom::ImeInfo::New();
  info->ime_id = id;
  info->display_name = name;
  info->settings_url = url;
  info->enabled = enabled;
  info->is_allowed_in_clamshell_mode = always_allowed;
  return info;
}

class FakeInputMethodBoundsObserver
    : public ArcInputMethodManagerService::Observer {
 public:
  FakeInputMethodBoundsObserver() = default;
  FakeInputMethodBoundsObserver(const FakeInputMethodBoundsObserver&) = delete;
  ~FakeInputMethodBoundsObserver() override = default;

  void Reset() {
    last_visibility_ = false;
    visibility_changed_call_count_ = 0;
  }

  bool last_visibility() const { return last_visibility_; }

  int visibility_changed_call_count() const {
    return visibility_changed_call_count_;
  }

  // ArcInputMethodManagerService::Observer:
  void OnAndroidVirtualKeyboardVisibilityChanged(bool visible) override {
    last_visibility_ = visible;
    ++visibility_changed_call_count_;
  }

 private:
  bool last_visibility_ = false;
  int visibility_changed_call_count_ = 0;
};

// The fake im::InputMethodManager for testing.
class TestInputMethodManager : public im::MockInputMethodManager {
 public:
  // The fake im::InputMethodManager::State implementation for testing.
  class TestState : public im::MockInputMethodManager::State {
   public:
    TestState()
        : added_input_method_extensions_(), enabled_input_method_ids_() {}

    const std::vector<std::string>& GetEnabledInputMethodIds() const override {
      return enabled_input_method_ids_;
    }

    im::InputMethodDescriptor GetCurrentInputMethod() const override {
      im::InputMethodDescriptor descriptor(
          current_ime_id_, "", "", "", std::vector<std::string>(),
          false /* is_login_keyboard */, GURL(), GURL(),
          /*handwriting_language=*/std::nullopt);
      return descriptor;
    }

    void AddInputMethodExtension(const std::string& extension_id,
                                 const im::InputMethodDescriptors& descriptors,
                                 ash::TextInputMethod* instance) override {
      added_input_method_extensions_.push_back(
          std::make_tuple(extension_id, descriptors, instance));
    }

    void RemoveInputMethodExtension(const std::string& extension_id) override {
      removed_input_method_extensions_.push_back(extension_id);
    }

    bool EnableInputMethod(
        const std::string& new_enabled_input_method_id) override {
      enabled_input_methods_.push_back(new_enabled_input_method_id);
      return true;
    }

    void AddEnabledInputMethodId(const std::string& ime_id) {
      if (!base::Contains(enabled_input_method_ids_, ime_id)) {
        enabled_input_method_ids_.push_back(ime_id);
      }
    }

    void RemoveEnabledInputMethodId(const std::string& ime_id) {
      std::erase_if(enabled_input_method_ids_,
                    [&ime_id](const std::string& id) { return id == ime_id; });
    }

    void SetCurrentInputMethod(const std::string& ime_id) {
      current_ime_id_ = ime_id;
    }

    void GetInputMethodExtensions(
        im::InputMethodDescriptors* descriptors) override {
      for (const auto& id : enabled_input_method_ids_) {
        descriptors->push_back(
            im::InputMethodDescriptor(id, "", "", {}, {}, false, GURL(), GURL(),
                                      /*handwriting_language=*/std::nullopt));
      }
    }

    void Reset() {
      added_input_method_extensions_.clear();
      removed_input_method_extensions_.clear();
      enabled_input_methods_.clear();
    }

    std::vector<std::tuple<std::string,
                           im::InputMethodDescriptors,
                           ash::TextInputMethod*>>
        added_input_method_extensions_;
    std::vector<std::string> removed_input_method_extensions_;
    std::vector<std::string> enabled_input_methods_;

   protected:
    friend base::RefCounted<InputMethodManager::State>;
    ~TestState() override = default;

   private:
    std::vector<std::string> enabled_input_method_ids_;
    std::string current_ime_id_;
  };

  TestInputMethodManager() {
    state_ = scoped_refptr<TestState>(new TestState());
  }

  TestInputMethodManager(const TestInputMethodManager&) = delete;
  TestInputMethodManager& operator=(const TestInputMethodManager&) = delete;

  ~TestInputMethodManager() override = default;

  scoped_refptr<InputMethodManager::State> GetActiveIMEState() override {
    return state_;
  }

  TestState* state() { return state_.get(); }

 private:
  scoped_refptr<TestState> state_;
};

class TestIMEInputContextHandler : public ash::MockIMEInputContextHandler {
 public:
  explicit TestIMEInputContextHandler(ui::InputMethod* input_method)
      : input_method_(input_method) {}

  TestIMEInputContextHandler(const TestIMEInputContextHandler&) = delete;
  TestIMEInputContextHandler& operator=(const TestIMEInputContextHandler&) =
      delete;

  ui::InputMethod* GetInputMethod() override { return input_method_; }

 private:
  const raw_ptr<ui::InputMethod> input_method_;
};

class TestWindowDelegate : public ArcInputMethodManagerService::WindowDelegate {
 public:
  TestWindowDelegate() = default;
  ~TestWindowDelegate() override = default;

  aura::Window* GetFocusedWindow() const override {
    return focused_;
    ;
  }

  aura::Window* GetActiveWindow() const override { return active_; }

  void SetFocusedWindow(aura::Window* window) { focused_ = window; }

  void SetActiveWindow(aura::Window* window) { active_ = window; }

 private:
  raw_ptr<aura::Window, DanglingUntriaged> focused_ = nullptr;
  raw_ptr<aura::Window, DanglingUntriaged> active_ = nullptr;
};

class ArcInputMethodManagerServiceTest : public testing::Test {
 protected:
  ArcInputMethodManagerServiceTest()
      : arc_service_manager_(std::make_unique<ArcServiceManager>()) {}

  ArcInputMethodManagerServiceTest(const ArcInputMethodManagerServiceTest&) =
      delete;
  ArcInputMethodManagerServiceTest& operator=(
      const ArcInputMethodManagerServiceTest&) = delete;

  ~ArcInputMethodManagerServiceTest() override = default;

  ArcInputMethodManagerService* service() { return service_; }

  TestInputMethodManagerBridge* bridge() { return test_bridge_; }

  TestInputMethodManager* imm() { return input_method_manager_; }

  TestingProfile* profile() { return profile_.get(); }

  TestWindowDelegate* window_delegate() { return window_delegate_; }

  void ToggleTabletMode(bool enabled) {
    auto state = enabled ? display::TabletState::kInTabletMode
                         : display::TabletState::kInClamshellMode;
    display::Screen::GetScreen()->OverrideTabletStateForTesting(state);
  }

  void NotifyNewBounds(const gfx::Rect& bounds) {
    input_method_bounds_tracker_->NotifyArcInputMethodBoundsChanged(bounds);
  }

  std::vector<std::string> GetEnabledInputMethodIds() {
    return base::SplitString(
        profile()->GetPrefs()->GetString(prefs::kLanguageEnabledImes), ",",
        base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  }

  aura::Window* CreateTestArcWindow() {
    auto* window = aura::test::CreateTestWindowWithId(1, nullptr);
    window->SetProperty(aura::client::kSkipImeProcessing, true);
    window->SetProperty(chromeos::kAppTypeKey, chromeos::AppType::ARC_APP);
    return window;
  }

  void SetUp() override {
    input_method_manager_ = new TestInputMethodManager();
    im::InputMethodManager::Initialize(input_method_manager_);
    profile_ = std::make_unique<TestingProfile>();

    input_method_bounds_tracker_ =
        std::make_unique<ash::ArcInputMethodBoundsTracker>();

    chrome_keyboard_controller_client_test_helper_ =
        ChromeKeyboardControllerClientTestHelper::InitializeWithFake();
    chrome_keyboard_controller_client_test_helper_->SetProfile(profile_.get());

    service_ = ArcInputMethodManagerService::GetForBrowserContextForTesting(
        profile_.get());
    test_bridge_ = new TestInputMethodManagerBridge();
    service_->SetInputMethodManagerBridgeForTesting(
        base::WrapUnique(test_bridge_.get()));
    window_delegate_ = new TestWindowDelegate();
    service_->SetWindowDelegateForTesting(
        base::WrapUnique(window_delegate_.get()));
  }

  void TearDown() override {
    test_bridge_ = nullptr;
    service_->Shutdown();
    chrome_keyboard_controller_client_test_helper_.reset();
    input_method_bounds_tracker_.reset();
    profile_.reset();
    im::InputMethodManager::Shutdown();
  }

 private:
  content::BrowserTaskEnvironment task_environment_;

  display::test::TestScreen test_screen_{/*create_dispay=*/true,
                                         /*register_screen=*/true};
  std::unique_ptr<ArcServiceManager> arc_service_manager_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<ChromeKeyboardControllerClientTestHelper>
      chrome_keyboard_controller_client_test_helper_;
  std::unique_ptr<ash::ArcInputMethodBoundsTracker>
      input_method_bounds_tracker_;
  raw_ptr<TestInputMethodManager, DanglingUntriaged> input_method_manager_ =
      nullptr;
  raw_ptr<TestInputMethodManagerBridge> test_bridge_ =
      nullptr;  // Owned by |service_|
  raw_ptr<ArcInputMethodManagerService, DanglingUntriaged> service_ = nullptr;
  raw_ptr<TestWindowDelegate, DanglingUntriaged> window_delegate_ = nullptr;
};

}  // anonymous namespace

TEST_F(ArcInputMethodManagerServiceTest, EnableIme) {
  namespace aeiu = ::ash::extension_ime_util;
  using crx_file::id_util::GenerateId;

  ToggleTabletMode(true);

  ASSERT_EQ(0u, bridge()->enable_ime_calls_.size());

  const std::string extension_ime_id =
      aeiu::GetInputMethodID(GenerateId("test.extension.ime"), "us");
  const std::string component_extension_ime_id =
      aeiu::GetComponentInputMethodID(
          GenerateId("test.component.extension.ime"), "us");
  const std::string arc_ime_id =
      aeiu::GetArcInputMethodID(GenerateId("test.arc.ime"), "us");

  // EnableIme is called only when ARC IME is enable or disabled.
  imm()->state()->AddEnabledInputMethodId(extension_ime_id);
  service()->ImeMenuListChanged();
  EXPECT_EQ(0u, bridge()->enable_ime_calls_.size());

  imm()->state()->AddEnabledInputMethodId(component_extension_ime_id);
  service()->ImeMenuListChanged();
  EXPECT_EQ(0u, bridge()->enable_ime_calls_.size());

  // Enable the ARC IME and verify that EnableIme is called.
  imm()->state()->AddEnabledInputMethodId(arc_ime_id);
  service()->ImeMenuListChanged();
  ASSERT_EQ(1u, bridge()->enable_ime_calls_.size());
  EXPECT_EQ(aeiu::GetComponentIDByInputMethodID(arc_ime_id),
            std::get<std::string>(bridge()->enable_ime_calls_[0]));
  EXPECT_TRUE(std::get<bool>(bridge()->enable_ime_calls_[0]));

  // Disable the ARC IME and verify that EnableIme is called with false.
  imm()->state()->RemoveEnabledInputMethodId(arc_ime_id);
  service()->ImeMenuListChanged();
  ASSERT_EQ(2u, bridge()->enable_ime_calls_.size());
  EXPECT_EQ(aeiu::GetComponentIDByInputMethodID(arc_ime_id),
            std::get<std::string>(bridge()->enable_ime_calls_[1]));
  EXPECT_FALSE(std::get<bool>(bridge()->enable_ime_calls_[1]));

  // EnableIme is not called when non ARC IME is disabled.
  imm()->state()->RemoveEnabledInputMethodId(extension_ime_id);
  service()->ImeMenuListChanged();
  EXPECT_EQ(2u, bridge()->enable_ime_calls_.size());
}

TEST_F(ArcInputMethodManagerServiceTest, EnableIme_WithPrefs) {
  namespace aeiu = ::ash::extension_ime_util;
  using crx_file::id_util::GenerateId;

  ToggleTabletMode(true);

  ASSERT_EQ(0u, bridge()->enable_ime_calls_.size());

  const std::string component_extension_ime_id =
      aeiu::GetComponentInputMethodID(
          GenerateId("test.component.extension.ime"), "us");
  const std::string arc_ime_id =
      aeiu::GetArcInputMethodID(GenerateId("test.arc.ime"), "us");

  imm()->state()->AddEnabledInputMethodId(component_extension_ime_id);
  service()->ImeMenuListChanged();
  EXPECT_EQ(0u, bridge()->enable_ime_calls_.size());

  imm()->state()->AddEnabledInputMethodId(arc_ime_id);
  service()->ImeMenuListChanged();
  ASSERT_EQ(1u, bridge()->enable_ime_calls_.size());

  // Test the case where |arc_ime_id| is temporarily disallowed because of the
  // toggling to the laptop mode. In that case, the prefs still have the IME's
  // ID.
  profile()->GetPrefs()->SetString(
      prefs::kLanguageEnabledImes,
      base::StringPrintf("%s,%s", component_extension_ime_id.c_str(),
                         arc_ime_id.c_str()));
  imm()->state()->RemoveEnabledInputMethodId(arc_ime_id);
  service()->ImeMenuListChanged();
  // Verify that EnableIme(id, false) is NOT called.
  EXPECT_EQ(1u, bridge()->enable_ime_calls_.size());  // still 1u, not 2u.
}

TEST_F(ArcInputMethodManagerServiceTest, SwitchImeTo) {
  namespace aeiu = ::ash::extension_ime_util;
  using crx_file::id_util::GenerateId;

  const std::string arc_ime_service_id =
      "org.chromium.arc.ime/.ArcInputMethodService";

  ToggleTabletMode(true);

  bridge()->switch_ime_to_calls_.clear();

  const std::string extension_ime_id =
      aeiu::GetInputMethodID(GenerateId("test.extension.ime"), "us");
  const std::string component_extension_ime_id =
      aeiu::GetComponentInputMethodID(
          GenerateId("test.component.extension.ime"), "us");
  const std::string arc_ime_id = aeiu::GetArcInputMethodID(
      GenerateId("test.arc.ime"), "ime.id.in.arc.container");

  // Set current (active) input method to the extension ime.
  imm()->state()->SetCurrentInputMethod(extension_ime_id);
  service()->InputMethodChanged(imm(), nullptr, false /* show_message */);
  // ArcImeService should be selected.
  ASSERT_EQ(1u, bridge()->switch_ime_to_calls_.size());
  EXPECT_EQ(arc_ime_service_id, bridge()->switch_ime_to_calls_[0]);

  // Set current (active) input method to the component extension ime.
  imm()->state()->SetCurrentInputMethod(component_extension_ime_id);
  service()->InputMethodChanged(imm(), nullptr, false /* show_message */);
  // ArcImeService should be selected.
  ASSERT_EQ(2u, bridge()->switch_ime_to_calls_.size());
  EXPECT_EQ(arc_ime_service_id, bridge()->switch_ime_to_calls_[1]);

  // Set current (active) input method to the arc ime.
  imm()->state()->SetCurrentInputMethod(arc_ime_id);
  service()->InputMethodChanged(imm(), nullptr, false /* show_message */);
  ASSERT_EQ(3u, bridge()->switch_ime_to_calls_.size());
  EXPECT_EQ("ime.id.in.arc.container", bridge()->switch_ime_to_calls_[2]);
}

TEST_F(ArcInputMethodManagerServiceTest, OnImeDisabled) {
  namespace aeiu = ::ash::extension_ime_util;

  constexpr char kNonArcIme[] = "ime_a";
  constexpr char kArcImeX[] = "arc_ime_x";
  constexpr char kArcImeY[] = "arc_ime_y";
  constexpr char kArcIMEProxyExtensionName[] =
      "org.chromium.arc.inputmethod.proxy";

  const std::string proxy_ime_extension_id =
      crx_file::id_util::GenerateId(kArcIMEProxyExtensionName);
  const std::string arc_ime_x_component =
      aeiu::GetArcInputMethodID(proxy_ime_extension_id, kArcImeX);
  const std::string arc_ime_y_component =
      aeiu::GetArcInputMethodID(proxy_ime_extension_id, kArcImeY);
  mojom::ImeInfoPtr arc_ime_x = GenerateImeInfo(kArcImeX, "", "", false, false);
  mojom::ImeInfoPtr arc_ime_y = GenerateImeInfo(kArcImeY, "", "", false, false);

  ToggleTabletMode(true);

  // Adding two ARC IMEs.
  {
    std::vector<mojom::ImeInfoPtr> info_array;
    info_array.emplace_back(arc_ime_x.Clone());
    info_array.emplace_back(arc_ime_y.Clone());
    service()->OnImeInfoChanged(std::move(info_array));
  }

  // Enable one non-ARC IME, then remove an ARC IME. This usually does not
  // happen, but confirm that OnImeDisabled() does not do anything bad even
  // if the IPC is called that way.
  profile()->GetPrefs()->SetString(prefs::kLanguageEnabledImes, kNonArcIme);
  service()->OnImeDisabled(kArcImeX);
  EXPECT_EQ(kNonArcIme,
            profile()->GetPrefs()->GetString(prefs::kLanguageEnabledImes));

  // Enable two IMEs (one non-ARC and one ARC), remove the ARC IME, and then
  // confirm the non-ARC one remains.
  arc_ime_x->enabled = true;
  {
    std::vector<mojom::ImeInfoPtr> info_array;
    info_array.emplace_back(arc_ime_x.Clone());
    info_array.emplace_back(arc_ime_y.Clone());
    service()->OnImeInfoChanged(std::move(info_array));
  }
  std::string pref_str =
      base::StringPrintf("%s,%s", kNonArcIme, arc_ime_x_component.c_str());
  EXPECT_EQ(pref_str,
            profile()->GetPrefs()->GetString(prefs::kLanguageEnabledImes));
  service()->OnImeDisabled(kArcImeX);
  EXPECT_EQ(kNonArcIme,
            profile()->GetPrefs()->GetString(prefs::kLanguageEnabledImes));

  // Enable two ARC IMEs along with one non-ARC one, remove one of two ARC IMEs,
  // then confirm one non-ARC IME and one ARC IME still remain.
  arc_ime_y->enabled = true;
  {
    std::vector<mojom::ImeInfoPtr> info_array;
    info_array.emplace_back(arc_ime_x.Clone());
    info_array.emplace_back(arc_ime_y.Clone());
    service()->OnImeInfoChanged(std::move(info_array));
  }
  pref_str =
      base::StringPrintf("%s,%s,%s", kNonArcIme, arc_ime_x_component.c_str(),
                         arc_ime_y_component.c_str());
  EXPECT_EQ(pref_str,
            profile()->GetPrefs()->GetString(prefs::kLanguageEnabledImes));
  service()->OnImeDisabled(kArcImeX);
  pref_str =
      base::StringPrintf("%s,%s", kNonArcIme, arc_ime_y_component.c_str());
  EXPECT_EQ(pref_str,
            profile()->GetPrefs()->GetString(prefs::kLanguageEnabledImes));
}

TEST_F(ArcInputMethodManagerServiceTest, OnImeInfoChanged) {
  namespace aeiu = ::ash::extension_ime_util;

  ToggleTabletMode(true);

  // Preparing 2 ImeInfo.
  const std::string android_ime_id1 = "test.arc.ime";
  const std::string display_name1 = "DisplayName";
  const std::string settings_url1 = "url_to_settings";
  mojom::ImeInfoPtr info1 = GenerateImeInfo(android_ime_id1, display_name1,
                                            settings_url1, false, false);

  const std::string android_ime_id2 = "test.arc.ime2";
  const std::string display_name2 = "DisplayName2";
  const std::string settings_url2 = "url_to_settings2";
  mojom::ImeInfoPtr info2 = GenerateImeInfo(android_ime_id2, display_name2,
                                            settings_url2, true, false);

  std::vector<std::tuple<std::string, im::InputMethodDescriptors,
                         ash::TextInputMethod*>>& added_extensions =
      imm()->state()->added_input_method_extensions_;
  ASSERT_EQ(0u, added_extensions.size());

  {
    // Passing empty info_array shouldn't call AddInputMethodExtension.
    std::vector<mojom::ImeInfoPtr> info_array{};
    service()->OnImeInfoChanged(std::move(info_array));
    EXPECT_TRUE(added_extensions.empty());
  }

  {
    // Adding one ARC IME.
    std::vector<mojom::ImeInfoPtr> info_array;
    info_array.emplace_back(info1.Clone());
    service()->OnImeInfoChanged(std::move(info_array));
    ASSERT_EQ(1u, added_extensions.size());
    ASSERT_EQ(1u, std::get<1>(added_extensions[0]).size());
    EXPECT_EQ(android_ime_id1, aeiu::GetComponentIDByInputMethodID(
                                   std::get<1>(added_extensions[0])[0].id()));
    EXPECT_EQ(display_name1, std::get<1>(added_extensions[0])[0].name());
    ASSERT_EQ(1u, std::get<1>(added_extensions[0])[0].language_codes().size());
    EXPECT_TRUE(ash::extension_ime_util::IsArcIME(
        std::get<1>(added_extensions[0])[0].id()));

    // Emulate enabling ARC IME from chrome://settings.
    const std::string& arc_ime_id = std::get<1>(added_extensions[0])[0].id();
    profile()->GetPrefs()->SetString(prefs::kLanguageEnabledImes, arc_ime_id);
    EXPECT_EQ(arc_ime_id,
              profile()->GetPrefs()->GetString(prefs::kLanguageEnabledImes));

    // Removing the ARC IME should clear the pref
    std::vector<mojom::ImeInfoPtr> empty_info_array;
    service()->OnImeInfoChanged(std::move(empty_info_array));
    EXPECT_TRUE(
        profile()->GetPrefs()->GetString(prefs::kLanguageEnabledImes).empty());
    added_extensions.clear();
  }

  {
    // Adding two ARC IMEs. One is already enabled.
    std::vector<mojom::ImeInfoPtr> info_array;
    info_array.emplace_back(info1.Clone());
    info_array.emplace_back(info2.Clone());
    service()->OnImeInfoChanged(std::move(info_array));
    // The ARC IMEs should be registered as two IMEs in one extension.
    ASSERT_EQ(1u, added_extensions.size());
    ASSERT_EQ(2u, std::get<1>(added_extensions[0]).size());
    EXPECT_EQ(android_ime_id1, aeiu::GetComponentIDByInputMethodID(
                                   std::get<1>(added_extensions[0])[0].id()));
    EXPECT_EQ(display_name1, std::get<1>(added_extensions[0])[0].name());
    EXPECT_EQ(android_ime_id2, aeiu::GetComponentIDByInputMethodID(
                                   std::get<1>(added_extensions[0])[1].id()));
    EXPECT_EQ(display_name2, std::get<1>(added_extensions[0])[1].name());

    // Already enabled IME should be added to the pref automatically.
    const std::string& arc_ime_id2 = std::get<1>(added_extensions[0])[1].id();
    EXPECT_EQ(arc_ime_id2,
              profile()->GetPrefs()->GetString(prefs::kLanguageEnabledImes));

    added_extensions.clear();
  }
}

TEST_F(ArcInputMethodManagerServiceTest, EnableArcIMEsOnlyInTabletMode) {
  namespace aeiu = ::ash::extension_ime_util;
  using crx_file::id_util::GenerateId;

  constexpr char kArcIMEProxyExtensionName[] =
      "org.chromium.arc.inputmethod.proxy";

  const std::string extension_ime_id =
      aeiu::GetInputMethodID(GenerateId("test.extension.ime"), "us");
  const std::string component_extension_ime_id =
      aeiu::GetComponentInputMethodID(
          GenerateId("test.component.extension.ime"), "us");
  const std::string proxy_ime_extension_id =
      crx_file::id_util::GenerateId(kArcIMEProxyExtensionName);
  const std::string android_ime_id = "test.arc.ime";
  const std::string arc_ime_id =
      aeiu::GetArcInputMethodID(proxy_ime_extension_id, android_ime_id);

  // Start from tablet mode.
  ToggleTabletMode(true);

  // Activate the extension IME and the component extension IME.
  imm()->state()->AddEnabledInputMethodId(extension_ime_id);
  imm()->state()->AddEnabledInputMethodId(component_extension_ime_id);
  // Update the prefs because the testee checks them.
  profile()->GetPrefs()->SetString(
      prefs::kLanguageEnabledImes,
      base::StringPrintf("%s,%s", extension_ime_id.c_str(),
                         component_extension_ime_id.c_str()));
  service()->ImeMenuListChanged();

  imm()->state()->Reset();

  // Enable the ARC IME.
  {
    mojom::ImeInfoPtr info =
        GenerateImeInfo(android_ime_id, "", "", true, false);
    std::vector<mojom::ImeInfoPtr> info_array{};
    info_array.emplace_back(info.Clone());
    service()->OnImeInfoChanged(std::move(info_array));
  }
  // IMM should get the newly enabled IME id.
  EXPECT_EQ(1u, imm()->state()->added_input_method_extensions_.size());
  EXPECT_EQ(1u, imm()->state()->enabled_input_methods_.size());
  EXPECT_EQ(arc_ime_id, imm()->state()->enabled_input_methods_.at(0));
  imm()->state()->enabled_input_methods_.clear();
  {
    // Pref should get updated.
    const auto enabled_ime_in_pref = GetEnabledInputMethodIds();
    EXPECT_EQ(3u, enabled_ime_in_pref.size());
    EXPECT_EQ(arc_ime_id, enabled_ime_in_pref.at(2));
  }

  imm()->state()->Reset();

  // Change to laptop mode.
  ToggleTabletMode(false);

  // ARC IME is not allowed in laptop mode.
  // The fake IME extension is uninstalled.
  EXPECT_EQ(1u, imm()->state()->removed_input_method_extensions_.size());
  EXPECT_TRUE(imm()->state()->enabled_input_methods_.empty());
  {
    const auto enabled_ime_in_pref = GetEnabledInputMethodIds();
    EXPECT_EQ(2u, enabled_ime_in_pref.size());
  }

  imm()->state()->Reset();

  // Back to tablet mode.
  ToggleTabletMode(true);

  // All IMEs are allowed to use.
  EXPECT_EQ(1u, imm()->state()->added_input_method_extensions_.size());
  EXPECT_EQ(1u, imm()->state()->enabled_input_methods_.size());
  EXPECT_EQ(arc_ime_id, imm()->state()->enabled_input_methods_.at(0));
  imm()->state()->enabled_input_methods_.clear();
  {
    const auto enabled_ime_in_pref = GetEnabledInputMethodIds();
    EXPECT_EQ(3u, enabled_ime_in_pref.size());
    EXPECT_EQ(arc_ime_id, enabled_ime_in_pref.at(2));
  }

  imm()->state()->Reset();

  // Confirm that entering the same mode twice in a row is no-op.
  ToggleTabletMode(true);
  EXPECT_TRUE(imm()->state()->removed_input_method_extensions_.empty());
  EXPECT_TRUE(imm()->state()->added_input_method_extensions_.empty());
  EXPECT_TRUE(imm()->state()->enabled_input_methods_.empty());

  ToggleTabletMode(false);
  EXPECT_EQ(1u, imm()->state()->removed_input_method_extensions_.size());
  EXPECT_TRUE(imm()->state()->enabled_input_methods_.empty());
  {
    const auto enabled_ime_in_pref = GetEnabledInputMethodIds();
    EXPECT_EQ(2u, enabled_ime_in_pref.size());
  }

  ToggleTabletMode(false);
  EXPECT_EQ(1u, imm()->state()->removed_input_method_extensions_.size());
  EXPECT_TRUE(imm()->state()->enabled_input_methods_.empty());
  {
    const auto enabled_ime_in_pref = GetEnabledInputMethodIds();
    EXPECT_EQ(2u, enabled_ime_in_pref.size());
  }
}

TEST_F(ArcInputMethodManagerServiceTest,
       RemoveArcIMEsWhenAccessibilityKeyboardEnabled) {
  namespace aeiu = ::ash::extension_ime_util;
  using crx_file::id_util::GenerateId;

  constexpr char kArcIMEProxyExtensionName[] =
      "org.chromium.arc.inputmethod.proxy";

  const std::string extension_ime_id =
      aeiu::GetInputMethodID(GenerateId("test.extension.ime"), "us");
  const std::string component_extension_ime_id =
      aeiu::GetComponentInputMethodID(
          GenerateId("test.component.extension.ime"), "us");
  const std::string proxy_ime_extension_id =
      crx_file::id_util::GenerateId(kArcIMEProxyExtensionName);
  const std::string android_ime_id = "test.arc.ime";
  const std::string arc_ime_id =
      aeiu::GetArcInputMethodID(proxy_ime_extension_id, android_ime_id);

  // Start from tablet mode.
  ToggleTabletMode(true);

  // Activate the extension IME and the component extension IME.
  imm()->state()->AddEnabledInputMethodId(extension_ime_id);
  imm()->state()->AddEnabledInputMethodId(component_extension_ime_id);
  // Update the prefs because the testee checks them.
  profile()->GetPrefs()->SetString(
      prefs::kLanguageEnabledImes,
      base::StringPrintf("%s,%s", extension_ime_id.c_str(),
                         component_extension_ime_id.c_str()));
  service()->ImeMenuListChanged();

  imm()->state()->Reset();

  // All IMEs are allowed to use.
  // Enable the ARC IME.
  {
    mojom::ImeInfoPtr info =
        GenerateImeInfo(android_ime_id, "", "", true, false);
    std::vector<mojom::ImeInfoPtr> info_array{};
    info_array.emplace_back(info.Clone());
    service()->OnImeInfoChanged(std::move(info_array));
  }
  // IMM should get the newly enabled IME id.
  EXPECT_EQ(1u, imm()->state()->added_input_method_extensions_.size());
  EXPECT_EQ(1u, imm()->state()->enabled_input_methods_.size());
  EXPECT_EQ(arc_ime_id, imm()->state()->enabled_input_methods_.at(0));
  imm()->state()->enabled_input_methods_.clear();
  {
    // Pref should get updated.
    const auto enabled_ime_in_pref = GetEnabledInputMethodIds();
    EXPECT_EQ(3u, enabled_ime_in_pref.size());
    EXPECT_EQ(arc_ime_id, enabled_ime_in_pref.at(2));
  }

  imm()->state()->Reset();

  // Enable a11y keyboard option.
  profile()->GetPrefs()->SetBoolean(
      ash::prefs::kAccessibilityVirtualKeyboardEnabled, true);
  // Notify ArcInputMethodManagerService.
  service()->OnAccessibilityStatusChanged(
      {AccessibilityNotificationType::kToggleVirtualKeyboard, true});

  // ARC IME is not allowed when a11y keyboard is enabled.
  EXPECT_EQ(1u, imm()->state()->removed_input_method_extensions_.size());
  EXPECT_TRUE(imm()->state()->enabled_input_methods_.empty());
  {
    const auto enabled_ime_in_pref = GetEnabledInputMethodIds();
    EXPECT_EQ(2u, enabled_ime_in_pref.size());
  }

  imm()->state()->removed_input_method_extensions_.clear();
  imm()->state()->added_input_method_extensions_.clear();
  imm()->state()->enabled_input_methods_.clear();

  // Disable a11y keyboard option.
  profile()->GetPrefs()->SetBoolean(
      ash::prefs::kAccessibilityVirtualKeyboardEnabled, false);
  // Notify ArcInputMethodManagerService.
  service()->OnAccessibilityStatusChanged(
      {AccessibilityNotificationType::kToggleVirtualKeyboard, false});

  // ARC IME can be enabled.
  EXPECT_EQ(1u, imm()->state()->added_input_method_extensions_.size());
  EXPECT_EQ(1u, imm()->state()->enabled_input_methods_.size());
  EXPECT_EQ(arc_ime_id, imm()->state()->enabled_input_methods_.at(0));
  imm()->state()->enabled_input_methods_.clear();
  {
    // Pref should get updated.
    const auto enabled_ime_in_pref = GetEnabledInputMethodIds();
    EXPECT_EQ(3u, enabled_ime_in_pref.size());
    EXPECT_EQ(arc_ime_id, enabled_ime_in_pref.at(2));
  }
}

TEST_F(ArcInputMethodManagerServiceTest,
       AllowArcIMEsWhileCommandLineFlagIsSet) {
  namespace aeiu = ::ash::extension_ime_util;
  using crx_file::id_util::GenerateId;

  constexpr char kArcIMEProxyExtensionName[] =
      "org.chromium.arc.inputmethod.proxy";

  const std::string extension_ime_id =
      aeiu::GetInputMethodID(GenerateId("test.extension.ime"), "us");
  const std::string component_extension_ime_id =
      aeiu::GetComponentInputMethodID(
          GenerateId("test.component.extension.ime"), "us");
  const std::string proxy_ime_extension_id =
      crx_file::id_util::GenerateId(kArcIMEProxyExtensionName);
  const std::string android_ime_id = "test.arc.ime";
  const std::string arc_ime_id =
      aeiu::GetArcInputMethodID(proxy_ime_extension_id, android_ime_id);

  // Add '--enable-virtual-keyboard' flag.
  base::test::ScopedCommandLine scoped_command_line;
  base::CommandLine* command_line = scoped_command_line.GetProcessCommandLine();
  command_line->AppendSwitch(keyboard::switches::kEnableVirtualKeyboard);

  // Start from tablet mode.
  ToggleTabletMode(true);

  // Activate the extension IME and the component extension IME.
  imm()->state()->AddEnabledInputMethodId(extension_ime_id);
  imm()->state()->AddEnabledInputMethodId(component_extension_ime_id);
  // Update the prefs because the testee checks them.
  profile()->GetPrefs()->SetString(
      prefs::kLanguageEnabledImes,
      base::StringPrintf("%s,%s", extension_ime_id.c_str(),
                         component_extension_ime_id.c_str()));
  service()->ImeMenuListChanged();

  imm()->state()->removed_input_method_extensions_.clear();
  imm()->state()->added_input_method_extensions_.clear();
  imm()->state()->enabled_input_methods_.clear();

  // Enable the ARC IME.
  {
    mojom::ImeInfoPtr info =
        GenerateImeInfo(android_ime_id, "", "", true, false);
    std::vector<mojom::ImeInfoPtr> info_array{};
    info_array.emplace_back(info.Clone());
    service()->OnImeInfoChanged(std::move(info_array));
  }
  // IMM should get the newly enabled IME id.
  EXPECT_EQ(1u, imm()->state()->added_input_method_extensions_.size());
  EXPECT_EQ(1u, imm()->state()->enabled_input_methods_.size());
  EXPECT_EQ(arc_ime_id, imm()->state()->enabled_input_methods_.at(0));
  {
    // Pref should get updated.
    const auto enabled_ime_in_pref = GetEnabledInputMethodIds();
    EXPECT_EQ(3u, enabled_ime_in_pref.size());
    EXPECT_EQ(arc_ime_id, enabled_ime_in_pref.at(2));
  }

  imm()->state()->removed_input_method_extensions_.clear();
  imm()->state()->added_input_method_extensions_.clear();
  imm()->state()->enabled_input_methods_.clear();

  // Change to laptop mode.
  ToggleTabletMode(false);

  // All IMEs are allowed to use even in laptop mode if the flag is set.
  EXPECT_EQ(1u, imm()->state()->added_input_method_extensions_.size());
  EXPECT_EQ(1u, imm()->state()->enabled_input_methods_.size());
  EXPECT_EQ(arc_ime_id, imm()->state()->enabled_input_methods_.at(0));
  {
    const auto enabled_ime_in_pref = GetEnabledInputMethodIds();
    EXPECT_EQ(3u, enabled_ime_in_pref.size());
    EXPECT_EQ(arc_ime_id, enabled_ime_in_pref.at(2));
  }
}

TEST_F(ArcInputMethodManagerServiceTest, FocusAndBlur) {
  ToggleTabletMode(true);

  // Adding one ARC IME.
  {
    const std::string android_ime_id = "test.arc.ime";
    const std::string display_name = "DisplayName";
    const std::string settings_url = "url_to_settings";
    mojom::ImeInfoPtr info = GenerateImeInfo(android_ime_id, display_name,
                                             settings_url, false, false);

    std::vector<mojom::ImeInfoPtr> info_array;
    info_array.emplace_back(std::move(info));
    service()->OnImeInfoChanged(std::move(info_array));
  }
  // The proxy IME engine should be added.
  ASSERT_EQ(1u, imm()->state()->added_input_method_extensions_.size());
  ash::TextInputMethod* engine_handler =
      std::get<2>(imm()->state()->added_input_method_extensions_.at(0));

  // Set up mock input context.
  const ash::TextInputMethod::InputContext test_context(
      ui::TEXT_INPUT_TYPE_TEXT);
  ui::MockInputMethod mock_input_method(nullptr);
  TestIMEInputContextHandler test_context_handler(&mock_input_method);
  ui::DummyTextInputClient dummy_text_input_client(ui::TEXT_INPUT_TYPE_TEXT);
  ash::IMEBridge::Get()->SetInputContextHandler(&test_context_handler);

  // Enable the ARC IME.
  ash::IMEBridge::Get()->SetCurrentEngineHandler(engine_handler);
  engine_handler->Enable(ash::extension_ime_util::GetComponentIDByInputMethodID(
      std::get<1>(imm()->state()->added_input_method_extensions_.at(0))
          .at(0)
          .id()));
  mock_input_method.SetFocusedTextInputClient(&dummy_text_input_client);

  ASSERT_EQ(0, bridge()->focus_calls_count_);

  engine_handler->Focus(test_context);
  EXPECT_EQ(1, bridge()->focus_calls_count_);

  engine_handler->Blur();
  EXPECT_EQ(1, bridge()->focus_calls_count_);

  // If an ARC window is focused, Focus doesn't call the bridge's Focus().
  auto window = base::WrapUnique(CreateTestArcWindow());
  window_delegate()->SetFocusedWindow(window.get());
  window_delegate()->SetActiveWindow(window.get());

  engine_handler->Focus(test_context);
  EXPECT_EQ(1, bridge()->focus_calls_count_);

  engine_handler->Blur();
  EXPECT_EQ(1, bridge()->focus_calls_count_);
}

TEST_F(ArcInputMethodManagerServiceTest, DisableFallbackVirtualKeyboard) {
  namespace aeiu = ::ash::extension_ime_util;
  using crx_file::id_util::GenerateId;

  ToggleTabletMode(true);

  const std::string extension_ime_id =
      aeiu::GetInputMethodID(GenerateId("test.extension.ime"), "us");
  const std::string component_extension_ime_id =
      aeiu::GetComponentInputMethodID(
          GenerateId("test.component.extension.ime"), "us");
  const std::string arc_ime_id = aeiu::GetArcInputMethodID(
      GenerateId("test.arc.ime"), "ime.id.in.arc.container");

  // Set current (active) input method to the extension ime.
  imm()->state()->SetCurrentInputMethod(extension_ime_id);
  service()->InputMethodChanged(imm(), profile(), false /* show_message */);

  // Enable Chrome OS virtual keyboard
  auto* client = ChromeKeyboardControllerClient::Get();
  client->ClearEnableFlag(keyboard::KeyboardEnableFlag::kAndroidDisabled);
  client->SetEnableFlag(keyboard::KeyboardEnableFlag::kTouchEnabled);
  base::RunLoop().RunUntilIdle();  // Allow observers to fire and process.
  ASSERT_FALSE(
      client->IsEnableFlagSet(keyboard::KeyboardEnableFlag::kAndroidDisabled));

  // It's disabled when the ARC IME is activated.
  imm()->state()->SetCurrentInputMethod(arc_ime_id);
  service()->InputMethodChanged(imm(), profile(), false);
  EXPECT_TRUE(
      client->IsEnableFlagSet(keyboard::KeyboardEnableFlag::kAndroidDisabled));

  // It's re-enabled when the ARC IME is deactivated.
  imm()->state()->SetCurrentInputMethod(component_extension_ime_id);
  service()->InputMethodChanged(imm(), profile(), false);
  EXPECT_FALSE(
      client->IsEnableFlagSet(keyboard::KeyboardEnableFlag::kAndroidDisabled));
}

TEST_F(ArcInputMethodManagerServiceTest, ShowVirtualKeyboard) {
  ToggleTabletMode(true);

  // Adding one ARC IME.
  {
    const std::string android_ime_id = "test.arc.ime";
    const std::string display_name = "DisplayName";
    const std::string settings_url = "url_to_settings";
    mojom::ImeInfoPtr info = GenerateImeInfo(android_ime_id, display_name,
                                             settings_url, false, false);

    std::vector<mojom::ImeInfoPtr> info_array;
    info_array.emplace_back(std::move(info));
    service()->OnImeInfoChanged(std::move(info_array));
  }
  // The proxy IME engine should be added.
  ASSERT_EQ(1u, imm()->state()->added_input_method_extensions_.size());
  ash::TextInputMethod* engine_handler =
      std::get<2>(imm()->state()->added_input_method_extensions_.at(0));

  // Set up mock input context.
  const ash::TextInputMethod::InputContext test_context(
      ui::TEXT_INPUT_TYPE_TEXT);
  ui::MockInputMethod mock_input_method(nullptr);
  TestIMEInputContextHandler test_context_handler(&mock_input_method);
  ui::DummyTextInputClient dummy_text_input_client(ui::TEXT_INPUT_TYPE_TEXT);
  ash::IMEBridge::Get()->SetInputContextHandler(&test_context_handler);

  // Enable the ARC IME.
  ash::IMEBridge::Get()->SetCurrentEngineHandler(engine_handler);
  engine_handler->Enable(ash::extension_ime_util::GetComponentIDByInputMethodID(
      std::get<1>(imm()->state()->added_input_method_extensions_.at(0))
          .at(0)
          .id()));

  mock_input_method.SetFocusedTextInputClient(&dummy_text_input_client);

  EXPECT_EQ(0, bridge()->show_virtual_keyboard_calls_count_);
  mock_input_method.SetVirtualKeyboardVisibilityIfEnabled(true);
  EXPECT_EQ(1, bridge()->show_virtual_keyboard_calls_count_);
  ash::IMEBridge::Get()->SetInputContextHandler(nullptr);
  ash::IMEBridge::Get()->SetCurrentEngineHandler(nullptr);
}

TEST_F(ArcInputMethodManagerServiceTest, VisibilityObserver) {
  ToggleTabletMode(true);

  FakeInputMethodBoundsObserver observer;
  service()->AddObserver(&observer);
  ASSERT_FALSE(observer.last_visibility());
  ASSERT_EQ(0, observer.visibility_changed_call_count());

  // Notify new non-empty bounds not when ARC IME is active.
  NotifyNewBounds(gfx::Rect(0, 0, 100, 100));
  // It should not cause visibility changed event.
  EXPECT_FALSE(observer.last_visibility());
  EXPECT_EQ(0, observer.visibility_changed_call_count());

  NotifyNewBounds(gfx::Rect(0, 0, 0, 0));
  EXPECT_FALSE(observer.last_visibility());
  EXPECT_EQ(0, observer.visibility_changed_call_count());
  observer.Reset();

  // Adding one ARC IME.
  {
    const std::string android_ime_id = "test.arc.ime";
    const std::string display_name = "DisplayName";
    const std::string settings_url = "url_to_settings";
    mojom::ImeInfoPtr info = GenerateImeInfo(android_ime_id, display_name,
                                             settings_url, false, false);
    info->ime_id = android_ime_id;
    info->display_name = display_name;
    info->enabled = false;
    info->settings_url = settings_url;

    std::vector<mojom::ImeInfoPtr> info_array;
    info_array.emplace_back(std::move(info));
    service()->OnImeInfoChanged(std::move(info_array));
  }
  // The proxy IME engine should be added.
  ASSERT_EQ(1u, imm()->state()->added_input_method_extensions_.size());
  ash::TextInputMethod* engine_handler =
      std::get<2>(imm()->state()->added_input_method_extensions_.at(0));

  // Set up mock input context.
  const ash::TextInputMethod::InputContext test_context(
      ui::TEXT_INPUT_TYPE_TEXT);
  ui::MockInputMethod mock_input_method(nullptr);
  TestIMEInputContextHandler test_context_handler(&mock_input_method);
  ui::DummyTextInputClient dummy_text_input_client(ui::TEXT_INPUT_TYPE_TEXT);
  ash::IMEBridge::Get()->SetInputContextHandler(&test_context_handler);

  // Enable the ARC IME.
  ash::IMEBridge::Get()->SetCurrentEngineHandler(engine_handler);
  engine_handler->Enable(ash::extension_ime_util::GetComponentIDByInputMethodID(
      std::get<1>(imm()->state()->added_input_method_extensions_.at(0))
          .at(0)
          .id()));
  mock_input_method.SetFocusedTextInputClient(&dummy_text_input_client);

  // Notify non-empty bounds should cause a visibility changed event now.
  NotifyNewBounds(gfx::Rect(0, 0, 100, 100));
  EXPECT_TRUE(observer.last_visibility());
  EXPECT_EQ(1, observer.visibility_changed_call_count());
  // A visibility changed event won't be sent if only size is changed.
  NotifyNewBounds(gfx::Rect(0, 0, 200, 200));
  EXPECT_TRUE(observer.last_visibility());
  EXPECT_EQ(1, observer.visibility_changed_call_count());

  NotifyNewBounds(gfx::Rect(0, 0, 0, 0));
  EXPECT_FALSE(observer.last_visibility());
  EXPECT_EQ(2, observer.visibility_changed_call_count());

  service()->RemoveObserver(&observer);
}

}  // namespace arc
