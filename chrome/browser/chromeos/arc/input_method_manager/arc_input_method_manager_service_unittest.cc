// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/input_method_manager/arc_input_method_manager_service.h"

#include <memory>
#include <tuple>
#include <utility>
#include <vector>

#include "ash/keyboard/ash_keyboard_controller.h"
#include "ash/public/cpp/ash_pref_names.h"
#include "ash/public/interfaces/constants.mojom.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/ash/chrome_keyboard_controller_client.h"
#include "chrome/browser/ui/ash/tablet_mode_client.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "components/arc/arc_features.h"
#include "components/arc/arc_service_manager.h"
#include "components/arc/test/test_browser_context.h"
#include "components/crx_file/id_util.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/public/test/test_service_manager_context.h"
#include "services/service_manager/public/cpp/connector.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ime/chromeos/extension_ime_util.h"
#include "ui/base/ime/chromeos/mock_input_method_manager.h"
#include "ui/base/ime/dummy_text_input_client.h"
#include "ui/base/ime/ime_bridge.h"
#include "ui/base/ime/mock_ime_input_context_handler.h"
#include "ui/base/ime/mock_input_method.h"

namespace arc {
namespace {

namespace im = chromeos::input_method;

// The fake im::InputMethodManager for testing.
class TestInputMethodManager : public im::MockInputMethodManager {
 public:
  // The fake im::InputMethodManager::State implementation for testing.
  class TestState : public im::MockInputMethodManager::State {
   public:
    TestState()
        : added_input_method_extensions_(), active_input_method_ids_() {}

    const std::vector<std::string>& GetActiveInputMethodIds() const override {
      return active_input_method_ids_;
    }

    im::InputMethodDescriptor GetCurrentInputMethod() const override {
      im::InputMethodDescriptor descriptor(
          active_ime_id_, "", "", std::vector<std::string>(),
          std::vector<std::string>(), false /* is_login_keyboard */, GURL(),
          GURL());
      return descriptor;
    }

    void AddInputMethodExtension(
        const std::string& extension_id,
        const im::InputMethodDescriptors& descriptors,
        ui::IMEEngineHandlerInterface* instance) override {
      added_input_method_extensions_.push_back(
          std::make_tuple(extension_id, descriptors, instance));
    }

    void RemoveInputMethodExtension(const std::string& extension_id) override {
      removed_input_method_extensions_.push_back(extension_id);
    }

    bool EnableInputMethod(
        const std::string& new_active_input_method_id) override {
      enabled_input_methods_.push_back(new_active_input_method_id);
      return true;
    }

    void AddActiveInputMethodId(const std::string& ime_id) {
      if (!std::count(active_input_method_ids_.begin(),
                      active_input_method_ids_.end(), ime_id)) {
        active_input_method_ids_.push_back(ime_id);
      }
    }

    void RemoveActiveInputMethodId(const std::string& ime_id) {
      base::EraseIf(active_input_method_ids_,
                    [&ime_id](const std::string& id) { return id == ime_id; });
    }

    void SetActiveInputMethod(const std::string& ime_id) {
      active_ime_id_ = ime_id;
    }

    void GetInputMethodExtensions(
        im::InputMethodDescriptors* descriptors) override {
      for (const auto& id : active_input_method_ids_) {
        descriptors->push_back(im::InputMethodDescriptor(
            id, "", "", {}, {}, false, GURL(), GURL()));
      }
    }

    bool SetAllowedInputMethods(
        const std::vector<std::string>& new_allowed_input_method_ids,
        bool enable_allowed_input_methods) override {
      allowed_input_methods_ = new_allowed_input_method_ids;
      return true;
    }

    const std::vector<std::string>& GetAllowedInputMethods() override {
      return allowed_input_methods_;
    }

    bool IsInputMethodAllowed(const std::string& ime_id) {
      return allowed_input_methods_.empty() ||
             base::ContainsValue(allowed_input_methods_, ime_id);
    }

    std::vector<std::tuple<std::string,
                           im::InputMethodDescriptors,
                           ui::IMEEngineHandlerInterface*>>
        added_input_method_extensions_;
    std::vector<std::string> removed_input_method_extensions_;
    std::vector<std::string> enabled_input_methods_;

   protected:
    friend base::RefCounted<InputMethodManager::State>;
    ~TestState() override = default;

   private:
    std::vector<std::string> active_input_method_ids_;
    std::string active_ime_id_ = "";
    std::vector<std::string> allowed_input_methods_;
  };

  TestInputMethodManager() {
    state_ = scoped_refptr<TestState>(new TestState());
  }
  ~TestInputMethodManager() override = default;

  scoped_refptr<InputMethodManager::State> GetActiveIMEState() override {
    return state_;
  }

  TestState* state() { return state_.get(); }

 private:
  scoped_refptr<TestState> state_;

  DISALLOW_COPY_AND_ASSIGN(TestInputMethodManager);
};

class TestIMEInputContextHandler : public ui::MockIMEInputContextHandler {
 public:
  explicit TestIMEInputContextHandler(ui::InputMethod* input_method)
      : input_method_(input_method) {}

  ui::InputMethod* GetInputMethod() override { return input_method_; }

  void SendKeyEvent(ui::KeyEvent* event) override {
    ui::MockIMEInputContextHandler::SendKeyEvent(event);
    ++send_key_event_call_count_;
  }

  void Reset() {
    ui::MockIMEInputContextHandler::Reset();
    send_key_event_call_count_ = 0;
  }

