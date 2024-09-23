// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/input_method/input_method_manager_impl.h"

#include <stddef.h>

#include <algorithm>
#include <memory>
#include <optional>
#include <utility>

#include "ash/public/cpp/ime_controller.h"
#include "base/compiler_specific.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/i18n/string_compare.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "chrome/browser/ash/input_method/mock_candidate_window_controller.h"
#include "chrome/browser/ash/input_method/mock_input_method_engine.h"
#include "chrome/browser/ash/input_method/test_ime_controller.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ash/input_method/ime_controller_client_impl.h"
#include "chrome/browser/ui/ash/keyboard/chrome_keyboard_controller_client_test_helper.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/components/kiosk/kiosk_test_utils.h"
#include "components/account_id/account_id.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/icu/source/common/unicode/uloc.h"
#include "third_party/icu/source/i18n/unicode/coll.h"
#include "ui/base/ime/ash/component_extension_ime_manager.h"
#include "ui/base/ime/ash/extension_ime_util.h"
#include "ui/base/ime/ash/fake_ime_keyboard.h"
#include "ui/base/ime/ash/fake_input_method_delegate.h"
#include "ui/base/ime/ash/ime_bridge.h"
#include "ui/base/ime/ash/mock_component_extension_ime_manager_delegate.h"
#include "ui/base/ime/ash/mock_ime_engine_handler.h"
#include "ui/base/ime/init/input_method_initializer.h"
#include "ui/base/ui_base_features.h"

namespace ash {
namespace input_method {
namespace {

const char kNaclMozcUsId[] = "nacl_mozc_us";
const char kNaclMozcJpId[] = "nacl_mozc_jp";
const char kExt2Engine1Id[] = "ext2_engine1-t-i0-engine_id";
const char kExt2Engine2Id[] = "ext2_engine2-t-i0-engine_id";
const char kPinyinImeId[] = "zh-t-i0-pinyin";
const char kExtensionId1[] = "00000000000000000000000000000000";
const char kExtensionId2[] = "11111111111111111111111111111111";

// Returns true if |descriptors| contain |target|.
bool Contain(const InputMethodDescriptors& descriptors,
             const InputMethodDescriptor& target) {
  for (const auto& descriptor : descriptors) {
    if (descriptor.id() == target.id()) {
      return true;
    }
  }
  return false;
}

std::string ImeIdFromEngineId(const std::string& id) {
  return extension_ime_util::GetInputMethodIDByEngineID(id);
}

class TestObserver : public InputMethodManager::Observer,
                     public ui::ime::InputMethodMenuManager::Observer {
 public:
  TestObserver()
      : input_method_changed_count_(0),
        input_method_extension_added_count_(0),
        input_method_extension_removed_count_(0),
        input_method_menu_item_changed_count_(0),
        last_show_message_(false) {}

  TestObserver(const TestObserver&) = delete;
  TestObserver& operator=(const TestObserver&) = delete;

  ~TestObserver() override = default;

  void InputMethodChanged(InputMethodManager* manager,
                          Profile* /* profile */,
                          bool show_message) override {
    ++input_method_changed_count_;
    last_show_message_ = show_message;
  }

  void OnInputMethodExtensionAdded(const std::string& id) override {
    ++input_method_extension_added_count_;
  }

  void OnInputMethodExtensionRemoved(const std::string& id) override {
    ++input_method_extension_removed_count_;
  }

  void InputMethodMenuItemChanged(
      ui::ime::InputMethodMenuManager* manager) override {
    ++input_method_menu_item_changed_count_;
  }

  int input_method_changed_count_;
  int input_method_extension_added_count_;
  int input_method_extension_removed_count_;
  int input_method_menu_item_changed_count_;
  bool last_show_message_;
};

class TestCandidateWindowObserver
    : public InputMethodManager::CandidateWindowObserver {
 public:
  TestCandidateWindowObserver()
      : candidate_window_opened_count_(0), candidate_window_closed_count_(0) {}

  TestCandidateWindowObserver(const TestCandidateWindowObserver&) = delete;
  TestCandidateWindowObserver& operator=(const TestCandidateWindowObserver&) =
      delete;

  ~TestCandidateWindowObserver() override = default;

  void CandidateWindowOpened(InputMethodManager* manager) override {
    ++candidate_window_opened_count_;
  }
  void CandidateWindowClosed(InputMethodManager* manager) override {
    ++candidate_window_closed_count_;
  }

  int candidate_window_opened_count_;
  int candidate_window_closed_count_;
};
}  // namespace

class InputMethodManagerImplTest : public BrowserWithTestWindowTest {
 public:
  InputMethodManagerImplTest() = default;

  InputMethodManagerImplTest(const InputMethodManagerImplTest&) = delete;
  InputMethodManagerImplTest& operator=(const InputMethodManagerImplTest&) =
      delete;

  ~InputMethodManagerImplTest() override = default;

  void SetUp() override {
    std::vector<ComponentExtensionIME> ime_list;
    InitImeList(ime_list);

    std::set<std::string> login_layout_set = {"us",
                                              "us(intl)",
                                              "us(altgr-intl)",
                                              "us(dvorak)",
                                              "us(dvp)",
                                              "us(colemak)",
                                              "us(workman)",
                                              "us(workman-intl)",
                                              "fr",
                                              "se",
                                              "jp",
                                              "hu",
                                              "de"};

    auto mock_delegate =
        std::make_unique<MockComponentExtensionIMEManagerDelegate>();
    mock_delegate->set_ime_list(ime_list);
    mock_delegate->set_login_layout_set(login_layout_set);

    auto fake_keyboard = std::make_unique<FakeImeKeyboard>();
    keyboard_ = fake_keyboard.get();

    manager_ = new InputMethodManagerImpl(
        std::make_unique<FakeInputMethodDelegate>(), std::move(mock_delegate),
        false, std::move(fake_keyboard));
    manager_->GetInputMethodUtil()->UpdateHardwareLayoutCache();
    candidate_window_controller_ = new MockCandidateWindowController;
    manager_->SetCandidateWindowControllerForTesting(
        candidate_window_controller_);
    mock_engine_handler_ = std::make_unique<MockInputMethodEngine>();
    IMEBridge::Get()->SetCurrentEngineHandler(mock_engine_handler_.get());

    menu_manager_ = ui::ime::InputMethodMenuManager::GetInstance();

    // Let the global pointer own manager_. Components in ash need to be
    // able to call InputMethodManager::Get() during initialization. Cleanup
    // the pointer by calling ShutDown() in TearDown().
    InputMethodManager::Initialize(manager_);
    BrowserWithTestWindowTest::SetUp();

    // Needs ash::Shell keyboard to be created first.
    chrome_keyboard_controller_client_test_helper_ =
        ChromeKeyboardControllerClientTestHelper::InitializeForAsh();

    // CreateNewState(nullptr) returns state with non-empty
    // current_input_method. So SetState() triggers ChangeInputMethod().
    InputMethodDescriptors descriptors;
    auto state =
        manager_->CreateNewState(ProfileManager::GetActiveUserProfile());
    state->AddInputMethodExtension(extension_ime_util::kXkbExtensionId,
                                   descriptors, mock_engine_handler_.get());
    state->AddInputMethodExtension(extension_ime_util::kMozcExtensionId,
                                   descriptors, mock_engine_handler_.get());
    state->AddInputMethodExtension(extension_ime_util::kT13nExtensionId,
                                   descriptors, mock_engine_handler_.get());
    manager_->SetState(state);
  }

  void TearDown() override {
    // Needs to destroyed before ash::Shell keyboard.
    chrome_keyboard_controller_client_test_helper_.reset();

    BrowserWithTestWindowTest::TearDown();

    candidate_window_controller_ = nullptr;
    keyboard_ = nullptr;

    // Cleanup the global manager and clear the member pointer.
    InputMethodManager::Shutdown();
    manager_ = nullptr;
  }

 private:
  static void InitImeList(std::vector<ComponentExtensionIME>& ime_list) {
    ime_list.clear();

    ComponentExtensionIME ext_xkb;
    ext_xkb.id = extension_ime_util::kXkbExtensionId;
    ext_xkb.description = "ext_xkb_description";
    ext_xkb.path = base::FilePath("ext_xkb_file_path");

    ComponentExtensionEngine ext_xkb_engine_us;
    ext_xkb_engine_us.engine_id = "xkb:us::eng";
    ext_xkb_engine_us.display_name = "xkb:us::eng";
    ext_xkb_engine_us.language_codes.emplace_back("en-US");
    ext_xkb_engine_us.layout = "us";
    ext_xkb.engines.push_back(ext_xkb_engine_us);

    ComponentExtensionEngine ext_xkb_engine_intl;
    ext_xkb_engine_intl.engine_id = "xkb:us:intl:eng";
    ext_xkb_engine_intl.display_name = "xkb:us:intl:eng";
    ext_xkb_engine_intl.language_codes.emplace_back("en-US");
    ext_xkb_engine_intl.layout = "us(intl)";
    ext_xkb.engines.push_back(ext_xkb_engine_intl);

    ComponentExtensionEngine ext_xkb_engine_altgr_intl;
    ext_xkb_engine_altgr_intl.engine_id = "xkb:us:altgr-intl:eng";
    ext_xkb_engine_altgr_intl.display_name = "xkb:us:altgr-intl:eng";
    ext_xkb_engine_altgr_intl.language_codes.emplace_back("en-US");
    ext_xkb_engine_altgr_intl.layout = "us(altgr-intl)";
    ext_xkb.engines.push_back(ext_xkb_engine_altgr_intl);

    ComponentExtensionEngine ext_xkb_engine_dvorak;
    ext_xkb_engine_dvorak.engine_id = "xkb:us:dvorak:eng";
    ext_xkb_engine_dvorak.display_name = "xkb:us:dvorak:eng";
    ext_xkb_engine_dvorak.language_codes.emplace_back("en-US");
    ext_xkb_engine_dvorak.layout = "us(dvorak)";
    ext_xkb.engines.push_back(ext_xkb_engine_dvorak);

    ComponentExtensionEngine ext_xkb_engine_dvp;
    ext_xkb_engine_dvp.engine_id = "xkb:us:dvp:eng";
    ext_xkb_engine_dvp.display_name = "xkb:us:dvp:eng";
    ext_xkb_engine_dvp.language_codes.emplace_back("en-US");
    ext_xkb_engine_dvp.layout = "us(dvp)";
    ext_xkb.engines.push_back(ext_xkb_engine_dvp);

    ComponentExtensionEngine ext_xkb_engine_colemak;
    ext_xkb_engine_colemak.engine_id = "xkb:us:colemak:eng";
    ext_xkb_engine_colemak.display_name = "xkb:us:colemak:eng";
    ext_xkb_engine_colemak.language_codes.emplace_back("en-US");
    ext_xkb_engine_colemak.layout = "us(colemak)";
    ext_xkb.engines.push_back(ext_xkb_engine_colemak);

    ComponentExtensionEngine ext_xkb_engine_workman;
    ext_xkb_engine_workman.engine_id = "xkb:us:workman:eng";
    ext_xkb_engine_workman.display_name = "xkb:us:workman:eng";
    ext_xkb_engine_workman.language_codes.emplace_back("en-US");
    ext_xkb_engine_workman.layout = "us(workman)";
    ext_xkb.engines.push_back(ext_xkb_engine_workman);

    ComponentExtensionEngine ext_xkb_engine_workman_intl;
    ext_xkb_engine_workman_intl.engine_id = "xkb:us:workman-intl:eng";
    ext_xkb_engine_workman_intl.display_name = "xkb:us:workman-intl:eng";
    ext_xkb_engine_workman_intl.language_codes.emplace_back("en-US");
    ext_xkb_engine_workman_intl.layout = "us(workman-intl)";
    ext_xkb.engines.push_back(ext_xkb_engine_workman_intl);

    ComponentExtensionEngine ext_xkb_engine_fr;
    ext_xkb_engine_fr.engine_id = "xkb:fr::fra";
    ext_xkb_engine_fr.display_name = "xkb:fr::fra";
    ext_xkb_engine_fr.language_codes.emplace_back("fr");
    ext_xkb_engine_fr.layout = "fr";
    ext_xkb.engines.push_back(ext_xkb_engine_fr);

    ComponentExtensionEngine ext_xkb_engine_se;
    ext_xkb_engine_se.engine_id = "xkb:se::swe";
    ext_xkb_engine_se.display_name = "xkb:se::swe";
    ext_xkb_engine_se.language_codes.emplace_back("sv");
    ext_xkb_engine_se.layout = "se";
    ext_xkb.engines.push_back(ext_xkb_engine_se);

    ComponentExtensionEngine ext_xkb_engine_jp;
    ext_xkb_engine_jp.engine_id = "xkb:jp::jpn";
    ext_xkb_engine_jp.display_name = "xkb:jp::jpn";
    ext_xkb_engine_jp.language_codes.emplace_back("ja");
    ext_xkb_engine_jp.layout = "jp";
    ext_xkb.engines.push_back(ext_xkb_engine_jp);

    ComponentExtensionEngine ext_xkb_engine_ru;
    ext_xkb_engine_ru.engine_id = "xkb:ru::rus";
    ext_xkb_engine_ru.display_name = "xkb:ru::rus";
    ext_xkb_engine_ru.language_codes.emplace_back("ru");
    ext_xkb_engine_ru.layout = "ru";
    ext_xkb.engines.push_back(ext_xkb_engine_ru);

    ComponentExtensionEngine ext_xkb_engine_hu;
    ext_xkb_engine_hu.engine_id = "xkb:hu::hun";
    ext_xkb_engine_hu.display_name = "xkb:hu::hun";
    ext_xkb_engine_hu.language_codes.emplace_back("hu");
    ext_xkb_engine_hu.layout = "hu";
    ext_xkb.engines.push_back(ext_xkb_engine_hu);

    ComponentExtensionEngine ext_xkb_engine_de;
    ext_xkb_engine_de.engine_id = "xkb:de::ger";
    ext_xkb_engine_de.display_name = "xkb:de::ger";
    ext_xkb_engine_de.language_codes.emplace_back("de");
    ext_xkb_engine_de.layout = "de";
    ext_xkb.engines.push_back(ext_xkb_engine_de);

    ime_list.push_back(ext_xkb);

    ComponentExtensionIME ext1;
    ext1.id = extension_ime_util::kMozcExtensionId;
    ext1.description = "ext1_description";
    ext1.path = base::FilePath("ext1_file_path");

    ComponentExtensionEngine ext1_engine1;
    ext1_engine1.engine_id = "nacl_mozc_us";
    ext1_engine1.display_name = "ext1_engine_1_display_name";
    ext1_engine1.language_codes.emplace_back("ja");
    ext1_engine1.layout = "us";
    ext1.engines.push_back(ext1_engine1);

    ComponentExtensionEngine ext1_engine2;
    ext1_engine2.engine_id = "nacl_mozc_jp";
    ext1_engine2.display_name = "ext1_engine_1_display_name";
    ext1_engine2.language_codes.emplace_back("ja");
    ext1_engine2.layout = "jp";
    ext1.engines.push_back(ext1_engine2);

    ime_list.push_back(ext1);

    ComponentExtensionIME ext2;
    ext2.id = extension_ime_util::kT13nExtensionId;
    ext2.description = "ext2_description";
    ext2.path = base::FilePath("ext2_file_path");

    ComponentExtensionEngine ext2_engine1;
    ext2_engine1.engine_id = kExt2Engine1Id;
    ext2_engine1.display_name = "ext2_engine_1_display_name";
    ext2_engine1.language_codes.emplace_back("en");
    ext2_engine1.layout = "us";
    ext2.engines.push_back(ext2_engine1);

    ComponentExtensionEngine ext2_engine2;
    ext2_engine2.engine_id = kExt2Engine2Id;
    ext2_engine2.display_name = "ext2_engine_2_display_name";
    ext2_engine2.language_codes.emplace_back("en");
    ext2_engine2.layout = "us(dvorak)";
    ext2.engines.push_back(ext2_engine2);

    ime_list.push_back(ext2);
  }