  int send_key_event_call_count() const { return send_key_event_call_count_; }

 private:
  ui::InputMethod* const input_method_;

  int send_key_event_call_count_ = 0;

  DISALLOW_COPY_AND_ASSIGN(TestIMEInputContextHandler);
};

class TestInputMethodManagerBridge : public ArcInputMethodManagerBridge {
 public:
  TestInputMethodManagerBridge() = default;
  ~TestInputMethodManagerBridge() override = default;

  void SendEnableIme(const std::string& ime_id,
                     bool enable,
                     EnableImeCallback callback) override {
    enable_ime_calls_.push_back(std::make_tuple(ime_id, enable));
    std::move(callback).Run(true);
  }
  void SendSwitchImeTo(const std::string& ime_id,
                       SwitchImeToCallback callback) override {
    switch_ime_to_calls_.push_back(ime_id);
    std::move(callback).Run(true);
  }

  void SendFocus(mojom::InputConnectionPtr connection,
                 mojom::TextInputStatePtr state) override {
    ++focus_calls_count_;
  }

  void SendUpdateTextInputState(mojom::TextInputStatePtr state) override {
    ++update_text_input_state_calls_count_;
    last_text_input_state = state.Clone();
  }

  void SendShowVirtualKeyboard() override {
    ++show_virtual_keyboard_calls_count_;
  }
  void SendHideVirtualKeyboard() override {}

  std::vector<std::tuple<std::string, bool>> enable_ime_calls_;
  std::vector<std::string> switch_ime_to_calls_;
  int focus_calls_count_ = 0;
  int update_text_input_state_calls_count_ = 0;
  mojom::TextInputStatePtr last_text_input_state;
  int show_virtual_keyboard_calls_count_ = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(TestInputMethodManagerBridge);
};

// TODO(crbug.com/890677): Stop inheriting ash::AshTestBase once ash::Shell
// dependency is removed from ArcInputMethodManagerService.
class ArcInputMethodManagerServiceTest : public ash::AshTestBase {
 protected:
  ArcInputMethodManagerServiceTest()
      : arc_service_manager_(std::make_unique<ArcServiceManager>()) {}
  ~ArcInputMethodManagerServiceTest() override = default;

  ArcInputMethodManagerService* service() { return service_; }

  TestInputMethodManagerBridge* bridge() { return test_bridge_; }

  TestInputMethodManager* imm() { return input_method_manager_; }

  TestingProfile* profile() { return profile_.get(); }

  void ToggleTabletMode(bool enabled) {
    tablet_mode_client_->OnTabletModeToggled(enabled);
  }

  void SetUp() override {
    ash::AshTestBase::SetUp();
    SetRunningOutsideAsh();
    ui::IMEBridge::Initialize();
    input_method_manager_ = new TestInputMethodManager();
    chromeos::input_method::InputMethodManager::Initialize(
        input_method_manager_);
    profile_ = std::make_unique<TestingProfile>();
    tablet_mode_client_ = std::make_unique<TabletModeClient>();

    // Create a local service manager connector to handle requests to
    // ash::mojom::CrosDisplayConfigController.
    service_manager::mojom::ConnectorRequest request;
    connector_ = service_manager::Connector::Create(&request);
    service_manager::Connector::TestApi test_api(connector_.get());
    test_api.OverrideBinderForTesting(
        service_manager::Identity(ash::mojom::kServiceName),
        ash::mojom::KeyboardController::Name_,
        base::BindRepeating(
            &ArcInputMethodManagerServiceTest::AddKeyboardControllerBinding,
            base::Unretained(this)));
    // Provide the local connector to ChromeKeyboardControllerClient.
    chrome_keyboard_controller_client_ =
        std::make_unique<ChromeKeyboardControllerClient>(connector_.get());
    chrome_keyboard_controller_client_->set_profile_for_test(profile_.get());

    service_ = ArcInputMethodManagerService::GetForBrowserContextForTesting(
        profile_.get());
    test_bridge_ = new TestInputMethodManagerBridge();
    service_->SetInputMethodManagerBridgeForTesting(
        base::WrapUnique(test_bridge_));
  }

  void AddKeyboardControllerBinding(mojo::ScopedMessagePipeHandle handle) {
    ash::Shell::Get()->ash_keyboard_controller()->BindRequest(
        ash::mojom::KeyboardControllerRequest(std::move(handle)));
  }

  void TearDown() override {
    test_bridge_ = nullptr;
    service_->Shutdown();
    profile_.reset(nullptr);
    chrome_keyboard_controller_client_.reset();
    connector_.reset();
    tablet_mode_client_.reset(nullptr);
    chromeos::input_method::InputMethodManager::Shutdown();
    ui::IMEBridge::Shutdown();
    ash::AshTestBase::TearDown();
  }

 private:
  content::TestServiceManagerContext service_manager_context_;
  std::unique_ptr<service_manager::Connector> connector_;
  std::unique_ptr<ArcServiceManager> arc_service_manager_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<TabletModeClient> tablet_mode_client_;
  std::unique_ptr<ChromeKeyboardControllerClient>
      chrome_keyboard_controller_client_;
  TestInputMethodManager* input_method_manager_ = nullptr;
  TestInputMethodManagerBridge* test_bridge_ = nullptr;  // Owned by |service_|
  ArcInputMethodManagerService* service_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(ArcInputMethodManagerServiceTest);
};

}  // anonymous namespace

TEST_F(ArcInputMethodManagerServiceTest, ConstructAndDestruct) {
  // These two method are not implemented yet.
  ASSERT_TRUE(service() != nullptr);
  service()->OnActiveImeChanged("");
  service()->OnImeInfoChanged({});
  SUCCEED();
}

TEST_F(ArcInputMethodManagerServiceTest, EnableIme) {
  namespace ceiu = chromeos::extension_ime_util;
  using crx_file::id_util::GenerateId;

  base::test::ScopedFeatureList feature;
  feature.InitAndEnableFeature(kEnableInputMethodFeature);
  ToggleTabletMode(true);

  ASSERT_EQ(0u, bridge()->enable_ime_calls_.size());

  const std::string extension_ime_id =
      ceiu::GetInputMethodID(GenerateId("test.extension.ime"), "us");
  const std::string component_extension_ime_id =
      ceiu::GetComponentInputMethodID(
          GenerateId("test.component.extension.ime"), "us");
  const std::string arc_ime_id =
      ceiu::GetArcInputMethodID(GenerateId("test.arc.ime"), "us");

  // EnableIme is called only when ARC IME is enable or disabled.
  imm()->state()->AddActiveInputMethodId(extension_ime_id);
  service()->ImeMenuListChanged();
  EXPECT_EQ(0u, bridge()->enable_ime_calls_.size());

  imm()->state()->AddActiveInputMethodId(component_extension_ime_id);
  service()->ImeMenuListChanged();
  EXPECT_EQ(0u, bridge()->enable_ime_calls_.size());

  // Enable the ARC IME and verify that EnableIme is called.
  imm()->state()->AddActiveInputMethodId(arc_ime_id);
  service()->ImeMenuListChanged();
  ASSERT_EQ(1u, bridge()->enable_ime_calls_.size());
  EXPECT_EQ(ceiu::GetComponentIDByInputMethodID(arc_ime_id),
            std::get<std::string>(bridge()->enable_ime_calls_[0]));
  EXPECT_TRUE(std::get<bool>(bridge()->enable_ime_calls_[0]));

  // Disable the ARC IME and verify that EnableIme is called with false.
  imm()->state()->RemoveActiveInputMethodId(arc_ime_id);
  service()->ImeMenuListChanged();
  ASSERT_EQ(2u, bridge()->enable_ime_calls_.size());
  EXPECT_EQ(ceiu::GetComponentIDByInputMethodID(arc_ime_id),
            std::get<std::string>(bridge()->enable_ime_calls_[1]));
  EXPECT_FALSE(std::get<bool>(bridge()->enable_ime_calls_[1]));

  // EnableIme is not called when non ARC IME is disabled.
  imm()->state()->RemoveActiveInputMethodId(extension_ime_id);
  service()->ImeMenuListChanged();
  EXPECT_EQ(2u, bridge()->enable_ime_calls_.size());
}

TEST_F(ArcInputMethodManagerServiceTest, EnableIme_WithPrefs) {
  namespace ceiu = chromeos::extension_ime_util;
  using crx_file::id_util::GenerateId;

  base::test::ScopedFeatureList feature;
  feature.InitAndEnableFeature(kEnableInputMethodFeature);
  ToggleTabletMode(true);

  ASSERT_EQ(0u, bridge()->enable_ime_calls_.size());

  const std::string component_extension_ime_id =
      ceiu::GetComponentInputMethodID(
          GenerateId("test.component.extension.ime"), "us");
  const std::string arc_ime_id =
      ceiu::GetArcInputMethodID(GenerateId("test.arc.ime"), "us");

  imm()->state()->AddActiveInputMethodId(component_extension_ime_id);
  service()->ImeMenuListChanged();
  EXPECT_EQ(0u, bridge()->enable_ime_calls_.size());

  imm()->state()->AddActiveInputMethodId(arc_ime_id);
  service()->ImeMenuListChanged();
  ASSERT_EQ(1u, bridge()->enable_ime_calls_.size());

  // Test the case where |arc_ime_id| is temporarily disallowed because of the
  // toggling to the laptop mode. In that case, the prefs still have the IME's
  // ID.
  profile()->GetPrefs()->SetString(
      prefs::kLanguageEnabledImes,
      base::StringPrintf("%s,%s", component_extension_ime_id.c_str(),
                         arc_ime_id.c_str()));
  imm()->state()->RemoveActiveInputMethodId(arc_ime_id);
  service()->ImeMenuListChanged();
  // Verify that EnableIme(id, false) is NOT called.
  EXPECT_EQ(1u, bridge()->enable_ime_calls_.size());  // still 1u, not 2u.
}

TEST_F(ArcInputMethodManagerServiceTest, SwitchImeTo) {
  namespace ceiu = chromeos::extension_ime_util;
  using crx_file::id_util::GenerateId;

  const std::string arc_ime_service_id =
      "org.chromium.arc.ime/.ArcInputMethodService";

  base::test::ScopedFeatureList feature;
  feature.InitAndEnableFeature(kEnableInputMethodFeature);
  ToggleTabletMode(true);

  ASSERT_EQ(0u, bridge()->switch_ime_to_calls_.size());

  const std::string extension_ime_id =
      ceiu::GetInputMethodID(GenerateId("test.extension.ime"), "us");
  const std::string component_extension_ime_id =
      ceiu::GetComponentInputMethodID(
          GenerateId("test.component.extension.ime"), "us");
  const std::string arc_ime_id = ceiu::GetArcInputMethodID(
      GenerateId("test.arc.ime"), "ime.id.in.arc.container");

  // Set active input method to the extension ime.
  imm()->state()->SetActiveInputMethod(extension_ime_id);
  service()->InputMethodChanged(imm(), nullptr, false /* show_message */);
  // ArcImeService should be selected.
  ASSERT_EQ(1u, bridge()->switch_ime_to_calls_.size());
  EXPECT_EQ(arc_ime_service_id, bridge()->switch_ime_to_calls_[0]);

  // Set active input method to the component extension ime.
  imm()->state()->SetActiveInputMethod(component_extension_ime_id);
  service()->InputMethodChanged(imm(), nullptr, false /* show_message */);
  // ArcImeService should be selected.
  ASSERT_EQ(2u, bridge()->switch_ime_to_calls_.size());
  EXPECT_EQ(arc_ime_service_id, bridge()->switch_ime_to_calls_[1]);

  // Set active input method to the arc ime.
  imm()->state()->SetActiveInputMethod(arc_ime_id);
  service()->InputMethodChanged(imm(), nullptr, false /* show_message */);
  ASSERT_EQ(3u, bridge()->switch_ime_to_calls_.size());
  EXPECT_EQ("ime.id.in.arc.container", bridge()->switch_ime_to_calls_[2]);
}

TEST_F(ArcInputMethodManagerServiceTest, OnImeDisabled) {
  namespace ceiu = chromeos::extension_ime_util;

  base::test::ScopedFeatureList feature;
  feature.InitAndEnableFeature(kEnableInputMethodFeature);

  constexpr char kNonArcIme[] = "ime_a";
  constexpr char kArcImeX[] = "arc_ime_x";
  constexpr char kArcImeY[] = "arc_ime_y";
  constexpr char kArcIMEProxyExtensionName[] =
      "org.chromium.arc.inputmethod.proxy";

  const std::string proxy_ime_extension_id =
      crx_file::id_util::GenerateId(kArcIMEProxyExtensionName);
  const std::string arc_ime_x_component =
      ceiu::GetArcInputMethodID(proxy_ime_extension_id, kArcImeX);
  const std::string arc_ime_y_component =
      ceiu::GetArcInputMethodID(proxy_ime_extension_id, kArcImeY);

  // Enable one non-ARC IME, then remove an ARC IME. This usually does not
  // happen, but confirm that OnImeDisabled() does not do anything bad even
  // if the IPC is called that way.
  profile()->GetPrefs()->SetString(prefs::kLanguageEnabledImes, kNonArcIme);
  service()->OnImeDisabled(kArcImeX);
  EXPECT_EQ(kNonArcIme,
            profile()->GetPrefs()->GetString(prefs::kLanguageEnabledImes));

  // Enable two IMEs (one non-ARC and one ARC), remove the ARC IME, and then
  // confirm the non-ARC one remains.
  std::string pref_str =
      base::StringPrintf("%s,%s", kNonArcIme, arc_ime_x_component.c_str());
  profile()->GetPrefs()->SetString(prefs::kLanguageEnabledImes, pref_str);
  service()->OnImeDisabled(kArcImeX);
  EXPECT_EQ(kNonArcIme,
            profile()->GetPrefs()->GetString(prefs::kLanguageEnabledImes));

  // Enable two ARC IMEs along with one non-ARC one, remove one of two ARC IMEs,
  // then confirm one non-ARC IME and one ARC IME still remain.
  pref_str = base::StringPrintf("%s,%s,%s", arc_ime_x_component.c_str(),
                                kNonArcIme, arc_ime_y_component.c_str());
  profile()->GetPrefs()->SetString(prefs::kLanguageEnabledImes, pref_str);
  service()->OnImeDisabled(kArcImeX);
  pref_str =
      base::StringPrintf("%s,%s", kNonArcIme, arc_ime_y_component.c_str());
  EXPECT_EQ(pref_str,
            profile()->GetPrefs()->GetString(prefs::kLanguageEnabledImes));
}

TEST_F(ArcInputMethodManagerServiceTest, OnImeInfoChanged) {
  namespace ceiu = chromeos::extension_ime_util;

  base::test::ScopedFeatureList feature;
  feature.InitAndEnableFeature(kEnableInputMethodFeature);
  ToggleTabletMode(true);

  // Preparing 2 ImeInfo.
  const std::string android_ime_id1 = "test.arc.ime";
  const std::string display_name1 = "DisplayName";
  const std::string settings_url1 = "url_to_settings";
  mojom::ImeInfoPtr info1 = mojom::ImeInfo::New();
  info1->ime_id = android_ime_id1;
  info1->display_name = display_name1;
  info1->enabled = false;
  info1->settings_url = settings_url1;

  const std::string android_ime_id2 = "test.arc.ime2";
  const std::string display_name2 = "DisplayName2";
  const std::string settings_url2 = "url_to_settings2";
  mojom::ImeInfoPtr info2 = mojom::ImeInfo::New();
  info2->ime_id = android_ime_id2;
  info2->display_name = display_name2;
  info2->enabled = true;
  info2->settings_url = settings_url2;

  std::vector<
      std::tuple<std::string, chromeos::input_method::InputMethodDescriptors,
                 ui::IMEEngineHandlerInterface*>>& added_extensions =
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
    EXPECT_EQ(android_ime_id1, ceiu::GetComponentIDByInputMethodID(
                                   std::get<1>(added_extensions[0])[0].id()));
    EXPECT_EQ(display_name1, std::get<1>(added_extensions[0])[0].name());
    ASSERT_EQ(1u, std::get<1>(added_extensions[0])[0].language_codes().size());
    EXPECT_TRUE(chromeos::extension_ime_util::IsLanguageForArcIME(
        (std::get<1>(added_extensions[0])[0].language_codes())[0]));

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
    EXPECT_EQ(android_ime_id1, ceiu::GetComponentIDByInputMethodID(
                                   std::get<1>(added_extensions[0])[0].id()));
    EXPECT_EQ(display_name1, std::get<1>(added_extensions[0])[0].name());
    EXPECT_EQ(android_ime_id2, ceiu::GetComponentIDByInputMethodID(
                                   std::get<1>(added_extensions[0])[1].id()));
    EXPECT_EQ(display_name2, std::get<1>(added_extensions[0])[1].name());

    // Already enabled IME should be added to the pref automatically.
    const std::string& arc_ime_id2 = std::get<1>(added_extensions[0])[1].id();
    EXPECT_EQ(arc_ime_id2,
              profile()->GetPrefs()->GetString(prefs::kLanguageEnabledImes));

    added_extensions.clear();
  }
}