 protected:
  std::unique_ptr<ChromeKeyboardControllerClientTestHelper>
      chrome_keyboard_controller_client_test_helper_;
  raw_ptr<InputMethodManagerImpl, DanglingUntriaged> manager_ = nullptr;
  raw_ptr<MockCandidateWindowController> candidate_window_controller_ = nullptr;
  std::unique_ptr<MockInputMethodEngine> mock_engine_handler_;
  raw_ptr<FakeImeKeyboard> keyboard_ = nullptr;
  raw_ptr<ui::ime::InputMethodMenuManager> menu_manager_;
};

TEST_F(InputMethodManagerImplTest, TestGetImeKeyboard) {
  EXPECT_TRUE(manager_->GetImeKeyboard());
  EXPECT_EQ(keyboard_, manager_->GetImeKeyboard());
}

TEST_F(InputMethodManagerImplTest, TestCandidateWindowObserver) {
  TestCandidateWindowObserver observer;
  candidate_window_controller_->NotifyCandidateWindowOpened();  // nop
  candidate_window_controller_->NotifyCandidateWindowClosed();  // nop
  manager_->AddCandidateWindowObserver(&observer);
  candidate_window_controller_->NotifyCandidateWindowOpened();
  EXPECT_EQ(1, observer.candidate_window_opened_count_);
  candidate_window_controller_->NotifyCandidateWindowClosed();
  EXPECT_EQ(1, observer.candidate_window_closed_count_);
  candidate_window_controller_->NotifyCandidateWindowOpened();
  EXPECT_EQ(2, observer.candidate_window_opened_count_);
  candidate_window_controller_->NotifyCandidateWindowClosed();
  EXPECT_EQ(2, observer.candidate_window_closed_count_);
  manager_->RemoveCandidateWindowObserver(&observer);
}

TEST_F(InputMethodManagerImplTest, TestObserver) {
  // For http://crbug.com/19655#c11 - (3).
  std::vector<std::string> keyboard_layouts;
  keyboard_layouts.emplace_back("xkb:us::eng");

  TestObserver observer;
  manager_->AddObserver(&observer);
  menu_manager_->AddObserver(&observer);
  EXPECT_EQ(0, observer.input_method_changed_count_);
  EXPECT_EQ(0, observer.input_method_extension_added_count_);
  EXPECT_EQ(0, observer.input_method_extension_removed_count_);
  manager_->GetActiveIMEState()->EnableLoginLayouts("en-US", keyboard_layouts);
  EXPECT_EQ(8U, manager_->GetActiveIMEState()->GetEnabledInputMethods().size());
  EXPECT_EQ(1, observer.input_method_changed_count_);
  // Menu change is triggered only if current input method was actually changed.
  EXPECT_EQ(0, observer.input_method_menu_item_changed_count_);
  manager_->GetActiveIMEState()->ChangeInputMethod(
      ImeIdFromEngineId("xkb:us:dvorak:eng"), false /* show_message */);
  EXPECT_FALSE(observer.last_show_message_);
  EXPECT_EQ(2, observer.input_method_changed_count_);
  EXPECT_EQ(1, observer.input_method_menu_item_changed_count_);
  manager_->GetActiveIMEState()->ChangeInputMethod(
      ImeIdFromEngineId("xkb:us:dvorak:eng"), false /* show_message */);
  EXPECT_FALSE(observer.last_show_message_);

  // The observer is always notified even when the same input method ID is
  // passed to ChangeInputMethod() more than twice.
  // TODO(komatsu): Revisit if this is necessary.
  EXPECT_EQ(3, observer.input_method_changed_count_);

  // If the same input method ID is passed, PropertyChanged() is not
  // notified.
  EXPECT_EQ(1, observer.input_method_menu_item_changed_count_);

  // Add an ARC IME, remove it, then check the observer counts.
  MockInputMethodEngine engine;
  const std::string ime_id =
      extension_ime_util::GetArcInputMethodID(kExtensionId1, "engine_id");
  InputMethodDescriptor descriptor(
      ime_id, "arc ime", "AI", {"us"}, {"en-US"}, false /* is_login_keyboard */,
      GURL(), GURL(), /*handwriting_language=*/std::nullopt);
  manager_->GetActiveIMEState()->AddInputMethodExtension(kExtensionId1,
                                                         {descriptor}, &engine);
  EXPECT_EQ(1, observer.input_method_extension_added_count_);
  EXPECT_EQ(0, observer.input_method_extension_removed_count_);
  manager_->GetActiveIMEState()->RemoveInputMethodExtension(kExtensionId1);
  EXPECT_EQ(1, observer.input_method_extension_added_count_);
  EXPECT_EQ(1, observer.input_method_extension_removed_count_);

  manager_->RemoveObserver(&observer);
  menu_manager_->RemoveObserver(&observer);
}

TEST_F(InputMethodManagerImplTest, TestGetSupportedInputMethods) {
  InputMethodDescriptors methods;
  methods = manager_->GetComponentExtensionIMEManager()
                ->GetXkbIMEAsInputMethodDescriptor();
  // Try to find random 4-5 layuts and IMEs to make sure the returned list is
  // correct.
  const InputMethodDescriptor* id_to_find =
      manager_->GetInputMethodUtil()->GetInputMethodDescriptorFromId(
          ImeIdFromEngineId(kNaclMozcUsId));
  id_to_find = manager_->GetInputMethodUtil()->GetInputMethodDescriptorFromId(
      ImeIdFromEngineId("xkb:us::eng"));
  EXPECT_TRUE(Contain(methods, *id_to_find));
  id_to_find = manager_->GetInputMethodUtil()->GetInputMethodDescriptorFromId(
      ImeIdFromEngineId("xkb:us:dvorak:eng"));
  EXPECT_TRUE(Contain(methods, *id_to_find));
  id_to_find = manager_->GetInputMethodUtil()->GetInputMethodDescriptorFromId(
      ImeIdFromEngineId("xkb:fr::fra"));
  EXPECT_TRUE(Contain(methods, *id_to_find));
}

TEST_F(InputMethodManagerImplTest, TestEnableLayouts) {
  // Currently 8 keyboard layouts are supported for en-US, and 1 for ja. See
  // ibus_input_method.txt.
  std::vector<std::string> keyboard_layouts;

  manager_->GetActiveIMEState()->EnableLoginLayouts("en-US", keyboard_layouts);
  EXPECT_EQ(8U, manager_->GetActiveIMEState()->GetNumEnabledInputMethods());

  // For http://crbug.com/19655#c11 - (5)
  // The hardware keyboard layout "xkb:us::eng" is always active, hence 2U.
  manager_->GetActiveIMEState()->EnableLoginLayouts(
      "ja", keyboard_layouts);  // Japanese
  EXPECT_EQ(2U, manager_->GetActiveIMEState()->GetNumEnabledInputMethods());
}

TEST_F(InputMethodManagerImplTest, TestEnableLayoutsAndCurrentInputMethod) {
  // For http://crbug.com/329061
  std::vector<std::string> keyboard_layouts;
  keyboard_layouts.push_back(ImeIdFromEngineId("xkb:se::swe"));

  manager_->GetActiveIMEState()->EnableLoginLayouts("en-US", keyboard_layouts);
  const std::string im_id =
      manager_->GetActiveIMEState()->GetCurrentInputMethod().id();
  EXPECT_EQ(ImeIdFromEngineId("xkb:se::swe"), im_id);
}

TEST_F(InputMethodManagerImplTest, TestEnableLayoutsNonUsHardwareKeyboard) {
  // The physical layout is French.
  manager_->GetInputMethodUtil()->SetHardwareKeyboardLayoutForTesting(
      "xkb:fr::fra");
  manager_->GetActiveIMEState()->EnableLoginLayouts(
      "en-US",
      manager_->GetInputMethodUtil()->GetHardwareLoginInputMethodIds());
  EXPECT_EQ(9U,
            manager_->GetActiveIMEState()
                ->GetNumEnabledInputMethods());  // 8 + French
  // The physical layout is Japanese.
  manager_->GetInputMethodUtil()->SetHardwareKeyboardLayoutForTesting(
      "xkb:jp::jpn");
  manager_->GetActiveIMEState()->EnableLoginLayouts(
      "ja", manager_->GetInputMethodUtil()->GetHardwareLoginInputMethodIds());
  // "xkb:us::eng" is not needed, hence 1.
  EXPECT_EQ(1U, manager_->GetActiveIMEState()->GetNumEnabledInputMethods());

  // The physical layout is Russian.
  manager_->GetInputMethodUtil()->SetHardwareKeyboardLayoutForTesting(
      "xkb:ru::rus");
  manager_->GetActiveIMEState()->EnableLoginLayouts(
      "ru", manager_->GetInputMethodUtil()->GetHardwareLoginInputMethodIds());
  // "xkb:us::eng" only.
  EXPECT_EQ(1U, manager_->GetActiveIMEState()->GetNumEnabledInputMethods());
  EXPECT_EQ(ImeIdFromEngineId("xkb:us::eng"),
            manager_->GetActiveIMEState()->GetEnabledInputMethodIds().front());
}

TEST_F(InputMethodManagerImplTest, TestEnableMultipleHardwareKeyboardLayout) {
  // The physical layouts are French and Hungarian.
  manager_->GetInputMethodUtil()->SetHardwareKeyboardLayoutForTesting(
      "xkb:fr::fra,xkb:hu::hun");
  manager_->GetActiveIMEState()->EnableLoginLayouts(
      "en-US",
      manager_->GetInputMethodUtil()->GetHardwareLoginInputMethodIds());
  // 8 + French + Hungarian
  EXPECT_EQ(10U, manager_->GetActiveIMEState()->GetNumEnabledInputMethods());
}

TEST_F(InputMethodManagerImplTest,
       TestEnableMultipleHardwareKeyboardLayout_NoLoginKeyboard) {
  // The physical layouts are English (US) and Russian.
  manager_->GetInputMethodUtil()->SetHardwareKeyboardLayoutForTesting(
      "xkb:us::eng,xkb:ru::rus");
  manager_->GetActiveIMEState()->EnableLoginLayouts(
      "ru", manager_->GetInputMethodUtil()->GetHardwareLoginInputMethodIds());
  // xkb:us:eng
  EXPECT_EQ(1U, manager_->GetActiveIMEState()->GetNumEnabledInputMethods());
}

TEST_F(InputMethodManagerImplTest, TestEnabledInputMethods) {
  std::vector<std::string> keyboard_layouts;
  manager_->GetActiveIMEState()->EnableLoginLayouts(
      "ja", keyboard_layouts);  // Japanese
  EXPECT_EQ(2U, manager_->GetActiveIMEState()->GetNumEnabledInputMethods());
  InputMethodDescriptors methods =
      manager_->GetActiveIMEState()->GetEnabledInputMethods();
  EXPECT_EQ(2U, methods.size());
  const InputMethodDescriptor* id_to_find =
      manager_->GetInputMethodUtil()->GetInputMethodDescriptorFromId(
          ImeIdFromEngineId("xkb:us::eng"));
  EXPECT_TRUE(id_to_find && Contain(methods, *id_to_find));
  id_to_find = manager_->GetInputMethodUtil()->GetInputMethodDescriptorFromId(
      ImeIdFromEngineId("xkb:jp::jpn"));
  EXPECT_TRUE(id_to_find && Contain(methods, *id_to_find));
}

TEST_F(InputMethodManagerImplTest, TestEnableTwoLayouts) {
  // For http://crbug.com/19655#c11 - (8), step 6.
  TestObserver observer;
  manager_->AddObserver(&observer);
  std::vector<std::string> ids;
  ids.push_back(ImeIdFromEngineId("xkb:us:dvorak:eng"));
  ids.push_back(ImeIdFromEngineId("xkb:us:colemak:eng"));
  EXPECT_TRUE(manager_->GetActiveIMEState()->ReplaceEnabledInputMethods(ids));
  EXPECT_EQ(2U, manager_->GetActiveIMEState()->GetNumEnabledInputMethods());
  // Since all the IDs added avobe are keyboard layouts, Start() should not be
  // called.
  EXPECT_EQ(1, observer.input_method_changed_count_);
  EXPECT_EQ(ImeIdFromEngineId(ids[0]),
            manager_->GetActiveIMEState()->GetCurrentInputMethod().id());
  EXPECT_EQ("us(dvorak)", keyboard_->GetCurrentKeyboardLayoutName());
  // Disable Dvorak.
  ids.erase(ids.begin());
  EXPECT_TRUE(manager_->GetActiveIMEState()->ReplaceEnabledInputMethods(ids));
  EXPECT_EQ(1U, manager_->GetActiveIMEState()->GetNumEnabledInputMethods());
  EXPECT_EQ(2, observer.input_method_changed_count_);
  EXPECT_EQ(ImeIdFromEngineId(ids[0]),  // colemak
            manager_->GetActiveIMEState()->GetCurrentInputMethod().id());
  EXPECT_EQ("us(colemak)", keyboard_->GetCurrentKeyboardLayoutName());
  manager_->RemoveObserver(&observer);
}

TEST_F(InputMethodManagerImplTest, TestEnableThreeLayouts) {
  // For http://crbug.com/19655#c11 - (9).
  TestObserver observer;
  manager_->AddObserver(&observer);
  std::string us_id = ImeIdFromEngineId("xkb:us::eng");
  std::string us_dvorak_id = ImeIdFromEngineId("xkb:us:dvorak:eng");
  std::string us_colemak_id = ImeIdFromEngineId("xkb:us:colemak:eng");
  std::vector<std::string> ids{us_id, us_dvorak_id, us_colemak_id};
  EXPECT_TRUE(manager_->GetActiveIMEState()->ReplaceEnabledInputMethods(ids));
  EXPECT_EQ(3U, manager_->GetActiveIMEState()->GetNumEnabledInputMethods());
  EXPECT_EQ(1, observer.input_method_changed_count_);
  EXPECT_EQ(us_id, manager_->GetActiveIMEState()->GetCurrentInputMethod().id());
  EXPECT_EQ("us", keyboard_->GetCurrentKeyboardLayoutName());
  // Switch to Dvorak.
  manager_->GetActiveIMEState()->ChangeInputMethod(us_dvorak_id,
                                                   /*show_message=*/false);
  EXPECT_EQ(2, observer.input_method_changed_count_);
  EXPECT_EQ(us_dvorak_id,
            manager_->GetActiveIMEState()->GetCurrentInputMethod().id());
  EXPECT_EQ("us(dvorak)", keyboard_->GetCurrentKeyboardLayoutName());
  // Disable Dvorak.
  ids.erase(ids.begin() + 1);
  EXPECT_TRUE(manager_->GetActiveIMEState()->ReplaceEnabledInputMethods(ids));
  EXPECT_EQ(2U, manager_->GetActiveIMEState()->GetNumEnabledInputMethods());
  EXPECT_EQ(3, observer.input_method_changed_count_);
  EXPECT_EQ(us_id, manager_->GetActiveIMEState()->GetCurrentInputMethod().id());
  EXPECT_EQ("us", keyboard_->GetCurrentKeyboardLayoutName());
  manager_->RemoveObserver(&observer);
}

TEST_F(InputMethodManagerImplTest, TestEnableLayoutAndIme) {
  // For http://crbug.com/19655#c11 - (10).
  TestObserver observer;
  manager_->AddObserver(&observer);
  std::string dvorak_id = ImeIdFromEngineId("xkb:us:dvorak:eng");
  std::string mozc_id = ImeIdFromEngineId(kNaclMozcUsId);
  std::vector<std::string> ids{dvorak_id, mozc_id};
  EXPECT_TRUE(manager_->GetActiveIMEState()->ReplaceEnabledInputMethods(ids));
  EXPECT_EQ(1, observer.input_method_changed_count_);
  EXPECT_EQ(dvorak_id,
            manager_->GetActiveIMEState()->GetCurrentInputMethod().id());
  EXPECT_EQ("us(dvorak)", keyboard_->GetCurrentKeyboardLayoutName());
  // Switch to Mozc
  manager_->GetActiveIMEState()->ChangeInputMethod(mozc_id,
                                                   /*show_message=*/false);
  EXPECT_EQ(2, observer.input_method_changed_count_);
  EXPECT_EQ(mozc_id,
            manager_->GetActiveIMEState()->GetCurrentInputMethod().id());
  EXPECT_EQ("us", keyboard_->GetCurrentKeyboardLayoutName());
  // Disable Mozc.
  ids.erase(ids.begin() + 1);
  EXPECT_TRUE(manager_->GetActiveIMEState()->ReplaceEnabledInputMethods(ids));
  EXPECT_EQ(1U, manager_->GetActiveIMEState()->GetNumEnabledInputMethods());
  EXPECT_EQ(dvorak_id,
            manager_->GetActiveIMEState()->GetCurrentInputMethod().id());
  EXPECT_EQ("us(dvorak)", keyboard_->GetCurrentKeyboardLayoutName());
}

TEST_F(InputMethodManagerImplTest, TestEnableLayoutAndIme2) {
  // For http://crbug.com/19655#c11 - (11).
  TestObserver observer;
  manager_->AddObserver(&observer);
  std::vector<std::string> ids;
  ids.push_back(ImeIdFromEngineId("xkb:us:dvorak:eng"));
  ids.push_back(ImeIdFromEngineId(kNaclMozcUsId));
  EXPECT_TRUE(manager_->GetActiveIMEState()->ReplaceEnabledInputMethods(ids));
  EXPECT_EQ(1, observer.input_method_changed_count_);
  EXPECT_EQ(ImeIdFromEngineId(ids[0]),
            manager_->GetActiveIMEState()->GetCurrentInputMethod().id());
  EXPECT_EQ("us(dvorak)", keyboard_->GetCurrentKeyboardLayoutName());

  // Disable Dvorak.
  ids.erase(ids.begin());
  EXPECT_TRUE(manager_->GetActiveIMEState()->ReplaceEnabledInputMethods(ids));
  EXPECT_EQ(1U, manager_->GetActiveIMEState()->GetNumEnabledInputMethods());
  EXPECT_EQ(ImeIdFromEngineId(ids[0]),  // Mozc
            manager_->GetActiveIMEState()->GetCurrentInputMethod().id());
  EXPECT_EQ("us", keyboard_->GetCurrentKeyboardLayoutName());
  manager_->RemoveObserver(&observer);
}

TEST_F(InputMethodManagerImplTest, TestEnableImes) {
  TestObserver observer;
  manager_->AddObserver(&observer);
  std::vector<std::string> ids;
  ids.push_back(ImeIdFromEngineId(kExt2Engine1Id));
  ids.emplace_back("mozc-dv");
  EXPECT_TRUE(manager_->GetActiveIMEState()->ReplaceEnabledInputMethods(ids));
  EXPECT_EQ(1, observer.input_method_changed_count_);
  EXPECT_EQ(ImeIdFromEngineId(ids[0]),
            manager_->GetActiveIMEState()->GetCurrentInputMethod().id());
  EXPECT_EQ("us", keyboard_->GetCurrentKeyboardLayoutName());
  manager_->RemoveObserver(&observer);
}

TEST_F(InputMethodManagerImplTest, TestEnableUnknownIds) {
  TestObserver observer;
  manager_->AddObserver(&observer);
  std::vector<std::string> ids;
  ids.emplace_back("xkb:tl::tlh");  // Klingon, which is not supported.
  ids.emplace_back("unknown-super-cool-ime");
  EXPECT_FALSE(manager_->GetActiveIMEState()->ReplaceEnabledInputMethods(ids));

  // TODO(yusukes): Should we fall back to the hardware keyboard layout in this
  // case?
  EXPECT_EQ(0, observer.input_method_changed_count_);

  manager_->RemoveObserver(&observer);
}

TEST_F(InputMethodManagerImplTest, TestEnableLayoutsThenLock) {
  // For http://crbug.com/19655#c11 - (14).
  TestObserver observer;
  manager_->AddObserver(&observer);
  std::string us_id = ImeIdFromEngineId("xkb:us::eng");
  std::string us_dvorak_id = ImeIdFromEngineId("xkb:us:dvorak:eng");
  std::vector<std::string> ids{us_id, us_dvorak_id};
  EXPECT_TRUE(manager_->GetActiveIMEState()->ReplaceEnabledInputMethods(ids));
  EXPECT_EQ(2U, manager_->GetActiveIMEState()->GetNumEnabledInputMethods());
  EXPECT_EQ(1, observer.input_method_changed_count_);
  EXPECT_EQ(us_id, manager_->GetActiveIMEState()->GetCurrentInputMethod().id());
  EXPECT_EQ("us", keyboard_->GetCurrentKeyboardLayoutName());

  // Switch to Dvorak.
  manager_->GetActiveIMEState()->ChangeInputMethod(us_dvorak_id,
                                                   /*show_message=*/false);
  EXPECT_EQ(2, observer.input_method_changed_count_);
  EXPECT_EQ(us_dvorak_id,
            manager_->GetActiveIMEState()->GetCurrentInputMethod().id());
  EXPECT_EQ("us(dvorak)", keyboard_->GetCurrentKeyboardLayoutName());

  // Lock screen
  scoped_refptr<InputMethodManager::State> saved_ime_state =
      manager_->GetActiveIMEState();
  manager_->SetState(saved_ime_state->Clone());
  manager_->GetActiveIMEState()->DisableNonLockScreenLayouts();
  EXPECT_EQ(2U, manager_->GetActiveIMEState()->GetNumEnabledInputMethods());
  EXPECT_EQ(us_dvorak_id,
            manager_->GetActiveIMEState()->GetCurrentInputMethod().id());
  EXPECT_EQ("us(dvorak)", keyboard_->GetCurrentKeyboardLayoutName());
  // Switch back to Qwerty.
  manager_->GetActiveIMEState()->ChangeInputMethod(us_id,
                                                   /*show_message=*/false);
  EXPECT_EQ(us_id, manager_->GetActiveIMEState()->GetCurrentInputMethod().id());
  EXPECT_EQ("us", keyboard_->GetCurrentKeyboardLayoutName());

  // Unlock screen. The original state, Dvorak, is restored.
  manager_->SetState(saved_ime_state);
  EXPECT_EQ(manager_->GetActiveIMEState()->GetUIStyle(),
            InputMethodManager::UIStyle::kNormal);
  EXPECT_EQ(2U, manager_->GetActiveIMEState()->GetNumEnabledInputMethods());
  EXPECT_EQ(us_dvorak_id,
            manager_->GetActiveIMEState()->GetCurrentInputMethod().id());
  EXPECT_EQ("us(dvorak)", keyboard_->GetCurrentKeyboardLayoutName());

  manager_->RemoveObserver(&observer);
}

TEST_F(InputMethodManagerImplTest, SwitchInputMethodTest) {
  // For http://crbug.com/19655#c11 - (15).
  TestObserver observer;
  manager_->AddObserver(&observer);
  std::string id1 = ImeIdFromEngineId("xkb:us:dvorak:eng");
  std::string id2 = ImeIdFromEngineId(kExt2Engine2Id);
  std::string id3 = ImeIdFromEngineId(kExt2Engine1Id);
  std::vector<std::string> ids{id1, id2, id3};
  EXPECT_TRUE(manager_->GetActiveIMEState()->ReplaceEnabledInputMethods(ids));
  EXPECT_EQ(3U, manager_->GetActiveIMEState()->GetNumEnabledInputMethods());
  EXPECT_EQ(1, observer.input_method_changed_count_);
  EXPECT_EQ(id1, manager_->GetActiveIMEState()->GetCurrentInputMethod().id());
  EXPECT_EQ("us(dvorak)", keyboard_->GetCurrentKeyboardLayoutName());

  // Switch to id2.
  manager_->GetActiveIMEState()->ChangeInputMethod(id2, /*show_message=*/false);
  EXPECT_EQ(2, observer.input_method_changed_count_);
  EXPECT_EQ(id2, manager_->GetActiveIMEState()->GetCurrentInputMethod().id());
  EXPECT_EQ("us(dvorak)", keyboard_->GetCurrentKeyboardLayoutName());

  // Lock screen
  scoped_refptr<InputMethodManager::State> saved_ime_state =
      manager_->GetActiveIMEState();
  manager_->SetState(saved_ime_state->Clone());
  manager_->GetActiveIMEState()->DisableNonLockScreenLayouts();
  EXPECT_EQ(2U,
            manager_->GetActiveIMEState()
                ->GetNumEnabledInputMethods());  // hardware layout + id1
  EXPECT_EQ(id1, manager_->GetActiveIMEState()->GetCurrentInputMethod().id());
  EXPECT_EQ("us(dvorak)", keyboard_->GetCurrentKeyboardLayoutName());

  std::string hardware_layout_ime_id = ImeIdFromEngineId("xkb:us::eng");
  manager_->GetActiveIMEState()->ChangeInputMethod(hardware_layout_ime_id,
                                                   /*show_message=*/false);
  EXPECT_EQ(hardware_layout_ime_id,
            manager_->GetActiveIMEState()->GetCurrentInputMethod().id());
  EXPECT_EQ("us", keyboard_->GetCurrentKeyboardLayoutName());

  // Unlock screen. The original state is restored.
  manager_->SetState(saved_ime_state);
  EXPECT_EQ(manager_->GetActiveIMEState()->GetUIStyle(),
            InputMethodManager::UIStyle::kNormal);
  EXPECT_EQ(3U, manager_->GetActiveIMEState()->GetNumEnabledInputMethods());
  EXPECT_EQ(id2, manager_->GetActiveIMEState()->GetCurrentInputMethod().id());
  EXPECT_EQ("us(dvorak)", keyboard_->GetCurrentKeyboardLayoutName());

  manager_->RemoveObserver(&observer);
}

TEST_F(InputMethodManagerImplTest, TestXkbSetting) {
  // For http://crbug.com/19655#c11 - (8), step 7-11.
  EXPECT_EQ(1, keyboard_->set_current_keyboard_layout_by_name_count_);
  std::string dvorak_id = ImeIdFromEngineId("xkb:us:dvorak:eng");
  std::string colemak_id = ImeIdFromEngineId("xkb:us:colemak:eng");
  std::string mozc_jp_id = ImeIdFromEngineId(kNaclMozcJpId);
  std::string mozc_us_id = ImeIdFromEngineId(kNaclMozcUsId);
  std::vector<std::string> ids{dvorak_id, colemak_id, mozc_jp_id, mozc_us_id};
  EXPECT_TRUE(manager_->GetActiveIMEState()->ReplaceEnabledInputMethods(ids));
  EXPECT_EQ(4U, manager_->GetActiveIMEState()->GetNumEnabledInputMethods());
  EXPECT_EQ(2, keyboard_->set_current_keyboard_layout_by_name_count_);
  EXPECT_EQ("us(dvorak)", keyboard_->GetCurrentKeyboardLayoutName());
  manager_->GetActiveIMEState()->ChangeInputMethod(colemak_id,
                                                   /*show_message=*/false);
  EXPECT_EQ(3, keyboard_->set_current_keyboard_layout_by_name_count_);
  EXPECT_EQ("us(colemak)", keyboard_->GetCurrentKeyboardLayoutName());
  manager_->GetActiveIMEState()->ChangeInputMethod(mozc_jp_id,
                                                   /*show_message=*/false);
  EXPECT_EQ(4, keyboard_->set_current_keyboard_layout_by_name_count_);
  EXPECT_EQ("jp", keyboard_->GetCurrentKeyboardLayoutName());
  manager_->GetActiveIMEState()->ChangeInputMethod(mozc_us_id,
                                                   /*show_message=*/false);
  EXPECT_EQ(5, keyboard_->set_current_keyboard_layout_by_name_count_);
  EXPECT_EQ("us", keyboard_->GetCurrentKeyboardLayoutName());
  manager_->GetActiveIMEState()->ChangeInputMethod(dvorak_id,
                                                   /*show_message=*/false);
  EXPECT_EQ(6, keyboard_->set_current_keyboard_layout_by_name_count_);
  EXPECT_EQ("us(dvorak)", keyboard_->GetCurrentKeyboardLayoutName());
  // Disable Dvorak.
  ids.erase(ids.begin());
  EXPECT_TRUE(manager_->GetActiveIMEState()->ReplaceEnabledInputMethods(ids));
  EXPECT_EQ(3U, manager_->GetActiveIMEState()->GetNumEnabledInputMethods());
  EXPECT_EQ(7, keyboard_->set_current_keyboard_layout_by_name_count_);
  EXPECT_EQ("us(colemak)", keyboard_->GetCurrentKeyboardLayoutName());
}

TEST_F(InputMethodManagerImplTest, TestActivateInputMethodMenuItem) {
  const std::string kKey = "key";
  ui::ime::InputMethodMenuItemList menu_list;
  menu_list.push_back(ui::ime::InputMethodMenuItem(kKey, "label", false));
  menu_manager_->SetCurrentInputMethodMenuItemList(menu_list);

  manager_->ActivateInputMethodMenuItem(kKey);
  EXPECT_EQ(kKey, mock_engine_handler_->last_activated_property());

  // Key2 is not registered, so activated property should not be changed.
  manager_->ActivateInputMethodMenuItem("key2");
  EXPECT_EQ(kKey, mock_engine_handler_->last_activated_property());
}

TEST_F(InputMethodManagerImplTest, TestGetCurrentInputMethodProperties) {
  EXPECT_TRUE(menu_manager_->GetCurrentInputMethodMenuItemList().empty());

  std::vector<std::string> ids;
  ids.push_back(ImeIdFromEngineId("xkb:us::eng"));
  ids.push_back(ImeIdFromEngineId(kNaclMozcUsId));
  EXPECT_TRUE(manager_->GetActiveIMEState()->ReplaceEnabledInputMethods(ids));
  EXPECT_EQ(2U, manager_->GetActiveIMEState()->GetNumEnabledInputMethods());
  EXPECT_TRUE(menu_manager_->GetCurrentInputMethodMenuItemList().empty());
  manager_->GetActiveIMEState()->ChangeInputMethod(
      ImeIdFromEngineId(kNaclMozcUsId), false /* show_message */);

  ui::ime::InputMethodMenuItemList current_property_list;
  current_property_list.push_back(
      ui::ime::InputMethodMenuItem("key", "label", false));
  menu_manager_->SetCurrentInputMethodMenuItemList(current_property_list);

  ASSERT_EQ(1U, menu_manager_->GetCurrentInputMethodMenuItemList().size());
  EXPECT_EQ("key",
            menu_manager_->GetCurrentInputMethodMenuItemList().at(0).key);

  manager_->GetActiveIMEState()->ChangeInputMethod("xkb:us::eng",
                                                   false /* show_message */);
  EXPECT_TRUE(menu_manager_->GetCurrentInputMethodMenuItemList().empty());
}

TEST_F(InputMethodManagerImplTest, TestGetCurrentInputMethodPropertiesTwoImes) {
  EXPECT_TRUE(menu_manager_->GetCurrentInputMethodMenuItemList().empty());

  std::vector<std::string> ids;
  ids.push_back(ImeIdFromEngineId(kNaclMozcUsId));   // Japanese
  ids.push_back(ImeIdFromEngineId(kExt2Engine1Id));  // T-Chinese
  EXPECT_TRUE(manager_->GetActiveIMEState()->ReplaceEnabledInputMethods(ids));
  EXPECT_EQ(2U, manager_->GetActiveIMEState()->GetNumEnabledInputMethods());
  EXPECT_TRUE(menu_manager_->GetCurrentInputMethodMenuItemList().empty());

  ui::ime::InputMethodMenuItemList current_property_list;
  current_property_list.push_back(
      ui::ime::InputMethodMenuItem("key-mozc", "label", false));
  menu_manager_->SetCurrentInputMethodMenuItemList(current_property_list);

  ASSERT_EQ(1U, menu_manager_->GetCurrentInputMethodMenuItemList().size());
  EXPECT_EQ("key-mozc",
            menu_manager_->GetCurrentInputMethodMenuItemList().at(0).key);

  manager_->GetActiveIMEState()->ChangeInputMethod(
      ImeIdFromEngineId(kExt2Engine1Id), false /* show_message */);
  // Since the IME is changed, the property for mozc Japanese should be hidden.
  EXPECT_TRUE(menu_manager_->GetCurrentInputMethodMenuItemList().empty());

  // Asynchronous property update signal from mozc-chewing.
  current_property_list.clear();
  current_property_list.push_back(
      ui::ime::InputMethodMenuItem("key-chewing", "label", false));
  menu_manager_->SetCurrentInputMethodMenuItemList(current_property_list);
  ASSERT_EQ(1U, menu_manager_->GetCurrentInputMethodMenuItemList().size());
  EXPECT_EQ("key-chewing",
            menu_manager_->GetCurrentInputMethodMenuItemList().at(0).key);
}

TEST_F(InputMethodManagerImplTest,
       TestGetEnabledInputMethodsSortedByDisplayNames) {
  scoped_refptr<InputMethodManager::State> active_state =
      manager_->GetActiveIMEState();
  active_state->EnableInputMethod(ImeIdFromEngineId("xkb:us::eng"));
  active_state->EnableInputMethod(ImeIdFromEngineId("xkb:fr::fra"));
  active_state->EnableInputMethod(ImeIdFromEngineId("xkb:se::swe"));
  active_state->EnableInputMethod(ImeIdFromEngineId("xkb:jp::jpn"));
  active_state->EnableInputMethod(ImeIdFromEngineId("xkb:ru::rus"));
  active_state->EnableInputMethod(ImeIdFromEngineId("xkb:hu::hun"));
  active_state->EnableInputMethod(ImeIdFromEngineId("xkb:de::ger"));

  base::i18n::SetICUDefaultLocale("en-US");
  InputMethodDescriptors result =
      active_state->GetEnabledInputMethodsSortedByLocalizedDisplayNames();
  ASSERT_FALSE(result.empty());

  InputMethodUtil* util = manager_->GetInputMethodUtil();
  UErrorCode error_code = U_ZERO_ERROR;
  std::unique_ptr<icu::Collator> collator(
      icu::Collator::createInstance(error_code));

  for (size_t i = 1; i < result.size(); ++i) {
    std::string prev_name = util->GetLocalizedDisplayName(result.at(i - 1));
    std::string name = util->GetLocalizedDisplayName(result.at(i));
    ASSERT_EQ(UCOL_LESS, base::i18n::CompareString16WithCollator(
                             *collator, base::UTF8ToUTF16(prev_name),
                             base::UTF8ToUTF16(name)));
  }
}

TEST_F(InputMethodManagerImplTest, TestNextInputMethod) {
  TestObserver observer;
  manager_->AddObserver(&observer);
  std::vector<std::string> keyboard_layouts;
  keyboard_layouts.push_back(ImeIdFromEngineId("xkb:us::eng"));

  // For http://crbug.com/19655#c11 - (1)
  manager_->GetActiveIMEState()->EnableLoginLayouts("en-US", keyboard_layouts);
  EXPECT_EQ(8U, manager_->GetActiveIMEState()->GetNumEnabledInputMethods());
  InputMethodDescriptors sorted_enabled_input_methods =
      manager_->GetActiveIMEState()
          ->GetEnabledInputMethodsSortedByLocalizedDisplayNames();
  InputMethodDescriptor current_input_method =
      manager_->GetActiveIMEState()->GetCurrentInputMethod();
  EXPECT_EQ(sorted_enabled_input_methods.at(0).id(), current_input_method.id());
  EXPECT_EQ(current_input_method.keyboard_layout(),
            keyboard_->GetCurrentKeyboardLayoutName());

  manager_->GetActiveIMEState()->SwitchToNextInputMethod();
  EXPECT_TRUE(observer.last_show_message_);
  current_input_method = manager_->GetActiveIMEState()->GetCurrentInputMethod();
  EXPECT_EQ(sorted_enabled_input_methods.at(1).id(), current_input_method.id());
  EXPECT_EQ(current_input_method.keyboard_layout(),
            keyboard_->GetCurrentKeyboardLayoutName());

  manager_->GetActiveIMEState()->SwitchToNextInputMethod();
  EXPECT_TRUE(observer.last_show_message_);
  current_input_method = manager_->GetActiveIMEState()->GetCurrentInputMethod();
  EXPECT_EQ(sorted_enabled_input_methods.at(2).id(), current_input_method.id());
  EXPECT_EQ(current_input_method.keyboard_layout(),
            keyboard_->GetCurrentKeyboardLayoutName());

  manager_->GetActiveIMEState()->SwitchToNextInputMethod();
  EXPECT_TRUE(observer.last_show_message_);
  current_input_method = manager_->GetActiveIMEState()->GetCurrentInputMethod();
  EXPECT_EQ(sorted_enabled_input_methods.at(3).id(), current_input_method.id());
  EXPECT_EQ(current_input_method.keyboard_layout(),
            keyboard_->GetCurrentKeyboardLayoutName());

  manager_->GetActiveIMEState()->SwitchToNextInputMethod();
  EXPECT_TRUE(observer.last_show_message_);
  current_input_method = manager_->GetActiveIMEState()->GetCurrentInputMethod();
  EXPECT_EQ(sorted_enabled_input_methods.at(4).id(), current_input_method.id());
  EXPECT_EQ(current_input_method.keyboard_layout(),
            keyboard_->GetCurrentKeyboardLayoutName());

  manager_->GetActiveIMEState()->SwitchToNextInputMethod();
  EXPECT_TRUE(observer.last_show_message_);
  current_input_method = manager_->GetActiveIMEState()->GetCurrentInputMethod();
  EXPECT_EQ(sorted_enabled_input_methods.at(5).id(), current_input_method.id());
  EXPECT_EQ(current_input_method.keyboard_layout(),
            keyboard_->GetCurrentKeyboardLayoutName());

  manager_->GetActiveIMEState()->SwitchToNextInputMethod();
  EXPECT_TRUE(observer.last_show_message_);
  current_input_method = manager_->GetActiveIMEState()->GetCurrentInputMethod();
  EXPECT_EQ(sorted_enabled_input_methods.at(6).id(), current_input_method.id());
  EXPECT_EQ(current_input_method.keyboard_layout(),
            keyboard_->GetCurrentKeyboardLayoutName());

  manager_->GetActiveIMEState()->SwitchToNextInputMethod();
  EXPECT_TRUE(observer.last_show_message_);
  current_input_method = manager_->GetActiveIMEState()->GetCurrentInputMethod();
  EXPECT_EQ(sorted_enabled_input_methods.at(7).id(), current_input_method.id());
  EXPECT_EQ(current_input_method.keyboard_layout(),
            keyboard_->GetCurrentKeyboardLayoutName());

  manager_->GetActiveIMEState()->SwitchToNextInputMethod();
  EXPECT_TRUE(observer.last_show_message_);
  current_input_method = manager_->GetActiveIMEState()->GetCurrentInputMethod();
  EXPECT_EQ(sorted_enabled_input_methods.at(0).id(), current_input_method.id());
  EXPECT_EQ(current_input_method.keyboard_layout(),
            keyboard_->GetCurrentKeyboardLayoutName());

  manager_->RemoveObserver(&observer);
}

TEST_F(InputMethodManagerImplTest, TestLastUsedInputMethod) {
  TestObserver observer;
  manager_->AddObserver(&observer);

  std::string us_id = ImeIdFromEngineId("xkb:us::eng");
  std::string us_intl_id = ImeIdFromEngineId("xkb:us:intl:eng");
  std::string us_altgr_intl_id = ImeIdFromEngineId("xkb:us:altgr-intl:eng");
  std::vector<std::string> keyboard_layouts;
  keyboard_layouts.push_back(us_id);

  manager_->GetActiveIMEState()->EnableLoginLayouts("en-US", keyboard_layouts);
  EXPECT_EQ(8U, manager_->GetActiveIMEState()->GetNumEnabledInputMethods());
  EXPECT_EQ(us_id, manager_->GetActiveIMEState()->GetCurrentInputMethod().id());
  EXPECT_EQ("us", keyboard_->GetCurrentKeyboardLayoutName());

  manager_->GetActiveIMEState()->ChangeInputMethod(us_intl_id,
                                                   /*show_message=*/true);
  EXPECT_TRUE(observer.last_show_message_);
  EXPECT_EQ(us_intl_id,
            manager_->GetActiveIMEState()->GetCurrentInputMethod().id());
  EXPECT_EQ("us(intl)", keyboard_->GetCurrentKeyboardLayoutName());

  manager_->GetActiveIMEState()->SwitchToLastUsedInputMethod();
  EXPECT_TRUE(observer.last_show_message_);
  EXPECT_EQ(us_id, manager_->GetActiveIMEState()->GetCurrentInputMethod().id());
  EXPECT_EQ("us", keyboard_->GetCurrentKeyboardLayoutName());

  manager_->GetActiveIMEState()->SwitchToLastUsedInputMethod();
  EXPECT_TRUE(observer.last_show_message_);
  EXPECT_EQ(us_intl_id,
            manager_->GetActiveIMEState()->GetCurrentInputMethod().id());
  EXPECT_EQ("us(intl)", keyboard_->GetCurrentKeyboardLayoutName());

  manager_->GetActiveIMEState()->SwitchToLastUsedInputMethod();
  EXPECT_TRUE(observer.last_show_message_);
  EXPECT_EQ(us_id, manager_->GetActiveIMEState()->GetCurrentInputMethod().id());
  EXPECT_EQ("us", keyboard_->GetCurrentKeyboardLayoutName());

  manager_->GetActiveIMEState()->ChangeInputMethod(us_intl_id,
                                                   /*show_message=*/true);
  EXPECT_TRUE(observer.last_show_message_);
  EXPECT_EQ(ImeIdFromEngineId("xkb:us:intl:eng"),
            manager_->GetActiveIMEState()->GetCurrentInputMethod().id());
  EXPECT_EQ("us(intl)", keyboard_->GetCurrentKeyboardLayoutName());

  manager_->GetActiveIMEState()->ChangeInputMethod(us_altgr_intl_id,
                                                   /*show_message=*/true);
  EXPECT_TRUE(observer.last_show_message_);
  EXPECT_EQ(us_altgr_intl_id,
            manager_->GetActiveIMEState()->GetCurrentInputMethod().id());
  EXPECT_EQ("us(altgr-intl)", keyboard_->GetCurrentKeyboardLayoutName());

  manager_->GetActiveIMEState()->SwitchToLastUsedInputMethod();
  EXPECT_TRUE(observer.last_show_message_);
  EXPECT_EQ(us_intl_id,
            manager_->GetActiveIMEState()->GetCurrentInputMethod().id());
  EXPECT_EQ("us(intl)", keyboard_->GetCurrentKeyboardLayoutName());

  manager_->GetActiveIMEState()->SwitchToLastUsedInputMethod();
  EXPECT_TRUE(observer.last_show_message_);
  EXPECT_EQ(us_altgr_intl_id,
            manager_->GetActiveIMEState()->GetCurrentInputMethod().id());
  EXPECT_EQ("us(altgr-intl)", keyboard_->GetCurrentKeyboardLayoutName());

  manager_->RemoveObserver(&observer);
}

TEST_F(InputMethodManagerImplTest, CycleInputMethodForOneEnabledInputMethod) {
  // Simulate a single input method.
  std::vector<std::string> ids;
  ids.push_back(ImeIdFromEngineId("xkb:us::eng"));
  EXPECT_TRUE(manager_->GetActiveIMEState()->ReplaceEnabledInputMethods(ids));
  EXPECT_EQ(1U, manager_->GetActiveIMEState()->GetNumEnabledInputMethods());

  // Switching to next does nothing.
  manager_->GetActiveIMEState()->SwitchToNextInputMethod();
  EXPECT_EQ(ImeIdFromEngineId("xkb:us::eng"),
            manager_->GetActiveIMEState()->GetCurrentInputMethod().id());

  // Switching to last-used does nothing.
  manager_->GetActiveIMEState()->SwitchToLastUsedInputMethod();
  EXPECT_EQ(ImeIdFromEngineId("xkb:us::eng"),
            manager_->GetActiveIMEState()->GetCurrentInputMethod().id());
}

TEST_F(InputMethodManagerImplTest, TestAddRemoveExtensionInputMethods) {
  TestObserver observer;
  manager_->AddObserver(&observer);
  std::vector<std::string> ids;
  ids.push_back(ImeIdFromEngineId("xkb:us:dvorak:eng"));
  EXPECT_TRUE(manager_->GetActiveIMEState()->ReplaceEnabledInputMethods(ids));
  EXPECT_EQ(1U, manager_->GetActiveIMEState()->GetNumEnabledInputMethods());
  EXPECT_EQ(1, observer.input_method_changed_count_);
  EXPECT_EQ(ImeIdFromEngineId(ids[0]),
            manager_->GetActiveIMEState()->GetCurrentInputMethod().id());
  EXPECT_EQ("us(dvorak)", keyboard_->GetCurrentKeyboardLayoutName());

  // Add two Extension IMEs.
  std::vector<std::string> languages;
  languages.emplace_back("en-US");

  const std::string ext1_id =
      extension_ime_util::GetInputMethodID(kExtensionId1, "engine_id");
  const InputMethodDescriptor descriptor1(
      ext1_id, "deadbeef input method", "DB",
      "us",  // layout
      languages,
      false,  // is_login_keyboard
      GURL(), GURL(), /*handwriting_language=*/std::nullopt);
  MockInputMethodEngine engine;
  InputMethodDescriptors descriptors;
  descriptors.push_back(descriptor1);
  manager_->GetActiveIMEState()->AddInputMethodExtension(kExtensionId1,
                                                         descriptors, &engine);

  // Extension IMEs are not enabled by default.
  EXPECT_EQ(1U, manager_->GetActiveIMEState()->GetNumEnabledInputMethods());

  std::vector<std::string> extension_ime_ids;
  extension_ime_ids.push_back(ext1_id);
  manager_->GetActiveIMEState()->SetEnabledExtensionImes(extension_ime_ids);
  EXPECT_EQ(2U, manager_->GetActiveIMEState()->GetNumEnabledInputMethods());

  {
    InputMethodDescriptors methods(
        manager_->GetActiveIMEState()->GetEnabledInputMethods());
    ASSERT_EQ(2U, methods.size());
    // Ext IMEs should be at the end of the list.
    EXPECT_EQ(ext1_id, methods.at(1).id());
  }

  const std::string ext2_id =
      extension_ime_util::GetInputMethodID(kExtensionId2, "engine_id");
  const InputMethodDescriptor descriptor2(
      ext2_id, "cafebabe input method", "CB",
      "us",  // layout
      languages,
      false,  // is_login_keyboard
      GURL(), GURL(), /*handwriting_language=*/std::nullopt);
  descriptors.clear();
  descriptors.push_back(descriptor2);
  MockInputMethodEngine engine2;
  manager_->GetActiveIMEState()->AddInputMethodExtension(kExtensionId2,
                                                         descriptors, &engine2);
  EXPECT_EQ(2U, manager_->GetActiveIMEState()->GetNumEnabledInputMethods());

  extension_ime_ids.push_back(ext2_id);
  manager_->GetActiveIMEState()->SetEnabledExtensionImes(extension_ime_ids);
  EXPECT_EQ(3U, manager_->GetActiveIMEState()->GetNumEnabledInputMethods());
  {
    InputMethodDescriptors methods(
        manager_->GetActiveIMEState()->GetEnabledInputMethods());
    ASSERT_EQ(3U, methods.size());
    // Ext IMEs should be at the end of the list.
    EXPECT_EQ(ext1_id, methods.at(1).id());
    EXPECT_EQ(ext2_id, methods.at(2).id());
  }

  // Remove them.
  manager_->GetActiveIMEState()->RemoveInputMethodExtension(kExtensionId1);
  EXPECT_EQ(2U, manager_->GetActiveIMEState()->GetNumEnabledInputMethods());
  manager_->GetActiveIMEState()->RemoveInputMethodExtension(kExtensionId2);
  EXPECT_EQ(1U, manager_->GetActiveIMEState()->GetNumEnabledInputMethods());
}

TEST_F(InputMethodManagerImplTest, TestAddExtensionInputThenLockScreen) {
  TestObserver observer;
  manager_->AddObserver(&observer);
  std::vector<std::string> ids;
  ids.push_back(ImeIdFromEngineId("xkb:us::eng"));
  EXPECT_TRUE(manager_->GetActiveIMEState()->ReplaceEnabledInputMethods(ids));
  EXPECT_EQ(1U, manager_->GetActiveIMEState()->GetNumEnabledInputMethods());
  EXPECT_EQ(1, observer.input_method_changed_count_);
  EXPECT_EQ(ImeIdFromEngineId(ids[0]),
            manager_->GetActiveIMEState()->GetCurrentInputMethod().id());
  EXPECT_EQ("us", keyboard_->GetCurrentKeyboardLayoutName());

  // Add an Extension IME
  std::vector<std::string> languages;
  languages.emplace_back("en-US");

  const std::string ext_id =
      extension_ime_util::GetInputMethodID(kExtensionId1, "engine_id");
  const InputMethodDescriptor descriptor(ext_id, "deadbeef input method", "DB",
                                         "us(dvorak)",  // layout
                                         languages,
                                         false,  // is_login_keyboard
                                         GURL(), GURL(),
                                         /*handwriting_language=*/std::nullopt);
  MockInputMethodEngine engine;
  InputMethodDescriptors descriptors;
  descriptors.push_back(descriptor);
  manager_->GetActiveIMEState()->AddInputMethodExtension(kExtensionId1,
                                                         descriptors, &engine);

  // Extension IME is not enabled by default.
  EXPECT_EQ(1U, manager_->GetActiveIMEState()->GetNumEnabledInputMethods());
  EXPECT_EQ(1, observer.input_method_changed_count_);

  std::vector<std::string> extension_ime_ids;
  extension_ime_ids.push_back(ext_id);
  manager_->GetActiveIMEState()->SetEnabledExtensionImes(extension_ime_ids);
  EXPECT_EQ(2U, manager_->GetActiveIMEState()->GetNumEnabledInputMethods());

  // Switch to the IME.
  manager_->GetActiveIMEState()->ChangeInputMethod(ext_id,
                                                   /*show_message=*/false);
  EXPECT_EQ(3, observer.input_method_changed_count_);
  EXPECT_EQ(ext_id,
            manager_->GetActiveIMEState()->GetCurrentInputMethod().id());
  EXPECT_EQ("us(dvorak)", keyboard_->GetCurrentKeyboardLayoutName());

  // Lock the screen. This is for crosbug.com/27049.
  scoped_refptr<InputMethodManager::State> saved_ime_state =
      manager_->GetActiveIMEState();
  manager_->SetState(saved_ime_state->Clone());
  manager_->GetActiveIMEState()->DisableNonLockScreenLayouts();
  EXPECT_EQ(1U,
            manager_->GetActiveIMEState()
                ->GetNumEnabledInputMethods());  // Qwerty. No Ext. IME
  EXPECT_EQ(ImeIdFromEngineId("xkb:us::eng"),
            manager_->GetActiveIMEState()->GetCurrentInputMethod().id());
  EXPECT_EQ("us", keyboard_->GetCurrentKeyboardLayoutName());

  // Unlock the screen.
  manager_->SetState(saved_ime_state);
  EXPECT_EQ(manager_->GetActiveIMEState()->GetUIStyle(),
            InputMethodManager::UIStyle::kNormal);
  EXPECT_EQ(2U, manager_->GetActiveIMEState()->GetNumEnabledInputMethods());
  EXPECT_EQ(ext_id,
            manager_->GetActiveIMEState()->GetCurrentInputMethod().id());
  EXPECT_EQ("us(dvorak)", keyboard_->GetCurrentKeyboardLayoutName());
  {
    // This is for crosbug.com/27052.
    InputMethodDescriptors methods(
        manager_->GetActiveIMEState()->GetEnabledInputMethods());
    ASSERT_EQ(2U, methods.size());
    // Ext. IMEs should be at the end of the list.
    EXPECT_EQ(ext_id, methods.at(1).id());
  }
  manager_->RemoveObserver(&observer);
}

TEST_F(InputMethodManagerImplTest, ChangeInputMethodComponentExtensionOneIME) {
  const std::string ext_id = extension_ime_util::GetComponentInputMethodID(
      extension_ime_util::kMozcExtensionId, "nacl_mozc_us");
  std::vector<std::string> ids;
  ids.push_back(ext_id);
  EXPECT_TRUE(manager_->GetActiveIMEState()->ReplaceEnabledInputMethods(ids));
  EXPECT_EQ(1U, manager_->GetActiveIMEState()->GetNumEnabledInputMethods());
  EXPECT_EQ(ext_id,
            manager_->GetActiveIMEState()->GetCurrentInputMethod().id());
}

TEST_F(InputMethodManagerImplTest, ChangeInputMethodComponentExtensionTwoIME) {
  const std::string ext_id1 = extension_ime_util::GetComponentInputMethodID(
      extension_ime_util::kMozcExtensionId, "nacl_mozc_us");
  const std::string ext_id2 = extension_ime_util::GetComponentInputMethodID(
      extension_ime_util::kT13nExtensionId, kExt2Engine1Id);
  std::vector<std::string> ids;
  ids.push_back(ext_id1);
  ids.push_back(ext_id2);
  EXPECT_TRUE(manager_->GetActiveIMEState()->ReplaceEnabledInputMethods(ids));
  EXPECT_EQ(2U, manager_->GetActiveIMEState()->GetNumEnabledInputMethods());
  EXPECT_EQ(ext_id1,
            manager_->GetActiveIMEState()->GetCurrentInputMethod().id());
  manager_->GetActiveIMEState()->ChangeInputMethod(ext_id2,
                                                   false /* show_message */);
  EXPECT_EQ(ext_id2,
            manager_->GetActiveIMEState()->GetCurrentInputMethod().id());
}

TEST_F(InputMethodManagerImplTest, GetMigratedInputMethodIDTest) {
  EXPECT_EQ(ImeIdFromEngineId("xkb:us::eng"),
            manager_->GetMigratedInputMethodID("xkb:us::eng"));
  EXPECT_EQ(ImeIdFromEngineId("xkb:fr::fra"),
            manager_->GetMigratedInputMethodID("xkb:fr::fra"));
  EXPECT_EQ(
      "_comp_ime_asdf_invalid_pinyin",
      manager_->GetMigratedInputMethodID("_comp_ime_asdf_invalid_pinyin"));
  EXPECT_EQ(ImeIdFromEngineId("zh-t-i0-pinyin"),
            manager_->GetMigratedInputMethodID("zh-t-i0-pinyin"));
}

TEST_F(InputMethodManagerImplTest, MigrateInputMethodsTest) {
  std::vector<std::string> input_method_ids;
  input_method_ids.emplace_back("xkb:us::eng");
  input_method_ids.emplace_back("xkb:fr::fra");
  input_method_ids.push_back(ImeIdFromEngineId("xkb:us::eng"));
  input_method_ids.emplace_back("xkb:fr::fra");
  input_method_ids.push_back(ImeIdFromEngineId("xkb:us::eng"));
  input_method_ids.emplace_back("_comp_ime_asdf_pinyin");
  input_method_ids.push_back(ImeIdFromEngineId(kPinyinImeId));

  manager_->GetMigratedInputMethodIDs(&input_method_ids);

  ASSERT_EQ(4U, input_method_ids.size());

  EXPECT_EQ(ImeIdFromEngineId("xkb:us::eng"), input_method_ids[0]);
  EXPECT_EQ(ImeIdFromEngineId("xkb:fr::fra"), input_method_ids[1]);
  EXPECT_EQ("_comp_ime_asdf_pinyin", input_method_ids[2]);
  EXPECT_EQ(ImeIdFromEngineId("zh-t-i0-pinyin"), input_method_ids[3]);
}

TEST_F(InputMethodManagerImplTest, OverrideKeyboardUrlRefWithKeyset) {
  // Create an input method with a input view URL for testing.
  const GURL inputview_url(
      "chrome-extension://"
      "inputview.html#id=us.compact.qwerty&language=en-US&passwordLayout=us."
      "compact.qwerty&name=keyboard_us");

  const auto ime_id =
      extension_ime_util::GetInputMethodID(kExtensionId1, "test_engine_id");
  InputMethodDescriptors descriptors;
  descriptors.push_back(InputMethodDescriptor(
      ime_id, "test", "TE", {}, {}, /*is_login_keyboard=*/false, GURL(),
      inputview_url, /*handwriting_language=*/std::nullopt));

  MockInputMethodEngine engine;
  std::vector<std::string> enabled_imes = {ime_id};
  manager_->GetActiveIMEState()->SetEnabledExtensionImes(enabled_imes);
  manager_->GetActiveIMEState()->AddInputMethodExtension(kExtensionId1,
                                                         descriptors, &engine);
  manager_->GetActiveIMEState()->ChangeInputMethod(ime_id, false);

  manager_->GetActiveIMEState()->EnableInputView();
  EXPECT_THAT(manager_->GetActiveIMEState()->GetInputViewUrl().spec(),
              ::testing::StartsWith(inputview_url.spec()));

  // Override the keyboard url ref with 'emoji'.
  const GURL overridden_url_emoji(
      "chrome-extension://"
      "inputview.html#id=us.compact.qwerty.emoji&language=en-US&passwordLayout="
      "us.compact.qwerty&name=keyboard_us");
  manager_->OverrideKeyboardKeyset(ImeKeyset::kEmoji);
  EXPECT_THAT(manager_->GetActiveIMEState()->GetInputViewUrl().spec(),
              ::testing::StartsWith(overridden_url_emoji.spec()));

  // Override the keyboard url ref with 'hwt'.
  const GURL overridden_url_hwt(
      "chrome-extension://"
      "inputview.html#id=us.compact.qwerty.hwt&language=en-US&passwordLayout="
      "us.compact.qwerty&name=keyboard_us");
  manager_->OverrideKeyboardKeyset(ImeKeyset::kHandwriting);
  EXPECT_THAT(manager_->GetActiveIMEState()->GetInputViewUrl().spec(),
              ::testing::StartsWith(overridden_url_hwt.spec()));

  // Override the keyboard url ref with 'voice'.
  const GURL overridden_url_voice(
      "chrome-extension://"
      "inputview.html#id=us.compact.qwerty.voice&language=en-US"
      "&passwordLayout=us.compact.qwerty&name=keyboard_us");
  manager_->OverrideKeyboardKeyset(ImeKeyset::kVoice);
  EXPECT_THAT(manager_->GetActiveIMEState()->GetInputViewUrl().spec(),
              ::testing::StartsWith(overridden_url_voice.spec()));
}

TEST_F(InputMethodManagerImplTest, OverrideDefaultKeyboardUrlRef) {
  const GURL default_url("chrome://inputview.html");

  const auto ime_id =
      extension_ime_util::GetInputMethodID(kExtensionId1, "test_engine_id");
  InputMethodDescriptors descriptors;
  descriptors.push_back(InputMethodDescriptor(
      ime_id, "test", "TE", {}, {}, /*is_login_keyboard=*/false, GURL(),
      default_url, /*handwriting_language=*/std::nullopt));

  MockInputMethodEngine engine;
  std::vector<std::string> enabled_imes = {ime_id};
  manager_->GetActiveIMEState()->SetEnabledExtensionImes(enabled_imes);
  manager_->GetActiveIMEState()->AddInputMethodExtension(kExtensionId1,
                                                         descriptors, &engine);
  manager_->GetActiveIMEState()->ChangeInputMethod(ime_id, false);
  manager_->GetActiveIMEState()->EnableInputView();

  manager_->OverrideKeyboardKeyset(ImeKeyset::kEmoji);
  EXPECT_EQ(default_url, manager_->GetActiveIMEState()->GetInputViewUrl());
}

TEST_F(InputMethodManagerImplTest, DoesNotResetInputViewUrlWhenOverridden) {
  // Create an input method with a input view URL for testing.
  const GURL inputview_url(
      "chrome-extension://"
      "inputview.html#id=us.compact.qwerty&language=en-US&passwordLayout=us."
      "compact.qwerty&name=keyboard_us");

  const auto ime_id =
      extension_ime_util::GetInputMethodID(kExtensionId1, "test_engine_id");
  InputMethodDescriptors descriptors;
  descriptors.push_back(InputMethodDescriptor(
      ime_id, "test", "TE", {}, {}, /*is_login_keyboard=*/false, GURL(),
      inputview_url, /*handwriting_language=*/std::nullopt));

  MockInputMethodEngine engine;
  std::vector<std::string> enabled_imes = {ime_id};
  manager_->GetActiveIMEState()->SetEnabledExtensionImes(enabled_imes);
  manager_->GetActiveIMEState()->AddInputMethodExtension(kExtensionId1,
                                                         descriptors, &engine);
  manager_->GetActiveIMEState()->ChangeInputMethod(ime_id, false);
  manager_->GetActiveIMEState()->EnableInputView();

  const GURL overridden_url_emoji(
      "chrome-extension://"
      "inputview.html#id=us.compact.qwerty.emoji&language=en-US&passwordLayout="
      "us.compact.qwerty&name=keyboard_us");

  manager_->OverrideKeyboardKeyset(ImeKeyset::kEmoji);
  EXPECT_THAT(manager_->GetActiveIMEState()->GetInputViewUrl().spec(),
              ::testing::StartsWith(overridden_url_emoji.spec()));

  manager_->GetActiveIMEState()->EnableInputView();
  EXPECT_THAT(manager_->GetActiveIMEState()->GetInputViewUrl().spec(),
              ::testing::StartsWith(overridden_url_emoji.spec()));
}

TEST_F(InputMethodManagerImplTest, AllowedInputMethodsValid) {
  // First, setup xkb:fr::fra input method
  std::string original_input_method(ImeIdFromEngineId("xkb:fr::fra"));
  ASSERT_TRUE(
      manager_->GetActiveIMEState()->EnableInputMethod(original_input_method));
  manager_->GetActiveIMEState()->ChangeInputMethod(original_input_method,
                                                   false);
  EXPECT_THAT(manager_->GetActiveIMEState()->GetCurrentInputMethod().id(),
              original_input_method);

  // Only allow xkb:us::eng
  std::vector<std::string> allowed = {"xkb:us::eng"};
  EXPECT_TRUE(manager_->GetActiveIMEState()->SetAllowedInputMethods(allowed));
  EXPECT_TRUE(manager_->GetActiveIMEState()->ReplaceEnabledInputMethods(
      manager_->GetActiveIMEState()->GetAllowedInputMethodIds()));
  EXPECT_THAT(manager_->GetActiveIMEState()->GetEnabledInputMethodIds(),
              testing::ElementsAre(ImeIdFromEngineId("xkb:us::eng")));
  EXPECT_THAT(manager_->GetActiveIMEState()->GetCurrentInputMethod().id(),
              ImeIdFromEngineId("xkb:us::eng"));
  EXPECT_THAT(manager_->GetActiveIMEState()->GetAllowedInputMethodIds(),
              testing::ElementsAre(ImeIdFromEngineId("xkb:us::eng")));
}

TEST_F(InputMethodManagerImplTest, AllowedInputMethodsInvalid) {
  // First, setup xkb:fr::fra input method
  std::string original_input_method(ImeIdFromEngineId("xkb:fr::fra"));
  ASSERT_TRUE(
      manager_->GetActiveIMEState()->EnableInputMethod(original_input_method));
  manager_->GetActiveIMEState()->ChangeInputMethod(original_input_method,
                                                   false);
  EXPECT_THAT(manager_->GetActiveIMEState()->GetCurrentInputMethod().id(),
              original_input_method);

  // Only allow xkb:us::eng
  std::vector<std::string> allowed = {"invalid_input_method"};
  EXPECT_FALSE(manager_->GetActiveIMEState()->SetAllowedInputMethods(allowed));
  EXPECT_THAT(manager_->GetActiveIMEState()->GetCurrentInputMethod().id(),
              original_input_method);
  EXPECT_THAT(manager_->GetActiveIMEState()->GetAllowedInputMethodIds(),
              testing::IsEmpty());
}

TEST_F(InputMethodManagerImplTest, AllowedInputMethodsValidAndInvalid) {
  // First, enable xkb:fr::fra and xkb:de::ger
  std::string original_input_method_1(ImeIdFromEngineId("xkb:fr::fra"));
  std::string original_input_method_2(ImeIdFromEngineId("xkb:de::ger"));
  ASSERT_TRUE(manager_->GetActiveIMEState()->EnableInputMethod(
      original_input_method_1));
  ASSERT_TRUE(manager_->GetActiveIMEState()->EnableInputMethod(
      original_input_method_2));
  manager_->GetActiveIMEState()->ChangeInputMethod(original_input_method_1,
                                                   false);

  // Allow xkb:fr::fra and an invalid input method id. The invalid id should be
  // ignored.
  std::vector<std::string> allowed = {original_input_method_1,
                                      "invalid_input_method"};
  EXPECT_TRUE(manager_->GetActiveIMEState()->SetAllowedInputMethods(allowed));
  EXPECT_TRUE(manager_->GetActiveIMEState()->ReplaceEnabledInputMethods(
      manager_->GetActiveIMEState()->GetAllowedInputMethodIds()));
  EXPECT_THAT(manager_->GetActiveIMEState()->GetCurrentInputMethod().id(),
              original_input_method_1);
  EXPECT_THAT(manager_->GetActiveIMEState()->GetAllowedInputMethodIds(),
              testing::ElementsAre(original_input_method_1));

  // Try to re-enable xkb:de::ger
  EXPECT_FALSE(manager_->GetActiveIMEState()->EnableInputMethod(
      original_input_method_2));
}

TEST_F(InputMethodManagerImplTest, AllowedInputMethodsAndExtensions) {
  EXPECT_TRUE(manager_->GetActiveIMEState()->EnableInputMethod(
      ImeIdFromEngineId(kNaclMozcJpId)));
  EXPECT_TRUE(manager_->GetActiveIMEState()->EnableInputMethod(
      ImeIdFromEngineId("xkb:fr::fra")));

  std::vector<std::string> allowed = {"xkb:us::eng", kNaclMozcJpId};
  EXPECT_TRUE(manager_->GetActiveIMEState()->SetAllowedInputMethods(allowed));
  EXPECT_TRUE(manager_->GetActiveIMEState()->ReplaceEnabledInputMethods(
      manager_->GetActiveIMEState()->GetAllowedInputMethodIds()));

  EXPECT_FALSE(manager_->GetActiveIMEState()->EnableInputMethod(
      ImeIdFromEngineId(kNaclMozcUsId)));
  EXPECT_THAT(manager_->GetActiveIMEState()->GetEnabledInputMethodIds(),
              testing::ElementsAre(ImeIdFromEngineId("xkb:us::eng"),
                                   ImeIdFromEngineId(kNaclMozcJpId)));
}

class InputMethodManagerImplKioskTest : public InputMethodManagerImplTest {
 public:
  void LogIn(const std::string& email) override {
    chromeos::SetUpFakeKioskSession(email);
    ash_test_helper()->test_session_controller_client()->AddUserSession(
        email, user_manager::UserType::kKioskApp);
  }
};

TEST_F(InputMethodManagerImplKioskTest, EnableAllowedInputMethods) {
  // First, setup xkb:fr::fra input method
  std::string original_input_method(ImeIdFromEngineId("xkb:fr::fra"));
  ASSERT_TRUE(
      manager_->GetActiveIMEState()->EnableInputMethod(original_input_method));
  manager_->GetActiveIMEState()->ChangeInputMethod(original_input_method,
                                                   false);
  EXPECT_THAT(manager_->GetActiveIMEState()->GetCurrentInputMethod().id(),
              original_input_method);

  // Also allow xkb:us::eng and xkb:de::ger.
  std::vector<std::string> allowed = {"xkb:us::eng", "xkb:de::ger"};
  EXPECT_TRUE(manager_->GetActiveIMEState()->SetAllowedInputMethods(allowed));

  // Fix enabled languages according to allowed languages filter.
  manager_->GetActiveIMEState()->ReplaceEnabledInputMethods(
      manager_->GetActiveIMEState()->GetEnabledInputMethodIds());

  // Check that all allowed languages are enabled languages.
  EXPECT_THAT(manager_->GetActiveIMEState()->GetEnabledInputMethodIds(),
              testing::ElementsAre(ImeIdFromEngineId("xkb:us::eng"),
                                   ImeIdFromEngineId("xkb:de::ger")));
  EXPECT_THAT(manager_->GetActiveIMEState()->GetAllowedInputMethodIds(),
              testing::ElementsAre(ImeIdFromEngineId("xkb:us::eng"),
                                   ImeIdFromEngineId("xkb:de::ger")));
}

TEST_F(InputMethodManagerImplTest, SetLoginDefaultWithAllowedInputMethods) {
  std::vector<std::string> allowed = {"xkb:us::eng", "xkb:de::ger",
                                      "xkb:fr::fra"};
  EXPECT_TRUE(manager_->GetActiveIMEState()->SetAllowedInputMethods(allowed));
  EXPECT_TRUE(manager_->GetActiveIMEState()->ReplaceEnabledInputMethods(
      manager_->GetActiveIMEState()->GetAllowedInputMethodIds()));
  manager_->GetActiveIMEState()->SetInputMethodLoginDefault();
  EXPECT_THAT(manager_->GetActiveIMEState()->GetEnabledInputMethodIds(),
              testing::ElementsAre(ImeIdFromEngineId("xkb:us::eng"),
                                   ImeIdFromEngineId("xkb:de::ger"),
                                   ImeIdFromEngineId("xkb:fr::fra")));
}

// Verifies that the combination of InputMethodManagerImpl and
// ImeControllerClientImpl sends the correct data to ash.
TEST_F(InputMethodManagerImplTest, IntegrationWithAsh) {
  TestImeController ime_controller;
  ImeControllerClientImpl ime_controller_client(manager_);
  ime_controller_client.Init();

  // Setup 3 IMEs.
  std::string id1 = ImeIdFromEngineId("xkb:us:dvorak:eng");
  std::string id2 = ImeIdFromEngineId(kExt2Engine2Id);
  std::string id3 = ImeIdFromEngineId(kExt2Engine1Id);
  std::vector<std::string> ids{id1, id2, id3};
  manager_->GetActiveIMEState()->ReplaceEnabledInputMethods(ids);

  // Ash received the IMEs.
  ASSERT_EQ(3u, ime_controller.available_imes_.size());
  EXPECT_EQ(id1, ime_controller.current_ime_id_);

  // Switch to another IME.
  manager_->GetActiveIMEState()->ChangeInputMethod(id3, false);
  EXPECT_EQ(id3, ime_controller.current_ime_id_);

  // Lock the screen.
  scoped_refptr<InputMethodManager::State> saved_ime_state =
      manager_->GetActiveIMEState();
  manager_->SetState(saved_ime_state->Clone());
  manager_->GetActiveIMEState()->DisableNonLockScreenLayouts();
  EXPECT_EQ(2u, ime_controller.available_imes_.size());  // id1, hardware layout
  EXPECT_EQ(id1, ime_controller.current_ime_id_);

  std::string hardware_layout_ime_id = ImeIdFromEngineId("xkb:us::eng");
  manager_->GetActiveIMEState()->ChangeInputMethod(hardware_layout_ime_id,
                                                   false);
  EXPECT_EQ(hardware_layout_ime_id, ime_controller.current_ime_id_);

  // Unlock screen. The original state is restored.
  manager_->SetState(saved_ime_state);
  EXPECT_EQ(manager_->GetActiveIMEState()->GetUIStyle(),
            InputMethodManager::UIStyle::kNormal);
  ASSERT_EQ(3u, ime_controller.available_imes_.size());
  EXPECT_EQ(id3, ime_controller.current_ime_id_);
}

TEST_F(InputMethodManagerImplTest, SetFeaturesDisabled) {
  // All features are enabled by default.
  EXPECT_TRUE(
      manager_->GetImeMenuFeatureEnabled(InputMethodManager::FEATURE_ALL));
  EXPECT_TRUE(
      manager_->GetImeMenuFeatureEnabled(InputMethodManager::FEATURE_EMOJI));
  EXPECT_TRUE(manager_->GetImeMenuFeatureEnabled(
      InputMethodManager::FEATURE_HANDWRITING));
  EXPECT_TRUE(
      manager_->GetImeMenuFeatureEnabled(InputMethodManager::FEATURE_VOICE));

  // Sets emoji disabled and then enabled.
  manager_->SetImeMenuFeatureEnabled(InputMethodManager::FEATURE_EMOJI, false);
  EXPECT_FALSE(
      manager_->GetImeMenuFeatureEnabled(InputMethodManager::FEATURE_EMOJI));
  manager_->SetImeMenuFeatureEnabled(InputMethodManager::FEATURE_EMOJI, true);
  EXPECT_TRUE(
      manager_->GetImeMenuFeatureEnabled(InputMethodManager::FEATURE_EMOJI));

  // Sets voice disabled and then enabled.
  manager_->SetImeMenuFeatureEnabled(InputMethodManager::FEATURE_VOICE, false);
  EXPECT_FALSE(
      manager_->GetImeMenuFeatureEnabled(InputMethodManager::FEATURE_VOICE));
  manager_->SetImeMenuFeatureEnabled(InputMethodManager::FEATURE_VOICE, true);
  EXPECT_TRUE(
      manager_->GetImeMenuFeatureEnabled(InputMethodManager::FEATURE_VOICE));

  // Sets handwriting disabled and then enabled.
  manager_->SetImeMenuFeatureEnabled(InputMethodManager::FEATURE_HANDWRITING,
                                     false);
  EXPECT_FALSE(manager_->GetImeMenuFeatureEnabled(
      InputMethodManager::FEATURE_HANDWRITING));
  manager_->SetImeMenuFeatureEnabled(InputMethodManager::FEATURE_HANDWRITING,
                                     true);
  EXPECT_TRUE(manager_->GetImeMenuFeatureEnabled(
      InputMethodManager::FEATURE_HANDWRITING));
}

TEST_F(InputMethodManagerImplTest, TestAddRemoveArcInputMethods) {
  // There is one default IME
  EXPECT_EQ(1u, manager_->GetActiveIMEState()->GetNumEnabledInputMethods());

  // Add an ARC IMEs.
  std::vector<std::string> languages({"en-US"});

  MockInputMethodEngine engine;

  const std::string ime_id =
      extension_ime_util::GetArcInputMethodID(kExtensionId1, "engine_id");
  const InputMethodDescriptor descriptor(
      ime_id, "arc ime", "AI", "us" /* layout */, languages,
      false /* is_login_keyboard */, GURL(), GURL(),
      /*handwriting_language=*/std::nullopt);
  InputMethodDescriptors descriptors({descriptor});
  manager_->GetActiveIMEState()->AddInputMethodExtension(kExtensionId1,
                                                         descriptors, &engine);

  InputMethodDescriptors result;
  manager_->GetActiveIMEState()->GetInputMethodExtensions(&result);
  EXPECT_EQ(1u, result.size());
  EXPECT_EQ(ime_id, result[0].id());
  result.clear();

  // The ARC IME is not enabled by default.
  EXPECT_EQ(1u, manager_->GetActiveIMEState()->GetNumEnabledInputMethods());

  // Enable it.
  std::vector<std::string> extension_ime_ids({ime_id});
  manager_->GetActiveIMEState()->SetEnabledExtensionImes(extension_ime_ids);
  EXPECT_EQ(2u, manager_->GetActiveIMEState()->GetNumEnabledInputMethods());
  {
    InputMethodDescriptors methods =
        manager_->GetActiveIMEState()->GetEnabledInputMethods();
    EXPECT_EQ(2u, methods.size());
    EXPECT_EQ(ime_id, methods.at(1).id());
  }

  // Change to it.
  manager_->GetActiveIMEState()->ChangeInputMethod(ime_id,
                                                   false /* show_message */);
  InputMethodDescriptor current =
      manager_->GetActiveIMEState()->GetCurrentInputMethod();
  EXPECT_EQ(ime_id, current.id());

  // Remove it.
  manager_->GetActiveIMEState()->RemoveInputMethodExtension(kExtensionId1);
  manager_->GetActiveIMEState()->GetInputMethodExtensions(&result);
  EXPECT_TRUE(result.empty());
}

// TODO(crbug.com/1179893): Remove once the feature is enabled permanently.
class InputMethodManagerImplPositionalTest : public InputMethodManagerImplTest {
 public:
  InputMethodManagerImplPositionalTest() = default;
  ~InputMethodManagerImplPositionalTest() override = default;

  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(
        ::features::kImprovedKeyboardShortcuts);