TEST_F(ArcInputMethodManagerServiceTest, AllowArcIMEsOnlyInTabletMode) {
  namespace ceiu = chromeos::extension_ime_util;
  using crx_file::id_util::GenerateId;

  base::test::ScopedFeatureList feature;
  feature.InitAndEnableFeature(kEnableInputMethodFeature);

  const std::string extension_ime_id =
      ceiu::GetInputMethodID(GenerateId("test.extension.ime"), "us");
  const std::string component_extension_ime_id =
      ceiu::GetComponentInputMethodID(
          GenerateId("test.component.extension.ime"), "us");
  const std::string arc_ime_id =
      ceiu::GetArcInputMethodID(GenerateId("test.arc.ime"), "us");

  // Start from tablet mode.
  ToggleTabletMode(true);

  // Activate 3 IMEs.
  imm()->state()->AddActiveInputMethodId(extension_ime_id);
  imm()->state()->AddActiveInputMethodId(component_extension_ime_id);
  imm()->state()->AddActiveInputMethodId(arc_ime_id);

  // Update the prefs because the testee checks them. Note that toggling the
  // mode never changes the prefs.
  profile()->GetPrefs()->SetString(
      prefs::kLanguageEnabledImes,
      base::StringPrintf("%s,%s,%s", extension_ime_id.c_str(),
                         component_extension_ime_id.c_str(),
                         arc_ime_id.c_str()));

  // All IMEs are allowed to use.
  EXPECT_TRUE(imm()->state()->IsInputMethodAllowed(extension_ime_id));
  EXPECT_TRUE(imm()->state()->IsInputMethodAllowed(component_extension_ime_id));
  EXPECT_TRUE(imm()->state()->IsInputMethodAllowed(arc_ime_id));

  // Change to laptop mode.
  ToggleTabletMode(false);

  // ARC IME is not allowed in laptop mode.
  EXPECT_TRUE(imm()->state()->IsInputMethodAllowed(extension_ime_id));
  EXPECT_TRUE(imm()->state()->IsInputMethodAllowed(component_extension_ime_id));
  EXPECT_FALSE(imm()->state()->IsInputMethodAllowed(arc_ime_id));

  // Back to tablet mode.
  EXPECT_TRUE(imm()->state()->enabled_input_methods_.empty());
  ToggleTabletMode(true);

  // All IMEs are allowed to use.
  EXPECT_TRUE(imm()->state()->IsInputMethodAllowed(extension_ime_id));
  EXPECT_TRUE(imm()->state()->IsInputMethodAllowed(component_extension_ime_id));
  EXPECT_TRUE(imm()->state()->IsInputMethodAllowed(arc_ime_id));

  // Verify they appear in the CrOS IME menu.
  ASSERT_EQ(1u, imm()->state()->enabled_input_methods_.size());
  EXPECT_EQ(arc_ime_id, imm()->state()->enabled_input_methods_[0]);

  // Do the same tests again, but with |extension_ime_id| disabled.
  imm()->state()->SetAllowedInputMethods(
      {component_extension_ime_id, arc_ime_id},
      false /* enable_allowed_input_methods */);
  ToggleTabletMode(false);

  EXPECT_FALSE(imm()->state()->IsInputMethodAllowed(extension_ime_id));
  EXPECT_TRUE(imm()->state()->IsInputMethodAllowed(component_extension_ime_id));
  EXPECT_FALSE(imm()->state()->IsInputMethodAllowed(arc_ime_id));

  ToggleTabletMode(true);

  EXPECT_FALSE(imm()->state()->IsInputMethodAllowed(extension_ime_id));
  EXPECT_TRUE(imm()->state()->IsInputMethodAllowed(component_extension_ime_id));
  EXPECT_TRUE(imm()->state()->IsInputMethodAllowed(arc_ime_id));

  // Confirm that entering the same mode twice in a row is no-op.
  ToggleTabletMode(true);

  EXPECT_FALSE(imm()->state()->IsInputMethodAllowed(extension_ime_id));
  EXPECT_TRUE(imm()->state()->IsInputMethodAllowed(component_extension_ime_id));
  EXPECT_TRUE(imm()->state()->IsInputMethodAllowed(arc_ime_id));

  ToggleTabletMode(false);

  EXPECT_FALSE(imm()->state()->IsInputMethodAllowed(extension_ime_id));
  EXPECT_TRUE(imm()->state()->IsInputMethodAllowed(component_extension_ime_id));
  EXPECT_FALSE(imm()->state()->IsInputMethodAllowed(arc_ime_id));

  ToggleTabletMode(false);

  EXPECT_FALSE(imm()->state()->IsInputMethodAllowed(extension_ime_id));
  EXPECT_TRUE(imm()->state()->IsInputMethodAllowed(component_extension_ime_id));
  EXPECT_FALSE(imm()->state()->IsInputMethodAllowed(arc_ime_id));
}

TEST_F(ArcInputMethodManagerServiceTest,
       DisallowArcIMEsWhenAccessibilityKeyboardEnabled) {
  namespace ceiu = chromeos::extension_ime_util;
  using crx_file::id_util::GenerateId;

  base::test::ScopedFeatureList feature;
  feature.InitAndEnableFeature(kEnableInputMethodFeature);

  const std::string extension_ime_id =
      ceiu::GetInputMethodID(GenerateId("test.extension.ime"), "us");
  const std::string component_extension_ime_id =
      ceiu::GetComponentInputMethodID(
          GenerateId("test.component.extension.ime"), "us");
  const std::string arc_ime_id =
      ceiu::GetArcInputMethodID(GenerateId("test.arc.ime"), "us");

  // Start from tablet mode.
  ToggleTabletMode(true);

  // Activate 3 IMEs.
  imm()->state()->AddActiveInputMethodId(extension_ime_id);
  imm()->state()->AddActiveInputMethodId(component_extension_ime_id);
  imm()->state()->AddActiveInputMethodId(arc_ime_id);

  // Update the prefs because the testee checks them. Note that toggling the
  // mode never changes the prefs.
  profile()->GetPrefs()->SetString(
      prefs::kLanguageEnabledImes,
      base::StringPrintf("%s,%s,%s", extension_ime_id.c_str(),
                         component_extension_ime_id.c_str(),
                         arc_ime_id.c_str()));

  // All IMEs are allowed to use.
  EXPECT_TRUE(imm()->state()->IsInputMethodAllowed(extension_ime_id));
  EXPECT_TRUE(imm()->state()->IsInputMethodAllowed(component_extension_ime_id));
  EXPECT_TRUE(imm()->state()->IsInputMethodAllowed(arc_ime_id));

  // Enable a11y keyboard option.
  profile()->GetPrefs()->SetBoolean(
      ash::prefs::kAccessibilityVirtualKeyboardEnabled, true);
  // Notify ArcInputMethodManagerService.
  service()->OnAccessibilityStatusChanged(
      {chromeos::ACCESSIBILITY_TOGGLE_VIRTUAL_KEYBOARD, true});

  // ARC IME is not allowed.
  EXPECT_TRUE(imm()->state()->IsInputMethodAllowed(extension_ime_id));
  EXPECT_TRUE(imm()->state()->IsInputMethodAllowed(component_extension_ime_id));
  EXPECT_FALSE(imm()->state()->IsInputMethodAllowed(arc_ime_id));

  // Disable a11y keyboard option.
  profile()->GetPrefs()->SetBoolean(
      ash::prefs::kAccessibilityVirtualKeyboardEnabled, false);
  // Notify ArcInputMethodManagerService.
  service()->OnAccessibilityStatusChanged(
      {chromeos::ACCESSIBILITY_TOGGLE_VIRTUAL_KEYBOARD, false});

  EXPECT_TRUE(imm()->state()->IsInputMethodAllowed(extension_ime_id));
  EXPECT_TRUE(imm()->state()->IsInputMethodAllowed(component_extension_ime_id));
  EXPECT_TRUE(imm()->state()->IsInputMethodAllowed(arc_ime_id));
}