    InputMethodManagerImplTest::SetUp();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(InputMethodManagerImplPositionalTest, ValidatePositionalShortcutLayout) {
  // Initialize with one positional (US) and one non-positional (US-dvorak)
  // layout.
  std::string us_id = ImeIdFromEngineId("xkb:us::eng");
  std::string us_dvorak_id = ImeIdFromEngineId("xkb:us:dvorak:eng");
  std::vector<std::string> ids{us_id, us_dvorak_id};
  EXPECT_TRUE(manager_->GetActiveIMEState()->ReplaceEnabledInputMethods(ids));
  EXPECT_EQ(2U, manager_->GetActiveIMEState()->GetNumEnabledInputMethods());

  // Verify the US layout is positional.
  EXPECT_EQ(us_id, manager_->GetActiveIMEState()->GetCurrentInputMethod().id());
  EXPECT_EQ("us", keyboard_->GetCurrentKeyboardLayoutName());
  EXPECT_TRUE(manager_->ArePositionalShortcutsUsedByCurrentInputMethod());

  // Switch to dvorak and verify it is non-positional.
  manager_->GetActiveIMEState()->ChangeInputMethod(us_dvorak_id,
                                                   /*show_message=*/false);
  EXPECT_EQ(us_dvorak_id,
            manager_->GetActiveIMEState()->GetCurrentInputMethod().id());
  EXPECT_EQ("us(dvorak)", keyboard_->GetCurrentKeyboardLayoutName());
  EXPECT_FALSE(manager_->ArePositionalShortcutsUsedByCurrentInputMethod());

  // Switch back to US and verify it is positional again.
  manager_->GetActiveIMEState()->ChangeInputMethod(us_id,
                                                   /*show_message=*/false);
  EXPECT_EQ(us_id, manager_->GetActiveIMEState()->GetCurrentInputMethod().id());
  EXPECT_EQ("us", keyboard_->GetCurrentKeyboardLayoutName());
  EXPECT_TRUE(manager_->ArePositionalShortcutsUsedByCurrentInputMethod());
}

}  // namespace input_method
}  // namespace ash