TEST_F(ArcInputMethodManagerServiceTest, FocusAndBlur) {
  base::test::ScopedFeatureList feature;
  feature.InitAndEnableFeature(kEnableInputMethodFeature);
  ToggleTabletMode(true);

  // Adding one ARC IME.
  {
    const std::string android_ime_id = "test.arc.ime";
    const std::string display_name = "DisplayName";
    const std::string settings_url = "url_to_settings";
    mojom::ImeInfoPtr info = mojom::ImeInfo::New();
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
  ui::IMEEngineHandlerInterface* engine_handler =
      std::get<2>(imm()->state()->added_input_method_extensions_.at(0));

  // Set up mock input context.
  constexpr int test_context_id = 0;
  const ui::IMEEngineHandlerInterface::InputContext test_context{
      test_context_id,
      ui::TEXT_INPUT_TYPE_TEXT,
      ui::TEXT_INPUT_MODE_DEFAULT,
      0 /* flags */,
      ui::TextInputClient::FOCUS_REASON_MOUSE,
      true /* should_do_learning */};
  ui::MockInputMethod mock_input_method(nullptr);
  TestIMEInputContextHandler test_context_handler(&mock_input_method);
  ui::DummyTextInputClient dummy_text_input_client(ui::TEXT_INPUT_TYPE_TEXT);
  ui::IMEBridge::Get()->SetInputContextHandler(&test_context_handler);

  // Enable the ARC IME.
  ui::IMEBridge::Get()->SetCurrentEngineHandler(engine_handler);
  engine_handler->Enable(
      chromeos::extension_ime_util::GetComponentIDByInputMethodID(
          std::get<1>(imm()->state()->added_input_method_extensions_.at(0))
              .at(0)
              .id()));
  mock_input_method.SetFocusedTextInputClient(&dummy_text_input_client);

  ASSERT_EQ(0, bridge()->focus_calls_count_);

  engine_handler->FocusIn(test_context);
  EXPECT_EQ(1, bridge()->focus_calls_count_);

  bridge()->update_text_input_state_calls_count_ = 0;

  engine_handler->SetCompositionBounds({});
  EXPECT_EQ(1, bridge()->update_text_input_state_calls_count_);
  EXPECT_FALSE(bridge()->last_text_input_state->first_update_after_operation);

  engine_handler->SetSurroundingText("", 0, 0, 0);
  EXPECT_EQ(2, bridge()->update_text_input_state_calls_count_);
  EXPECT_FALSE(bridge()->last_text_input_state->first_update_after_operation);

  engine_handler->FocusOut();
  EXPECT_EQ(1, bridge()->focus_calls_count_);

  mock_input_method.DetachTextInputClient(&dummy_text_input_client);
  ui::IMEBridge::Get()->SetInputContextHandler(nullptr);
}

TEST_F(ArcInputMethodManagerServiceTest, IMEOperations) {
  base::test::ScopedFeatureList feature;
  feature.InitAndEnableFeature(kEnableInputMethodFeature);
  ToggleTabletMode(true);

  // Adding one ARC IME.
  {
    const std::string android_ime_id = "test.arc.ime";
    const std::string display_name = "DisplayName";
    const std::string settings_url = "url_to_settings";
    mojom::ImeInfoPtr info = mojom::ImeInfo::New();
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
  ui::IMEEngineHandlerInterface* engine_handler =
      std::get<2>(imm()->state()->added_input_method_extensions_.at(0));

  // Set up mock input context.
  constexpr int test_context_id = 0;
  const ui::IMEEngineHandlerInterface::InputContext test_context{
      test_context_id,
      ui::TEXT_INPUT_TYPE_TEXT,
      ui::TEXT_INPUT_MODE_DEFAULT,
      0 /* flags */,
      ui::TextInputClient::FOCUS_REASON_MOUSE,
      true /* should_do_learning */};
  ui::MockInputMethod mock_input_method(nullptr);
  TestIMEInputContextHandler test_context_handler(&mock_input_method);
  ui::DummyTextInputClient dummy_text_input_client(ui::TEXT_INPUT_TYPE_TEXT);
  ui::IMEBridge::Get()->SetInputContextHandler(&test_context_handler);

  // Enable the ARC IME.
  ui::IMEBridge::Get()->SetCurrentEngineHandler(engine_handler);
  engine_handler->Enable(
      chromeos::extension_ime_util::GetComponentIDByInputMethodID(
          std::get<1>(imm()->state()->added_input_method_extensions_.at(0))
              .at(0)
              .id()));
  mock_input_method.SetFocusedTextInputClient(&dummy_text_input_client);

  engine_handler->FocusIn(test_context);
  bridge()->update_text_input_state_calls_count_ = 0;

  InputConnectionImpl* connection = service()->GetInputConnectionForTesting();
  ASSERT_NE(nullptr, connection);
  connection->CommitText(base::ASCIIToUTF16("text"), 0);
  EXPECT_EQ(1, test_context_handler.commit_text_call_count());
  // Trigger an observer method to trigger text input state updating.
  engine_handler->SetSurroundingText("", 0, 0, 0);
  EXPECT_EQ(1, bridge()->update_text_input_state_calls_count_);
  EXPECT_TRUE(bridge()->last_text_input_state->first_update_after_operation);

  // Calling CommitText() with '\n' doesn't invoke
  // InputMethodEngine::CommitText.
  EXPECT_EQ(0, test_context_handler.send_key_event_call_count());
  connection->CommitText(base::ASCIIToUTF16("\n"), 0);
  EXPECT_EQ(1, test_context_handler.commit_text_call_count());
  EXPECT_EQ(2, test_context_handler.send_key_event_call_count());

  test_context_handler.Reset();
  connection->DeleteSurroundingText(1, 1);
  EXPECT_EQ(1, test_context_handler.delete_surrounding_text_call_count());

  // If there is no composing text, FinishComposingText() does nothing.
  test_context_handler.Reset();
  connection->FinishComposingText();
  EXPECT_EQ(0, test_context_handler.commit_text_call_count());

  // If there is composing text, FinishComposingText() calls CommitText() with
  // the text.
  connection->SetComposingText(base::ASCIIToUTF16("composing"), 0,
                               base::nullopt);
  EXPECT_EQ(0, test_context_handler.commit_text_call_count());
  connection->FinishComposingText();
  EXPECT_EQ(1, test_context_handler.commit_text_call_count());

  base::string16 text = base::ASCIIToUTF16("text");
  test_context_handler.Reset();
  connection->SetComposingText(text, 0, base::nullopt);
  EXPECT_EQ(1, test_context_handler.update_preedit_text_call_count());
  EXPECT_EQ(
      text,
      test_context_handler.last_update_composition_arg().composition_text.text);
  EXPECT_EQ(3u, test_context_handler.last_update_composition_arg()
                    .composition_text.selection.start());
  // Committing the composing text calls ClearComposition() and CommitText().
  connection->CommitText(base::ASCIIToUTF16("text"), 0);
  EXPECT_EQ(2, test_context_handler.update_preedit_text_call_count());
  EXPECT_EQ(
      base::ASCIIToUTF16(""),
      test_context_handler.last_update_composition_arg().composition_text.text);
  EXPECT_EQ(1, test_context_handler.commit_text_call_count());

  // CommitText should clear the composing text.
  connection->FinishComposingText();
  // commit_text_call_count() doesn't change.
  EXPECT_EQ(1, test_context_handler.commit_text_call_count());

  test_context_handler.Reset();
  connection->SetComposingText(text, 0, base::make_optional<gfx::Range>(1, 3));
  EXPECT_EQ(1u, test_context_handler.last_update_composition_arg()
                    .composition_text.selection.start());
  EXPECT_EQ(3u, test_context_handler.last_update_composition_arg()
                    .composition_text.selection.end());

  engine_handler->FocusOut();

  mock_input_method.DetachTextInputClient(&dummy_text_input_client);
  ui::IMEBridge::Get()->SetInputContextHandler(nullptr);
}

TEST_F(ArcInputMethodManagerServiceTest, DisableFallbackVirtualKeyboard) {
  namespace ceiu = chromeos::extension_ime_util;
  using crx_file::id_util::GenerateId;

  base::test::ScopedFeatureList feature;
  feature.InitAndEnableFeature(kEnableInputMethodFeature);
  ToggleTabletMode(true);

  const std::string extension_ime_id =
      ceiu::GetInputMethodID(GenerateId("test.extension.ime"), "us");
  const std::string component_extension_ime_id =
      ceiu::GetComponentInputMethodID(
          GenerateId("test.component.extension.ime"), "us");
  const std::string arc_ime_id = ceiu::GetArcInputMethodID(
      GenerateId("test.arc.ime"), "ime.id.in.arc.container");

  // Set active input method to the extension ime.
  imm()->state()->SetActiveInputMethod(extension_ime_id);
  service()->InputMethodChanged(imm(), profile(), false /* show_message */);

  // Enable Chrome OS virtual keyboard
  auto* client = ChromeKeyboardControllerClient::Get();
  client->ClearEnableFlag(
      keyboard::mojom::KeyboardEnableFlag::kAndroidDisabled);
  client->SetEnableFlag(keyboard::mojom::KeyboardEnableFlag::kTouchEnabled);
  client->FlushForTesting();
  base::RunLoop().RunUntilIdle();  // Allow observers to fire and process.
  ASSERT_TRUE(client->is_keyboard_enabled());

  // It's disabled when the ARC IME is activated.
  imm()->state()->SetActiveInputMethod(arc_ime_id);
  service()->InputMethodChanged(imm(), profile(), false);
  client->FlushForTesting();
  EXPECT_FALSE(client->is_keyboard_enabled());

  // It's re-enabled when the ARC IME is deactivated.
  imm()->state()->SetActiveInputMethod(component_extension_ime_id);
  service()->InputMethodChanged(imm(), profile(), false);
  client->FlushForTesting();
  EXPECT_TRUE(client->is_keyboard_enabled());
}

TEST_F(ArcInputMethodManagerServiceTest, ShowVirtualKeyboard) {
  base::test::ScopedFeatureList feature;
  feature.InitAndEnableFeature(kEnableInputMethodFeature);
  ToggleTabletMode(true);

  // Adding one ARC IME.
  {
    const std::string android_ime_id = "test.arc.ime";
    const std::string display_name = "DisplayName";
    const std::string settings_url = "url_to_settings";
    mojom::ImeInfoPtr info = mojom::ImeInfo::New();
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
  ui::IMEEngineHandlerInterface* engine_handler =
      std::get<2>(imm()->state()->added_input_method_extensions_.at(0));

  // Set up mock input context.
  constexpr int test_context_id = 0;
  const ui::IMEEngineHandlerInterface::InputContext test_context{
      test_context_id,
      ui::TEXT_INPUT_TYPE_TEXT,
      ui::TEXT_INPUT_MODE_DEFAULT,
      0 /* flags */,
      ui::TextInputClient::FOCUS_REASON_MOUSE,
      true /* should_do_learning */};
  ui::MockInputMethod mock_input_method(nullptr);
  TestIMEInputContextHandler test_context_handler(&mock_input_method);
  ui::DummyTextInputClient dummy_text_input_client(ui::TEXT_INPUT_TYPE_TEXT);
  ui::IMEBridge::Get()->SetInputContextHandler(&test_context_handler);

  // Enable the ARC IME.
  ui::IMEBridge::Get()->SetCurrentEngineHandler(engine_handler);
  engine_handler->Enable(
      chromeos::extension_ime_util::GetComponentIDByInputMethodID(
          std::get<1>(imm()->state()->added_input_method_extensions_.at(0))
              .at(0)
              .id()));

  mock_input_method.SetFocusedTextInputClient(&dummy_text_input_client);

  EXPECT_EQ(0, bridge()->show_virtual_keyboard_calls_count_);
  mock_input_method.ShowVirtualKeyboardIfEnabled();
  EXPECT_EQ(1, bridge()->show_virtual_keyboard_calls_count_);
  ui::IMEBridge::Get()->SetInputContextHandler(nullptr);
  ui::IMEBridge::Get()->SetCurrentEngineHandler(nullptr);
}

}  // namespace arc
