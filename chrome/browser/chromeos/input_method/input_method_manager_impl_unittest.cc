// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/input_method/input_method_manager_impl.h"

#include <stddef.h>

#include <algorithm>
#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "chrome/browser/chromeos/input_method/mock_candidate_window_controller.h"
#include "chrome/browser/chromeos/input_method/mock_input_method_engine.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ash/ime_controller_client.h"
#include "chrome/browser/ui/ash/keyboard/chrome_keyboard_controller_client_test_helper.h"
#include "chrome/browser/ui/ash/test_ime_controller.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ime/chromeos/extension_ime_util.h"
#include "ui/base/ime/chromeos/fake_ime_keyboard.h"
#include "ui/base/ime/chromeos/fake_input_method_delegate.h"
#include "ui/base/ime/chromeos/mock_component_extension_ime_manager_delegate.h"
#include "ui/base/ime/chromeos/mock_ime_engine_handler.h"
#include "ui/base/ime/ime_bridge.h"
#include "ui/base/ime/init/input_method_initializer.h"

namespace chromeos {

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
    if (descriptor.id() == target.id())
      return true;
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

 private:
  DISALLOW_COPY_AND_ASSIGN(TestObserver);
};

class TestCandidateWindowObserver
    : public InputMethodManager::CandidateWindowObserver {
 public:
  TestCandidateWindowObserver()
      : candidate_window_opened_count_(0),
        candidate_window_closed_count_(0) {
  }

  ~TestCandidateWindowObserver() override = default;

  void CandidateWindowOpened(InputMethodManager* manager) override {
    ++candidate_window_opened_count_;
  }
  void CandidateWindowClosed(InputMethodManager* manager) override {
    ++candidate_window_closed_count_;
  }

  int candidate_window_opened_count_;
  int candidate_window_closed_count_;

 private:
  DISALLOW_COPY_AND_ASSIGN(TestCandidateWindowObserver);
};
}  // namespace

class InputMethodManagerImplTest :  public BrowserWithTestWindowTest {
 public:
  InputMethodManagerImplTest()
      : delegate_(nullptr),
        candidate_window_controller_(nullptr),
        keyboard_(nullptr) {
  }

  ~InputMethodManagerImplTest() override = default;

  void SetUp() override {
    ui::InitializeInputMethodForTesting();

    delegate_ = new FakeInputMethodDelegate();
    manager_.reset(new InputMethodManagerImpl(
        std::unique_ptr<InputMethodDelegate>(delegate_), false));
    manager_->GetInputMethodUtil()->UpdateHardwareLayoutCache();
    candidate_window_controller_ = new MockCandidateWindowController;
    manager_->SetCandidateWindowControllerForTesting(
        candidate_window_controller_);
    keyboard_ = new FakeImeKeyboard;
    manager_->SetImeKeyboardForTesting(keyboard_);
    mock_engine_handler_.reset(new MockInputMethodEngine());
    ui::IMEBridge::Initialize();
    ui::IMEBridge::Get()->SetCurrentEngineHandler(mock_engine_handler_.get());

    menu_manager_ = ui::ime::InputMethodMenuManager::GetInstance();

    InitImeList();

    BrowserWithTestWindowTest::SetUp();

    // Needs ash::Shell keyboard to be created first.
    chrome_keyboard_controller_client_test_helper_ =
        ChromeKeyboardControllerClientTestHelper::InitializeForAsh();
  }

  void TearDown() override {
    // Needs to destroyed before ash::Shell keyboard.
    chrome_keyboard_controller_client_test_helper_.reset();

    BrowserWithTestWindowTest::TearDown();

    ui::ShutdownInputMethodForTesting();

    delegate_ = nullptr;
    candidate_window_controller_ = nullptr;
    keyboard_ = nullptr;
    manager_.reset();
  }

  scoped_refptr<InputMethodManagerImpl::StateImpl> GetActiveIMEState() {
    return scoped_refptr<InputMethodManagerImpl::StateImpl>(
        manager_->state_.get());
  }

 protected:
  // Helper function to initialize component extension stuff for testing.
  void InitComponentExtension() {
    mock_delegate_ = new MockComponentExtIMEManagerDelegate();
    mock_delegate_->set_ime_list(ime_list_);
    std::unique_ptr<ComponentExtensionIMEManagerDelegate> delegate(
        mock_delegate_);

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

    std::vector<std::string> layouts;
    layouts.emplace_back("us");
    std::vector<std::string> languages;
    languages.emplace_back("en-US");

    // Note, for production, these SetEngineHandler are called when
    // IMEEngineHandlerInterface is initialized via
    // InitializeComponentextension.
    manager_->InitializeComponentExtensionForTesting(std::move(delegate));
  }

  void InitImeList() {
    ime_list_.clear();

    ComponentExtensionIME ext_xkb;
    ext_xkb.id = extension_ime_util::kXkbExtensionId;
    ext_xkb.description = "ext_xkb_description";
    ext_xkb.path = base::FilePath("ext_xkb_file_path");

    ComponentExtensionEngine ext_xkb_engine_us;
    ext_xkb_engine_us.engine_id = "xkb:us::eng";
    ext_xkb_engine_us.display_name = "xkb:us::eng";
    ext_xkb_engine_us.language_codes.emplace_back("en-US");
    ext_xkb_engine_us.layouts.emplace_back("us");
    ext_xkb.engines.push_back(ext_xkb_engine_us);

    ComponentExtensionEngine ext_xkb_engine_intl;
    ext_xkb_engine_intl.engine_id = "xkb:us:intl:eng";
    ext_xkb_engine_intl.display_name = "xkb:us:intl:eng";
    ext_xkb_engine_intl.language_codes.emplace_back("en-US");
    ext_xkb_engine_intl.layouts.emplace_back("us(intl)");
    ext_xkb.engines.push_back(ext_xkb_engine_intl);

    ComponentExtensionEngine ext_xkb_engine_altgr_intl;
    ext_xkb_engine_altgr_intl.engine_id = "xkb:us:altgr-intl:eng";
    ext_xkb_engine_altgr_intl.display_name = "xkb:us:altgr-intl:eng";
    ext_xkb_engine_altgr_intl.language_codes.emplace_back("en-US");
    ext_xkb_engine_altgr_intl.layouts.emplace_back("us(altgr-intl)");
    ext_xkb.engines.push_back(ext_xkb_engine_altgr_intl);

    ComponentExtensionEngine ext_xkb_engine_dvorak;
    ext_xkb_engine_dvorak.engine_id = "xkb:us:dvorak:eng";
    ext_xkb_engine_dvorak.display_name = "xkb:us:dvorak:eng";
    ext_xkb_engine_dvorak.language_codes.emplace_back("en-US");
    ext_xkb_engine_dvorak.layouts.emplace_back("us(dvorak)");
    ext_xkb.engines.push_back(ext_xkb_engine_dvorak);

    ComponentExtensionEngine ext_xkb_engine_dvp;
    ext_xkb_engine_dvp.engine_id = "xkb:us:dvp:eng";
    ext_xkb_engine_dvp.display_name = "xkb:us:dvp:eng";
    ext_xkb_engine_dvp.language_codes.emplace_back("en-US");
    ext_xkb_engine_dvp.layouts.emplace_back("us(dvp)");
    ext_xkb.engines.push_back(ext_xkb_engine_dvp);

    ComponentExtensionEngine ext_xkb_engine_colemak;
    ext_xkb_engine_colemak.engine_id = "xkb:us:colemak:eng";
    ext_xkb_engine_colemak.display_name = "xkb:us:colemak:eng";
    ext_xkb_engine_colemak.language_codes.emplace_back("en-US");
    ext_xkb_engine_colemak.layouts.emplace_back("us(colemak)");
    ext_xkb.engines.push_back(ext_xkb_engine_colemak);

    ComponentExtensionEngine ext_xkb_engine_workman;
    ext_xkb_engine_workman.engine_id = "xkb:us:workman:eng";
    ext_xkb_engine_workman.display_name = "xkb:us:workman:eng";
    ext_xkb_engine_workman.language_codes.emplace_back("en-US");
    ext_xkb_engine_workman.layouts.emplace_back("us(workman)");
    ext_xkb.engines.push_back(ext_xkb_engine_workman);

    ComponentExtensionEngine ext_xkb_engine_workman_intl;
    ext_xkb_engine_workman_intl.engine_id = "xkb:us:workman-intl:eng";
    ext_xkb_engine_workman_intl.display_name = "xkb:us:workman-intl:eng";
    ext_xkb_engine_workman_intl.language_codes.emplace_back("en-US");
    ext_xkb_engine_workman_intl.layouts.emplace_back("us(workman-intl)");
    ext_xkb.engines.push_back(ext_xkb_engine_workman_intl);

    ComponentExtensionEngine ext_xkb_engine_fr;
    ext_xkb_engine_fr.engine_id = "xkb:fr::fra";
    ext_xkb_engine_fr.display_name = "xkb:fr::fra";
    ext_xkb_engine_fr.language_codes.emplace_back("fr");
    ext_xkb_engine_fr.layouts.emplace_back("fr");
    ext_xkb.engines.push_back(ext_xkb_engine_fr);

    ComponentExtensionEngine ext_xkb_engine_se;
    ext_xkb_engine_se.engine_id = "xkb:se::swe";
    ext_xkb_engine_se.display_name = "xkb:se::swe";
    ext_xkb_engine_se.language_codes.emplace_back("sv");
    ext_xkb_engine_se.layouts.emplace_back("se");
    ext_xkb.engines.push_back(ext_xkb_engine_se);

    ComponentExtensionEngine ext_xkb_engine_jp;
    ext_xkb_engine_jp.engine_id = "xkb:jp::jpn";
    ext_xkb_engine_jp.display_name = "xkb:jp::jpn";
    ext_xkb_engine_jp.language_codes.emplace_back("ja");
    ext_xkb_engine_jp.layouts.emplace_back("jp");
    ext_xkb.engines.push_back(ext_xkb_engine_jp);

    ComponentExtensionEngine ext_xkb_engine_ru;
    ext_xkb_engine_ru.engine_id = "xkb:ru::rus";
    ext_xkb_engine_ru.display_name = "xkb:ru::rus";
    ext_xkb_engine_ru.language_codes.emplace_back("ru");
    ext_xkb_engine_ru.layouts.emplace_back("ru");
    ext_xkb.engines.push_back(ext_xkb_engine_ru);

    ComponentExtensionEngine ext_xkb_engine_hu;
    ext_xkb_engine_hu.engine_id = "xkb:hu::hun";
    ext_xkb_engine_hu.display_name = "xkb:hu::hun";
    ext_xkb_engine_hu.language_codes.emplace_back("hu");
    ext_xkb_engine_hu.layouts.emplace_back("hu");
    ext_xkb.engines.push_back(ext_xkb_engine_hu);

    ime_list_.push_back(ext_xkb);

    ComponentExtensionIME ext1;
    ext1.id = extension_ime_util::kMozcExtensionId;
    ext1.description = "ext1_description";
    ext1.path = base::FilePath("ext1_file_path");

    ComponentExtensionEngine ext1_engine1;
    ext1_engine1.engine_id = "nacl_mozc_us";
    ext1_engine1.display_name = "ext1_engine_1_display_name";
    ext1_engine1.language_codes.emplace_back("ja");
    ext1_engine1.layouts.emplace_back("us");
    ext1.engines.push_back(ext1_engine1);

    ComponentExtensionEngine ext1_engine2;
    ext1_engine2.engine_id = "nacl_mozc_jp";
    ext1_engine2.display_name = "ext1_engine_1_display_name";
    ext1_engine2.language_codes.emplace_back("ja");
    ext1_engine2.layouts.emplace_back("jp");
    ext1.engines.push_back(ext1_engine2);

    ime_list_.push_back(ext1);

    ComponentExtensionIME ext2;
    ext2.id = extension_ime_util::kT13nExtensionId;
    ext2.description = "ext2_description";
    ext2.path = base::FilePath("ext2_file_path");

    ComponentExtensionEngine ext2_engine1;
    ext2_engine1.engine_id = kExt2Engine1Id;
    ext2_engine1.display_name = "ext2_engine_1_display_name";
    ext2_engine1.language_codes.emplace_back("en");
    ext2_engine1.layouts.emplace_back("us");
    ext2.engines.push_back(ext2_engine1);

    ComponentExtensionEngine ext2_engine2;
    ext2_engine2.engine_id = kExt2Engine2Id;
    ext2_engine2.display_name = "ext2_engine_2_display_name";
    ext2_engine2.language_codes.emplace_back("en");
    ext2_engine2.layouts.emplace_back("us(dvorak)");
    ext2.engines.push_back(ext2_engine2);

    ime_list_.push_back(ext2);
  }

  std::unique_ptr<ChromeKeyboardControllerClientTestHelper>
      chrome_keyboard_controller_client_test_helper_;
  std::unique_ptr<InputMethodManagerImpl> manager_;
  FakeInputMethodDelegate* delegate_;
  MockCandidateWindowController* candidate_window_controller_;
  std::unique_ptr<MockInputMethodEngine> mock_engine_handler_;
  FakeImeKeyboard* keyboard_;
  MockComponentExtIMEManagerDelegate* mock_delegate_;
  std::vector<ComponentExtensionIME> ime_list_;
  ui::ime::InputMethodMenuManager* menu_manager_;

 private:
  DISALLOW_COPY_AND_ASSIGN(InputMethodManagerImplTest);
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
  // For http://crbug.com/19655#c11 - (3). browser_state_monitor_unittest.cc is
  // also for the scenario.
  std::vector<std::string> keyboard_layouts;
  keyboard_layouts.emplace_back("xkb:us::eng");

  TestObserver observer;
  InitComponentExtension();
  manager_->AddObserver(&observer);
  menu_manager_->AddObserver(&observer);
  EXPECT_EQ(0, observer.input_method_changed_count_);
  EXPECT_EQ(0, observer.input_method_extension_added_count_);
  EXPECT_EQ(0, observer.input_method_extension_removed_count_);
  manager_->GetActiveIMEState()->EnableLoginLayouts("en-US", keyboard_layouts);
  EXPECT_EQ(8U, manager_->GetActiveIMEState()->GetActiveInputMethods()->size());
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
  // TODO(komatsu): Revisit if this is neccessary.
  EXPECT_EQ(3, observer.input_method_changed_count_);

  // If the same input method ID is passed, PropertyChanged() is not
  // notified.
  EXPECT_EQ(1, observer.input_method_menu_item_changed_count_);

  // Add an ARC IME, remove it, then check the observer counts.
  MockInputMethodEngine engine;
  const std::string ime_id =
      extension_ime_util::GetArcInputMethodID(kExtensionId1, "engine_id");
  InputMethodDescriptor descriptor(ime_id, "arc ime", "AI", {"us"}, {"en-US"},
                                   false /* is_login_keyboard */, GURL(),
                                   GURL());
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
  InitComponentExtension();
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

  InitComponentExtension();
  manager_->GetActiveIMEState()->EnableLoginLayouts("en-US", keyboard_layouts);
  EXPECT_EQ(8U, manager_->GetActiveIMEState()->GetNumActiveInputMethods());

  // For http://crbug.com/19655#c11 - (5)
  // The hardware keyboard layout "xkb:us::eng" is always active, hence 2U.
  manager_->GetActiveIMEState()->EnableLoginLayouts(
      "ja", keyboard_layouts);  // Japanese
  EXPECT_EQ(2U, manager_->GetActiveIMEState()->GetNumActiveInputMethods());
}

TEST_F(InputMethodManagerImplTest, TestEnableLayoutsAndCurrentInputMethod) {
  // For http://crbug.com/329061
  std::vector<std::string> keyboard_layouts;
  keyboard_layouts.push_back(ImeIdFromEngineId("xkb:se::swe"));

  InitComponentExtension();
  manager_->GetActiveIMEState()->EnableLoginLayouts("en-US", keyboard_layouts);
  const std::string im_id =
      manager_->GetActiveIMEState()->GetCurrentInputMethod().id();
  EXPECT_EQ(ImeIdFromEngineId("xkb:se::swe"), im_id);
}

TEST_F(InputMethodManagerImplTest, TestEnableLayoutsNonUsHardwareKeyboard) {
  InitComponentExtension();
  // The physical layout is French.
  manager_->GetInputMethodUtil()->SetHardwareKeyboardLayoutForTesting(
      "xkb:fr::fra");
  manager_->GetActiveIMEState()->EnableLoginLayouts(
      "en-US",
      manager_->GetInputMethodUtil()->GetHardwareLoginInputMethodIds());
  EXPECT_EQ(
      9U,
      manager_->GetActiveIMEState()->GetNumActiveInputMethods());  // 8 + French
  // The physical layout is Japanese.
  manager_->GetInputMethodUtil()->SetHardwareKeyboardLayoutForTesting(
      "xkb:jp::jpn");
  manager_->GetActiveIMEState()->EnableLoginLayouts(
      "ja", manager_->GetInputMethodUtil()->GetHardwareLoginInputMethodIds());
  // "xkb:us::eng" is not needed, hence 1.
  EXPECT_EQ(1U, manager_->GetActiveIMEState()->GetNumActiveInputMethods());

  // The physical layout is Russian.
  manager_->GetInputMethodUtil()->SetHardwareKeyboardLayoutForTesting(
      "xkb:ru::rus");
  manager_->GetActiveIMEState()->EnableLoginLayouts(
      "ru", manager_->GetInputMethodUtil()->GetHardwareLoginInputMethodIds());
  // "xkb:us::eng" only.
  EXPECT_EQ(1U, manager_->GetActiveIMEState()->GetNumActiveInputMethods());
  EXPECT_EQ(ImeIdFromEngineId("xkb:us::eng"),
            manager_->GetActiveIMEState()->GetActiveInputMethodIds().front());
}

TEST_F(InputMethodManagerImplTest, TestEnableMultipleHardwareKeyboardLayout) {
  InitComponentExtension();
  // The physical layouts are French and Hungarian.
  manager_->GetInputMethodUtil()->SetHardwareKeyboardLayoutForTesting(
      "xkb:fr::fra,xkb:hu::hun");
  manager_->GetActiveIMEState()->EnableLoginLayouts(
      "en-US",
      manager_->GetInputMethodUtil()->GetHardwareLoginInputMethodIds());
  // 8 + French + Hungarian
  EXPECT_EQ(10U, manager_->GetActiveIMEState()->GetNumActiveInputMethods());
}

TEST_F(InputMethodManagerImplTest,
       TestEnableMultipleHardwareKeyboardLayout_NoLoginKeyboard) {
  InitComponentExtension();
  // The physical layouts are English (US) and Russian.
  manager_->GetInputMethodUtil()->SetHardwareKeyboardLayoutForTesting(
      "xkb:us::eng,xkb:ru::rus");
  manager_->GetActiveIMEState()->EnableLoginLayouts(
      "ru", manager_->GetInputMethodUtil()->GetHardwareLoginInputMethodIds());
  // xkb:us:eng
  EXPECT_EQ(1U, manager_->GetActiveIMEState()->GetNumActiveInputMethods());
}

TEST_F(InputMethodManagerImplTest, TestActiveInputMethods) {
  InitComponentExtension();
  std::vector<std::string> keyboard_layouts;
  manager_->GetActiveIMEState()->EnableLoginLayouts(
      "ja", keyboard_layouts);  // Japanese
  EXPECT_EQ(2U, manager_->GetActiveIMEState()->GetNumActiveInputMethods());
  std::unique_ptr<InputMethodDescriptors> methods(
      manager_->GetActiveIMEState()->GetActiveInputMethods());
  ASSERT_TRUE(methods.get());
  EXPECT_EQ(2U, methods->size());
  const InputMethodDescriptor* id_to_find =
      manager_->GetInputMethodUtil()->GetInputMethodDescriptorFromId(
          ImeIdFromEngineId("xkb:us::eng"));
  EXPECT_TRUE(id_to_find && Contain(*methods.get(), *id_to_find));
  id_to_find = manager_->GetInputMethodUtil()->GetInputMethodDescriptorFromId(
      ImeIdFromEngineId("xkb:jp::jpn"));
  EXPECT_TRUE(id_to_find && Contain(*methods.get(), *id_to_find));
}

TEST_F(InputMethodManagerImplTest, TestEnableTwoLayouts) {
  // For http://crbug.com/19655#c11 - (8), step 6.
  TestObserver observer;
  InitComponentExtension();
  manager_->AddObserver(&observer);
  manager_->SetUISessionState(InputMethodManager::STATE_BROWSER_SCREEN);
  std::vector<std::string> ids;
  ids.push_back(ImeIdFromEngineId("xkb:us:dvorak:eng"));
  ids.push_back(ImeIdFromEngineId("xkb:us:colemak:eng"));
  EXPECT_TRUE(manager_->GetActiveIMEState()->ReplaceEnabledInputMethods(ids));
  EXPECT_EQ(2U, manager_->GetActiveIMEState()->GetNumActiveInputMethods());
  // Since all the IDs added avobe are keyboard layouts, Start() should not be
  // called.
  EXPECT_EQ(1, observer.input_method_changed_count_);
  EXPECT_EQ(ImeIdFromEngineId(ids[0]),
            manager_->GetActiveIMEState()->GetCurrentInputMethod().id());
  EXPECT_EQ("us(dvorak)", keyboard_->last_layout_);
  // Disable Dvorak.
  ids.erase(ids.begin());
  EXPECT_TRUE(manager_->GetActiveIMEState()->ReplaceEnabledInputMethods(ids));
  EXPECT_EQ(1U, manager_->GetActiveIMEState()->GetNumActiveInputMethods());
  EXPECT_EQ(2, observer.input_method_changed_count_);
  EXPECT_EQ(ImeIdFromEngineId(ids[0]),  // colemak
            manager_->GetActiveIMEState()->GetCurrentInputMethod().id());
  EXPECT_EQ("us(colemak)", keyboard_->last_layout_);
  manager_->RemoveObserver(&observer);
}

TEST_F(InputMethodManagerImplTest, TestEnableThreeLayouts) {
  // For http://crbug.com/19655#c11 - (9).
  TestObserver observer;
  InitComponentExtension();
  manager_->AddObserver(&observer);
  manager_->SetUISessionState(InputMethodManager::STATE_BROWSER_SCREEN);
  std::vector<std::string> ids;
  ids.push_back(ImeIdFromEngineId("xkb:us::eng"));
  ids.push_back(ImeIdFromEngineId("xkb:us:dvorak:eng"));
  ids.push_back(ImeIdFromEngineId("xkb:us:colemak:eng"));
  EXPECT_TRUE(manager_->GetActiveIMEState()->ReplaceEnabledInputMethods(ids));
  EXPECT_EQ(3U, manager_->GetActiveIMEState()->GetNumActiveInputMethods());
  EXPECT_EQ(1, observer.input_method_changed_count_);
  EXPECT_EQ(ImeIdFromEngineId(ids[0]),
            manager_->GetActiveIMEState()->GetCurrentInputMethod().id());
  EXPECT_EQ("us", keyboard_->last_layout_);
  // Switch to Dvorak.
  manager_->GetActiveIMEState()->SwitchToNextInputMethod();
  EXPECT_EQ(2, observer.input_method_changed_count_);
  EXPECT_EQ(ImeIdFromEngineId(ids[1]),
            manager_->GetActiveIMEState()->GetCurrentInputMethod().id());
  EXPECT_EQ("us(dvorak)", keyboard_->last_layout_);
  // Disable Dvorak.
  ids.erase(ids.begin() + 1);
  EXPECT_TRUE(manager_->GetActiveIMEState()->ReplaceEnabledInputMethods(ids));
  EXPECT_EQ(2U, manager_->GetActiveIMEState()->GetNumActiveInputMethods());
  EXPECT_EQ(3, observer.input_method_changed_count_);
  EXPECT_EQ(ImeIdFromEngineId(ids[0]),  // US Qwerty
            manager_->GetActiveIMEState()->GetCurrentInputMethod().id());
  EXPECT_EQ("us", keyboard_->last_layout_);
  manager_->RemoveObserver(&observer);
}

TEST_F(InputMethodManagerImplTest, TestEnableLayoutAndIme) {
  // For http://crbug.com/19655#c11 - (10).
  TestObserver observer;
  InitComponentExtension();
  manager_->AddObserver(&observer);
  manager_->SetUISessionState(InputMethodManager::STATE_BROWSER_SCREEN);
  std::vector<std::string> ids;
  ids.push_back(ImeIdFromEngineId("xkb:us:dvorak:eng"));
  ids.push_back(ImeIdFromEngineId(kNaclMozcUsId));
  EXPECT_TRUE(manager_->GetActiveIMEState()->ReplaceEnabledInputMethods(ids));
  EXPECT_EQ(1, observer.input_method_changed_count_);
  EXPECT_EQ(ImeIdFromEngineId(ids[0]),
            manager_->GetActiveIMEState()->GetCurrentInputMethod().id());
  EXPECT_EQ("us(dvorak)", keyboard_->last_layout_);
  // Switch to Mozc
  manager_->GetActiveIMEState()->SwitchToNextInputMethod();
  EXPECT_EQ(2, observer.input_method_changed_count_);
  EXPECT_EQ(ImeIdFromEngineId(ids[1]),
            manager_->GetActiveIMEState()->GetCurrentInputMethod().id());
  EXPECT_EQ("us", keyboard_->last_layout_);
  // Disable Mozc.
  ids.erase(ids.begin() + 1);
  EXPECT_TRUE(manager_->GetActiveIMEState()->ReplaceEnabledInputMethods(ids));
  EXPECT_EQ(1U, manager_->GetActiveIMEState()->GetNumActiveInputMethods());
  EXPECT_EQ(ImeIdFromEngineId(ids[0]),
            manager_->GetActiveIMEState()->GetCurrentInputMethod().id());
  EXPECT_EQ("us(dvorak)", keyboard_->last_layout_);
}

TEST_F(InputMethodManagerImplTest, TestEnableLayoutAndIme2) {
  // For http://crbug.com/19655#c11 - (11).
  TestObserver observer;
  InitComponentExtension();
  manager_->AddObserver(&observer);
  manager_->SetUISessionState(InputMethodManager::STATE_BROWSER_SCREEN);
  std::vector<std::string> ids;
  ids.push_back(ImeIdFromEngineId("xkb:us:dvorak:eng"));
  ids.push_back(ImeIdFromEngineId(kNaclMozcUsId));
  EXPECT_TRUE(manager_->GetActiveIMEState()->ReplaceEnabledInputMethods(ids));
  EXPECT_EQ(1, observer.input_method_changed_count_);
  EXPECT_EQ(ImeIdFromEngineId(ids[0]),
            manager_->GetActiveIMEState()->GetCurrentInputMethod().id());
  EXPECT_EQ("us(dvorak)", keyboard_->last_layout_);

  // Disable Dvorak.
  ids.erase(ids.begin());
  EXPECT_TRUE(manager_->GetActiveIMEState()->ReplaceEnabledInputMethods(ids));
  EXPECT_EQ(1U, manager_->GetActiveIMEState()->GetNumActiveInputMethods());
  EXPECT_EQ(ImeIdFromEngineId(ids[0]),  // Mozc
            manager_->GetActiveIMEState()->GetCurrentInputMethod().id());
  EXPECT_EQ("us", keyboard_->last_layout_);
  manager_->RemoveObserver(&observer);
}

TEST_F(InputMethodManagerImplTest, TestEnableImes) {
  TestObserver observer;
  InitComponentExtension();
  manager_->AddObserver(&observer);
  manager_->SetUISessionState(InputMethodManager::STATE_BROWSER_SCREEN);
  std::vector<std::string> ids;
  ids.push_back(ImeIdFromEngineId(kExt2Engine1Id));
  ids.emplace_back("mozc-dv");
  EXPECT_TRUE(manager_->GetActiveIMEState()->ReplaceEnabledInputMethods(ids));
  EXPECT_EQ(1, observer.input_method_changed_count_);
  EXPECT_EQ(ImeIdFromEngineId(ids[0]),
            manager_->GetActiveIMEState()->GetCurrentInputMethod().id());
  EXPECT_EQ("us", keyboard_->last_layout_);
  manager_->RemoveObserver(&observer);
}

TEST_F(InputMethodManagerImplTest, TestEnableUnknownIds) {
  TestObserver observer;
  InitComponentExtension();
  manager_->AddObserver(&observer);
  manager_->SetUISessionState(InputMethodManager::STATE_BROWSER_SCREEN);
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
  InitComponentExtension();
  manager_->AddObserver(&observer);
  manager_->SetUISessionState(InputMethodManager::STATE_BROWSER_SCREEN);
  std::vector<std::string> ids;
  ids.push_back(ImeIdFromEngineId("xkb:us::eng"));
  ids.push_back(ImeIdFromEngineId("xkb:us:dvorak:eng"));
  EXPECT_TRUE(manager_->GetActiveIMEState()->ReplaceEnabledInputMethods(ids));
  EXPECT_EQ(2U, manager_->GetActiveIMEState()->GetNumActiveInputMethods());
  EXPECT_EQ(1, observer.input_method_changed_count_);
  EXPECT_EQ(ImeIdFromEngineId(ids[0]),
            manager_->GetActiveIMEState()->GetCurrentInputMethod().id());
  EXPECT_EQ("us", keyboard_->last_layout_);

  // Switch to Dvorak.
  manager_->GetActiveIMEState()->SwitchToNextInputMethod();
  EXPECT_EQ(2, observer.input_method_changed_count_);
  EXPECT_EQ(ImeIdFromEngineId(ids[1]),
            manager_->GetActiveIMEState()->GetCurrentInputMethod().id());
  EXPECT_EQ("us(dvorak)", keyboard_->last_layout_);

  // Lock screen
  scoped_refptr<input_method::InputMethodManager::State> saved_ime_state =
      manager_->GetActiveIMEState();
  manager_->SetState(saved_ime_state->Clone());
  manager_->GetActiveIMEState()->EnableLockScreenLayouts();
  manager_->SetUISessionState(InputMethodManager::STATE_LOCK_SCREEN);
  EXPECT_EQ(2U, manager_->GetActiveIMEState()->GetNumActiveInputMethods());
  EXPECT_EQ(ImeIdFromEngineId(ids[1]),  // still Dvorak
            manager_->GetActiveIMEState()->GetCurrentInputMethod().id());
  EXPECT_EQ("us(dvorak)", keyboard_->last_layout_);
  // Switch back to Qwerty.
  manager_->GetActiveIMEState()->SwitchToNextInputMethod();
  EXPECT_EQ(ImeIdFromEngineId(ids[0]),
            manager_->GetActiveIMEState()->GetCurrentInputMethod().id());
  EXPECT_EQ("us", keyboard_->last_layout_);

  // Unlock screen. The original state, Dvorak, is restored.
  manager_->SetState(saved_ime_state);
  manager_->SetUISessionState(InputMethodManager::STATE_BROWSER_SCREEN);
  EXPECT_EQ(2U, manager_->GetActiveIMEState()->GetNumActiveInputMethods());
  EXPECT_EQ(ImeIdFromEngineId(ids[1]),
            manager_->GetActiveIMEState()->GetCurrentInputMethod().id());
  EXPECT_EQ("us(dvorak)", keyboard_->last_layout_);

  manager_->RemoveObserver(&observer);
}

TEST_F(InputMethodManagerImplTest, SwitchInputMethodTest) {
  // For http://crbug.com/19655#c11 - (15).
  TestObserver observer;
  InitComponentExtension();
  manager_->AddObserver(&observer);
  manager_->SetUISessionState(InputMethodManager::STATE_BROWSER_SCREEN);
  std::vector<std::string> ids;
  ids.push_back(ImeIdFromEngineId("xkb:us:dvorak:eng"));
  ids.push_back(ImeIdFromEngineId(kExt2Engine2Id));
  ids.push_back(ImeIdFromEngineId(kExt2Engine1Id));
  EXPECT_TRUE(manager_->GetActiveIMEState()->ReplaceEnabledInputMethods(ids));
  EXPECT_EQ(3U, manager_->GetActiveIMEState()->GetNumActiveInputMethods());
  EXPECT_EQ(1, observer.input_method_changed_count_);
  EXPECT_EQ(ImeIdFromEngineId(ids[0]),
            manager_->GetActiveIMEState()->GetCurrentInputMethod().id());
  EXPECT_EQ("us(dvorak)", keyboard_->last_layout_);

  // Switch to Mozc.
  manager_->GetActiveIMEState()->SwitchToNextInputMethod();
  EXPECT_EQ(2, observer.input_method_changed_count_);
  EXPECT_EQ(ImeIdFromEngineId(ids[1]),
            manager_->GetActiveIMEState()->GetCurrentInputMethod().id());
  EXPECT_EQ("us(dvorak)", keyboard_->last_layout_);

  // Lock screen
  scoped_refptr<input_method::InputMethodManager::State> saved_ime_state =
      manager_->GetActiveIMEState();
  manager_->SetState(saved_ime_state->Clone());
  manager_->GetActiveIMEState()->EnableLockScreenLayouts();
  manager_->SetUISessionState(InputMethodManager::STATE_LOCK_SCREEN);
  EXPECT_EQ(2U,
            manager_->GetActiveIMEState()
                ->GetNumActiveInputMethods());  // Qwerty+Dvorak.
  EXPECT_EQ(ImeIdFromEngineId("xkb:us:dvorak:eng"),
            manager_->GetActiveIMEState()->GetCurrentInputMethod().id());
  EXPECT_EQ("us(dvorak)", keyboard_->last_layout_);
  manager_->GetActiveIMEState()->SwitchToNextInputMethod();
  EXPECT_EQ(ImeIdFromEngineId("xkb:us::eng"),  // The hardware keyboard layout.
            manager_->GetActiveIMEState()->GetCurrentInputMethod().id());
  EXPECT_EQ("us", keyboard_->last_layout_);

  // Unlock screen. The original state, pinyin-dv, is restored.
  manager_->SetState(saved_ime_state);
  manager_->SetUISessionState(InputMethodManager::STATE_BROWSER_SCREEN);
  EXPECT_EQ(3U,
            manager_->GetActiveIMEState()
                ->GetNumActiveInputMethods());  // Dvorak and 2 IMEs.
  EXPECT_EQ(ImeIdFromEngineId(ids[1]),
            manager_->GetActiveIMEState()->GetCurrentInputMethod().id());
  EXPECT_EQ("us(dvorak)", keyboard_->last_layout_);

  manager_->RemoveObserver(&observer);
}

TEST_F(InputMethodManagerImplTest, TestXkbSetting) {
  EXPECT_EQ(0, keyboard_->set_current_keyboard_layout_by_name_count_);
  // For http://crbug.com/19655#c11 - (8), step 7-11.
  InitComponentExtension();
  EXPECT_EQ(1, keyboard_->set_current_keyboard_layout_by_name_count_);
  manager_->SetUISessionState(InputMethodManager::STATE_BROWSER_SCREEN);
  std::vector<std::string> ids;
  ids.push_back(ImeIdFromEngineId("xkb:us:dvorak:eng"));
  ids.push_back(ImeIdFromEngineId("xkb:us:colemak:eng"));
  ids.push_back(ImeIdFromEngineId(kNaclMozcJpId));
  ids.push_back(ImeIdFromEngineId(kNaclMozcUsId));
  EXPECT_TRUE(manager_->GetActiveIMEState()->ReplaceEnabledInputMethods(ids));
  EXPECT_EQ(4U, manager_->GetActiveIMEState()->GetNumActiveInputMethods());
  EXPECT_EQ(2, keyboard_->set_current_keyboard_layout_by_name_count_);
  // See input_methods.txt for an expected XKB layout name.
  EXPECT_EQ("us(dvorak)", keyboard_->last_layout_);
  manager_->GetActiveIMEState()->SwitchToNextInputMethod();
  EXPECT_EQ(3, keyboard_->set_current_keyboard_layout_by_name_count_);
  EXPECT_EQ("us(colemak)", keyboard_->last_layout_);
  manager_->GetActiveIMEState()->SwitchToNextInputMethod();
  EXPECT_EQ(4, keyboard_->set_current_keyboard_layout_by_name_count_);
  EXPECT_EQ("jp", keyboard_->last_layout_);
  manager_->GetActiveIMEState()->SwitchToNextInputMethod();
  EXPECT_EQ(5, keyboard_->set_current_keyboard_layout_by_name_count_);
  EXPECT_EQ("us", keyboard_->last_layout_);
  manager_->GetActiveIMEState()->SwitchToNextInputMethod();
  EXPECT_EQ(6, keyboard_->set_current_keyboard_layout_by_name_count_);
  EXPECT_EQ("us(dvorak)", keyboard_->last_layout_);
  // Disable Dvorak.
  ids.erase(ids.begin());
  EXPECT_TRUE(manager_->GetActiveIMEState()->ReplaceEnabledInputMethods(ids));
  EXPECT_EQ(3U, manager_->GetActiveIMEState()->GetNumActiveInputMethods());
  EXPECT_EQ(7, keyboard_->set_current_keyboard_layout_by_name_count_);
  EXPECT_EQ("us(colemak)", keyboard_->last_layout_);
}

TEST_F(InputMethodManagerImplTest, TestActivateInputMethodMenuItem) {
  const std::string kKey = "key";
  ui::ime::InputMethodMenuItemList menu_list;
  menu_list.push_back(ui::ime::InputMethodMenuItem(
      kKey, "label", false, false));
  menu_manager_->SetCurrentInputMethodMenuItemList(menu_list);

  manager_->ActivateInputMethodMenuItem(kKey);
  EXPECT_EQ(kKey, mock_engine_handler_->last_activated_property());

  // Key2 is not registered, so activated property should not be changed.
  manager_->ActivateInputMethodMenuItem("key2");
  EXPECT_EQ(kKey, mock_engine_handler_->last_activated_property());
}

TEST_F(InputMethodManagerImplTest, TestGetCurrentInputMethodProperties) {
  InitComponentExtension();
  EXPECT_TRUE(menu_manager_->GetCurrentInputMethodMenuItemList().empty());

  manager_->SetUISessionState(InputMethodManager::STATE_BROWSER_SCREEN);
  std::vector<std::string> ids;
  ids.push_back(ImeIdFromEngineId("xkb:us::eng"));
  ids.push_back(ImeIdFromEngineId(kNaclMozcUsId));
  EXPECT_TRUE(manager_->GetActiveIMEState()->ReplaceEnabledInputMethods(ids));
  EXPECT_EQ(2U, manager_->GetActiveIMEState()->GetNumActiveInputMethods());
  EXPECT_TRUE(menu_manager_->GetCurrentInputMethodMenuItemList().empty());
  manager_->GetActiveIMEState()->ChangeInputMethod(
      ImeIdFromEngineId(kNaclMozcUsId), false /* show_message */);

  ui::ime::InputMethodMenuItemList current_property_list;
  current_property_list.push_back(ui::ime::InputMethodMenuItem(
      "key", "label", false, false));
  menu_manager_->SetCurrentInputMethodMenuItemList(current_property_list);

  ASSERT_EQ(1U, menu_manager_->GetCurrentInputMethodMenuItemList().size());
  EXPECT_EQ("key",
            menu_manager_->GetCurrentInputMethodMenuItemList().at(0).key);

  manager_->GetActiveIMEState()->ChangeInputMethod("xkb:us::eng",
                                                   false /* show_message */);
  EXPECT_TRUE(menu_manager_->GetCurrentInputMethodMenuItemList().empty());
}

TEST_F(InputMethodManagerImplTest, TestGetCurrentInputMethodPropertiesTwoImes) {
  InitComponentExtension();
  EXPECT_TRUE(menu_manager_->GetCurrentInputMethodMenuItemList().empty());

  manager_->SetUISessionState(InputMethodManager::STATE_BROWSER_SCREEN);
  std::vector<std::string> ids;
  ids.push_back(ImeIdFromEngineId(kNaclMozcUsId));   // Japanese
  ids.push_back(ImeIdFromEngineId(kExt2Engine1Id));  // T-Chinese
  EXPECT_TRUE(manager_->GetActiveIMEState()->ReplaceEnabledInputMethods(ids));
  EXPECT_EQ(2U, manager_->GetActiveIMEState()->GetNumActiveInputMethods());
  EXPECT_TRUE(menu_manager_->GetCurrentInputMethodMenuItemList().empty());

  ui::ime::InputMethodMenuItemList current_property_list;
  current_property_list.push_back(ui::ime::InputMethodMenuItem("key-mozc",
                                                                "label",
                                                                false,
                                                                false));
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
  current_property_list.push_back(ui::ime::InputMethodMenuItem(
      "key-chewing", "label", false, false));
  menu_manager_->SetCurrentInputMethodMenuItemList(current_property_list);
  ASSERT_EQ(1U, menu_manager_->GetCurrentInputMethodMenuItemList().size());
  EXPECT_EQ("key-chewing",
            menu_manager_->GetCurrentInputMethodMenuItemList().at(0).key);
}

TEST_F(InputMethodManagerImplTest, TestNextInputMethod) {
  TestObserver observer;
  InitComponentExtension();
  manager_->AddObserver(&observer);
  std::vector<std::string> keyboard_layouts;
  keyboard_layouts.push_back(ImeIdFromEngineId("xkb:us::eng"));
  // For http://crbug.com/19655#c11 - (1)
  manager_->GetActiveIMEState()->EnableLoginLayouts("en-US", keyboard_layouts);
  EXPECT_EQ(8U, manager_->GetActiveIMEState()->GetNumActiveInputMethods());
  EXPECT_EQ(ImeIdFromEngineId("xkb:us::eng"),
            manager_->GetActiveIMEState()->GetCurrentInputMethod().id());
  EXPECT_EQ("us", keyboard_->last_layout_);
  manager_->GetActiveIMEState()->SwitchToNextInputMethod();
  EXPECT_TRUE(observer.last_show_message_);
  EXPECT_EQ(ImeIdFromEngineId("xkb:us:intl:eng"),
            manager_->GetActiveIMEState()->GetCurrentInputMethod().id());
  EXPECT_EQ("us(intl)", keyboard_->last_layout_);
  manager_->GetActiveIMEState()->SwitchToNextInputMethod();
  EXPECT_TRUE(observer.last_show_message_);
  EXPECT_EQ(ImeIdFromEngineId("xkb:us:altgr-intl:eng"),
            manager_->GetActiveIMEState()->GetCurrentInputMethod().id());
  EXPECT_EQ("us(altgr-intl)", keyboard_->last_layout_);
  manager_->GetActiveIMEState()->SwitchToNextInputMethod();
  EXPECT_TRUE(observer.last_show_message_);
  EXPECT_EQ(ImeIdFromEngineId("xkb:us:dvorak:eng"),
            manager_->GetActiveIMEState()->GetCurrentInputMethod().id());
  EXPECT_EQ("us(dvorak)", keyboard_->last_layout_);
  manager_->GetActiveIMEState()->SwitchToNextInputMethod();
  EXPECT_TRUE(observer.last_show_message_);
  EXPECT_EQ(ImeIdFromEngineId("xkb:us:dvp:eng"),
            manager_->GetActiveIMEState()->GetCurrentInputMethod().id());
  EXPECT_EQ("us(dvp)", keyboard_->last_layout_);
  manager_->GetActiveIMEState()->SwitchToNextInputMethod();
  EXPECT_TRUE(observer.last_show_message_);
  EXPECT_EQ(ImeIdFromEngineId("xkb:us:colemak:eng"),
            manager_->GetActiveIMEState()->GetCurrentInputMethod().id());
  EXPECT_EQ("us(colemak)", keyboard_->last_layout_);
  manager_->GetActiveIMEState()->SwitchToNextInputMethod();
  EXPECT_TRUE(observer.last_show_message_);
  EXPECT_EQ(ImeIdFromEngineId("xkb:us:workman:eng"),
            manager_->GetActiveIMEState()->GetCurrentInputMethod().id());
  EXPECT_EQ("us(workman)", keyboard_->last_layout_);
  manager_->GetActiveIMEState()->SwitchToNextInputMethod();
  EXPECT_TRUE(observer.last_show_message_);
  EXPECT_EQ(ImeIdFromEngineId("xkb:us:workman-intl:eng"),
            manager_->GetActiveIMEState()->GetCurrentInputMethod().id());
  EXPECT_EQ("us(workman-intl)", keyboard_->last_layout_);
  manager_->GetActiveIMEState()->SwitchToNextInputMethod();
  EXPECT_TRUE(observer.last_show_message_);
  EXPECT_EQ(ImeIdFromEngineId("xkb:us::eng"),
            manager_->GetActiveIMEState()->GetCurrentInputMethod().id());
  EXPECT_EQ("us", keyboard_->last_layout_);

  manager_->RemoveObserver(&observer);
}

TEST_F(InputMethodManagerImplTest, TestLastUsedInputMethod) {
  TestObserver observer;
  InitComponentExtension();
  manager_->AddObserver(&observer);

  std::vector<std::string> keyboard_layouts;
  keyboard_layouts.push_back(ImeIdFromEngineId("xkb:us::eng"));
  manager_->GetActiveIMEState()->EnableLoginLayouts("en-US", keyboard_layouts);
  EXPECT_EQ(8U, manager_->GetActiveIMEState()->GetNumActiveInputMethods());
  EXPECT_EQ(ImeIdFromEngineId("xkb:us::eng"),
            manager_->GetActiveIMEState()->GetCurrentInputMethod().id());
  EXPECT_EQ("us", keyboard_->last_layout_);
  manager_->GetActiveIMEState()->SwitchToNextInputMethod();
  EXPECT_TRUE(observer.last_show_message_);
  EXPECT_EQ(ImeIdFromEngineId("xkb:us:intl:eng"),
            manager_->GetActiveIMEState()->GetCurrentInputMethod().id());
  EXPECT_EQ("us(intl)", keyboard_->last_layout_);
  manager_->GetActiveIMEState()->SwitchToLastUsedInputMethod();
  EXPECT_TRUE(observer.last_show_message_);
  EXPECT_EQ(ImeIdFromEngineId("xkb:us::eng"),
            manager_->GetActiveIMEState()->GetCurrentInputMethod().id());
  EXPECT_EQ("us", keyboard_->last_layout_);
  manager_->GetActiveIMEState()->SwitchToLastUsedInputMethod();
  EXPECT_TRUE(observer.last_show_message_);
  EXPECT_EQ(ImeIdFromEngineId("xkb:us:intl:eng"),
            manager_->GetActiveIMEState()->GetCurrentInputMethod().id());
  EXPECT_EQ("us(intl)", keyboard_->last_layout_);
  manager_->GetActiveIMEState()->SwitchToLastUsedInputMethod();
  EXPECT_TRUE(observer.last_show_message_);
  EXPECT_EQ(ImeIdFromEngineId("xkb:us::eng"),
            manager_->GetActiveIMEState()->GetCurrentInputMethod().id());
  EXPECT_EQ("us", keyboard_->last_layout_);
  manager_->GetActiveIMEState()->SwitchToNextInputMethod();
  EXPECT_TRUE(observer.last_show_message_);
  EXPECT_EQ(ImeIdFromEngineId("xkb:us:intl:eng"),
            manager_->GetActiveIMEState()->GetCurrentInputMethod().id());
  EXPECT_EQ("us(intl)", keyboard_->last_layout_);
  manager_->GetActiveIMEState()->SwitchToNextInputMethod();
  EXPECT_TRUE(observer.last_show_message_);
  EXPECT_EQ(ImeIdFromEngineId("xkb:us:altgr-intl:eng"),
            manager_->GetActiveIMEState()->GetCurrentInputMethod().id());
  EXPECT_EQ("us(altgr-intl)", keyboard_->last_layout_);
  manager_->GetActiveIMEState()->SwitchToLastUsedInputMethod();
  EXPECT_TRUE(observer.last_show_message_);
  EXPECT_EQ(ImeIdFromEngineId("xkb:us:intl:eng"),
            manager_->GetActiveIMEState()->GetCurrentInputMethod().id());
  EXPECT_EQ("us(intl)", keyboard_->last_layout_);
  manager_->GetActiveIMEState()->SwitchToLastUsedInputMethod();
  EXPECT_TRUE(observer.last_show_message_);
  EXPECT_EQ(ImeIdFromEngineId("xkb:us:altgr-intl:eng"),
            manager_->GetActiveIMEState()->GetCurrentInputMethod().id());
  EXPECT_EQ("us(altgr-intl)", keyboard_->last_layout_);

  manager_->RemoveObserver(&observer);
}

TEST_F(InputMethodManagerImplTest, CycleInputMethodForOneActiveInputMethod) {
  InitComponentExtension();

  // Simulate a single input method.
  std::vector<std::string> ids;
  ids.push_back(ImeIdFromEngineId("xkb:us::eng"));
  EXPECT_TRUE(manager_->GetActiveIMEState()->ReplaceEnabledInputMethods(ids));
  EXPECT_EQ(1U, manager_->GetActiveIMEState()->GetNumActiveInputMethods());

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
  InitComponentExtension();
  manager_->AddObserver(&observer);
  manager_->SetUISessionState(InputMethodManager::STATE_BROWSER_SCREEN);
  std::vector<std::string> ids;
  ids.push_back(ImeIdFromEngineId("xkb:us:dvorak:eng"));
  EXPECT_TRUE(manager_->GetActiveIMEState()->ReplaceEnabledInputMethods(ids));
  EXPECT_EQ(1U, manager_->GetActiveIMEState()->GetNumActiveInputMethods());
  EXPECT_EQ(1, observer.input_method_changed_count_);
  EXPECT_EQ(ImeIdFromEngineId(ids[0]),
            manager_->GetActiveIMEState()->GetCurrentInputMethod().id());
  EXPECT_EQ("us(dvorak)", keyboard_->last_layout_);

  // Add two Extension IMEs.
  std::vector<std::string> layouts;
  layouts.emplace_back("us");
  std::vector<std::string> languages;
  languages.emplace_back("en-US");

  const std::string ext1_id =
      extension_ime_util::GetInputMethodID(kExtensionId1, "engine_id");
  const InputMethodDescriptor descriptor1(ext1_id,
                                          "deadbeef input method",
                                          "DB",
                                          layouts,
                                          languages,
                                          false,  // is_login_keyboard
                                          GURL(),
                                          GURL());
  MockInputMethodEngine engine;
  InputMethodDescriptors descriptors;
  descriptors.push_back(descriptor1);
  manager_->GetActiveIMEState()->AddInputMethodExtension(
      kExtensionId1, descriptors, &engine);

  // Extension IMEs are not enabled by default.
  EXPECT_EQ(1U, manager_->GetActiveIMEState()->GetNumActiveInputMethods());

  std::vector<std::string> extension_ime_ids;
  extension_ime_ids.push_back(ext1_id);
  manager_->GetActiveIMEState()->SetEnabledExtensionImes(&extension_ime_ids);
  EXPECT_EQ(2U, manager_->GetActiveIMEState()->GetNumActiveInputMethods());

  {
    std::unique_ptr<InputMethodDescriptors> methods(
        manager_->GetActiveIMEState()->GetActiveInputMethods());
    ASSERT_EQ(2U, methods->size());
    // Ext IMEs should be at the end of the list.
    EXPECT_EQ(ext1_id, methods->at(1).id());
  }

  const std::string ext2_id =
      extension_ime_util::GetInputMethodID(kExtensionId2, "engine_id");
  const InputMethodDescriptor descriptor2(ext2_id,
                                          "cafebabe input method",
                                          "CB",
                                          layouts,
                                          languages,
                                          false,  // is_login_keyboard
                                          GURL(),
                                          GURL());
  descriptors.clear();
  descriptors.push_back(descriptor2);
  MockInputMethodEngine engine2;
  manager_->GetActiveIMEState()->AddInputMethodExtension(
      kExtensionId2, descriptors, &engine2);
  EXPECT_EQ(2U, manager_->GetActiveIMEState()->GetNumActiveInputMethods());

  extension_ime_ids.push_back(ext2_id);
  manager_->GetActiveIMEState()->SetEnabledExtensionImes(&extension_ime_ids);
  EXPECT_EQ(3U, manager_->GetActiveIMEState()->GetNumActiveInputMethods());
  {
    std::unique_ptr<InputMethodDescriptors> methods(
        manager_->GetActiveIMEState()->GetActiveInputMethods());
    ASSERT_EQ(3U, methods->size());
    // Ext IMEs should be at the end of the list.
    EXPECT_EQ(ext1_id, methods->at(1).id());
    EXPECT_EQ(ext2_id, methods->at(2).id());
  }

  // Remove them.
  manager_->GetActiveIMEState()->RemoveInputMethodExtension(kExtensionId1);
  EXPECT_EQ(2U, manager_->GetActiveIMEState()->GetNumActiveInputMethods());
  manager_->GetActiveIMEState()->RemoveInputMethodExtension(kExtensionId2);
  EXPECT_EQ(1U, manager_->GetActiveIMEState()->GetNumActiveInputMethods());
}

TEST_F(InputMethodManagerImplTest, TestAddExtensionInputThenLockScreen) {
  TestObserver observer;
  InitComponentExtension();
  manager_->AddObserver(&observer);
  manager_->SetUISessionState(InputMethodManager::STATE_BROWSER_SCREEN);
  std::vector<std::string> ids;
  ids.push_back(ImeIdFromEngineId("xkb:us::eng"));
  EXPECT_TRUE(manager_->GetActiveIMEState()->ReplaceEnabledInputMethods(ids));
  EXPECT_EQ(1U, manager_->GetActiveIMEState()->GetNumActiveInputMethods());
  EXPECT_EQ(1, observer.input_method_changed_count_);
  EXPECT_EQ(ImeIdFromEngineId(ids[0]),
            manager_->GetActiveIMEState()->GetCurrentInputMethod().id());
  EXPECT_EQ("us", keyboard_->last_layout_);

  // Add an Extension IME.
  std::vector<std::string> layouts;
  layouts.emplace_back("us(dvorak)");
  std::vector<std::string> languages;
  languages.emplace_back("en-US");

  const std::string ext_id =
      extension_ime_util::GetInputMethodID(kExtensionId1, "engine_id");
  const InputMethodDescriptor descriptor(ext_id,
                                         "deadbeef input method",
                                         "DB",
                                         layouts,
                                         languages,
                                         false,  // is_login_keyboard
                                         GURL(),
                                         GURL());
  MockInputMethodEngine engine;
  InputMethodDescriptors descriptors;
  descriptors.push_back(descriptor);
  manager_->GetActiveIMEState()->AddInputMethodExtension(
      kExtensionId1, descriptors, &engine);

  // Extension IME is not enabled by default.
  EXPECT_EQ(1U, manager_->GetActiveIMEState()->GetNumActiveInputMethods());
  EXPECT_EQ(1, observer.input_method_changed_count_);

  std::vector<std::string> extension_ime_ids;
  extension_ime_ids.push_back(ext_id);
  manager_->GetActiveIMEState()->SetEnabledExtensionImes(&extension_ime_ids);
  EXPECT_EQ(2U, manager_->GetActiveIMEState()->GetNumActiveInputMethods());

  // Switch to the IME.
  manager_->GetActiveIMEState()->SwitchToNextInputMethod();
  EXPECT_EQ(3, observer.input_method_changed_count_);
  EXPECT_EQ(ext_id,
            manager_->GetActiveIMEState()->GetCurrentInputMethod().id());
  EXPECT_EQ("us(dvorak)", keyboard_->last_layout_);

  // Lock the screen. This is for crosbug.com/27049.
  scoped_refptr<input_method::InputMethodManager::State> saved_ime_state =
      manager_->GetActiveIMEState();
  manager_->SetState(saved_ime_state->Clone());
  manager_->GetActiveIMEState()->EnableLockScreenLayouts();
  manager_->SetUISessionState(InputMethodManager::STATE_LOCK_SCREEN);
  EXPECT_EQ(1U,
            manager_->GetActiveIMEState()
                ->GetNumActiveInputMethods());  // Qwerty. No Ext. IME
  EXPECT_EQ(ImeIdFromEngineId("xkb:us::eng"),
            manager_->GetActiveIMEState()->GetCurrentInputMethod().id());
  EXPECT_EQ("us", keyboard_->last_layout_);

  // Unlock the screen.
  manager_->SetState(saved_ime_state);
  manager_->SetUISessionState(InputMethodManager::STATE_BROWSER_SCREEN);
  EXPECT_EQ(2U, manager_->GetActiveIMEState()->GetNumActiveInputMethods());
  EXPECT_EQ(ext_id,
            manager_->GetActiveIMEState()->GetCurrentInputMethod().id());
  EXPECT_EQ("us(dvorak)", keyboard_->last_layout_);
  {
    // This is for crosbug.com/27052.
    std::unique_ptr<InputMethodDescriptors> methods(
        manager_->GetActiveIMEState()->GetActiveInputMethods());
    ASSERT_EQ(2U, methods->size());
    // Ext. IMEs should be at the end of the list.
    EXPECT_EQ(ext_id, methods->at(1).id());
  }
  manager_->RemoveObserver(&observer);
}

TEST_F(InputMethodManagerImplTest,
       ChangeInputMethod_ComponenteExtensionOneIME) {
  InitComponentExtension();
  manager_->SetUISessionState(InputMethodManager::STATE_BROWSER_SCREEN);
  const std::string ext_id = extension_ime_util::GetComponentInputMethodID(
      ime_list_[1].id,
      ime_list_[1].engines[0].engine_id);
  std::vector<std::string> ids;
  ids.push_back(ext_id);
  EXPECT_TRUE(manager_->GetActiveIMEState()->ReplaceEnabledInputMethods(ids));
  EXPECT_EQ(1U, manager_->GetActiveIMEState()->GetNumActiveInputMethods());
  EXPECT_EQ(ext_id,
            manager_->GetActiveIMEState()->GetCurrentInputMethod().id());
}

TEST_F(InputMethodManagerImplTest,
       ChangeInputMethod_ComponenteExtensionTwoIME) {
  InitComponentExtension();
  manager_->SetUISessionState(InputMethodManager::STATE_BROWSER_SCREEN);
  const std::string ext_id1 = extension_ime_util::GetComponentInputMethodID(
      ime_list_[1].id,
      ime_list_[1].engines[0].engine_id);
  const std::string ext_id2 = extension_ime_util::GetComponentInputMethodID(
      ime_list_[2].id,
      ime_list_[2].engines[0].engine_id);
  std::vector<std::string> ids;
  ids.push_back(ext_id1);
  ids.push_back(ext_id2);
  EXPECT_TRUE(manager_->GetActiveIMEState()->ReplaceEnabledInputMethods(ids));
  EXPECT_EQ(2U, manager_->GetActiveIMEState()->GetNumActiveInputMethods());
  EXPECT_EQ(ext_id1,
            manager_->GetActiveIMEState()->GetCurrentInputMethod().id());
  manager_->GetActiveIMEState()->ChangeInputMethod(ext_id2,
                                                   false /* show_message */);
  EXPECT_EQ(ext_id2,
            manager_->GetActiveIMEState()->GetCurrentInputMethod().id());
}

TEST_F(InputMethodManagerImplTest, MigrateInputMethodTest) {
  std::vector<std::string> input_method_ids;
  input_method_ids.emplace_back("xkb:us::eng");
  input_method_ids.emplace_back("xkb:fr::fra");
  input_method_ids.push_back(ImeIdFromEngineId("xkb:us::eng"));
  input_method_ids.emplace_back("xkb:fr::fra");
  input_method_ids.push_back(ImeIdFromEngineId("xkb:us::eng"));
  input_method_ids.emplace_back("_comp_ime_asdf_pinyin");
  input_method_ids.push_back(ImeIdFromEngineId(kPinyinImeId));

  manager_->MigrateInputMethods(&input_method_ids);

  ASSERT_EQ(4U, input_method_ids.size());

  EXPECT_EQ(ImeIdFromEngineId("xkb:us::eng"), input_method_ids[0]);
  EXPECT_EQ(ImeIdFromEngineId("xkb:fr::fra"), input_method_ids[1]);
  EXPECT_EQ("_comp_ime_asdf_pinyin", input_method_ids[2]);
  EXPECT_EQ(ImeIdFromEngineId("zh-t-i0-pinyin"), input_method_ids[3]);
}

TEST_F(InputMethodManagerImplTest, OverrideKeyboardUrlRefWithKeyset) {
  InitComponentExtension();
  const GURL inputview_url(
      "chrome-extension://"
      "inputview.html#id=us.compact.qwerty&language=en-US&passwordLayout=us."
      "compact.qwerty&name=keyboard_us");
  GetActiveIMEState()->input_view_url = inputview_url;
  EXPECT_EQ(inputview_url, GetActiveIMEState()->GetInputViewUrl());

  // Override the keyboard url ref with 'emoji'.
  const GURL overridden_url_emoji(
      "chrome-extension://"
      "inputview.html#id=us.compact.qwerty.emoji&language=en-US&passwordLayout="
      "us.compact.qwerty&name=keyboard_us");
  manager_->OverrideKeyboardKeyset(mojom::ImeKeyset::kEmoji);
  EXPECT_EQ(overridden_url_emoji, GetActiveIMEState()->GetInputViewUrl());

  // Override the keyboard url ref with 'hwt'.
  GetActiveIMEState()->input_view_url = inputview_url;
  const GURL overridden_url_hwt(
      "chrome-extension://"
      "inputview.html#id=us.compact.qwerty.hwt&language=en-US&passwordLayout="
      "us.compact.qwerty&name=keyboard_us");
  manager_->OverrideKeyboardKeyset(mojom::ImeKeyset::kHandwriting);
  EXPECT_EQ(overridden_url_hwt, GetActiveIMEState()->GetInputViewUrl());

  // Override the keyboard url ref with 'voice'.
  GetActiveIMEState()->input_view_url = inputview_url;
  const GURL overridden_url_voice(
      "chrome-extension://"
      "inputview.html#id=us.compact.qwerty.voice&language=en-US"
      "&passwordLayout=us.compact.qwerty&name=keyboard_us");
  manager_->OverrideKeyboardKeyset(mojom::ImeKeyset::kVoice);
  EXPECT_EQ(overridden_url_voice, GetActiveIMEState()->GetInputViewUrl());
}

TEST_F(InputMethodManagerImplTest, OverrideDefaultKeyboardUrlRef) {
  InitComponentExtension();
  const GURL default_url("chrome://inputview.html");
  GetActiveIMEState()->input_view_url = default_url;

  EXPECT_EQ(default_url, GetActiveIMEState()->GetInputViewUrl());

  manager_->OverrideKeyboardKeyset(mojom::ImeKeyset::kEmoji);
  EXPECT_EQ(default_url, GetActiveIMEState()->GetInputViewUrl());
}

TEST_F(InputMethodManagerImplTest, AllowedKeyboardLayoutsValid) {
  InitComponentExtension();

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
  EXPECT_TRUE(
      manager_->GetActiveIMEState()->SetAllowedInputMethods(allowed, true));
  EXPECT_THAT(manager_->GetActiveIMEState()->GetActiveInputMethodIds(),
              testing::ElementsAre(ImeIdFromEngineId("xkb:us::eng")));
  EXPECT_THAT(manager_->GetActiveIMEState()->GetCurrentInputMethod().id(),
              ImeIdFromEngineId("xkb:us::eng"));
  EXPECT_THAT(manager_->GetActiveIMEState()->GetAllowedInputMethods(),
              testing::ElementsAre(ImeIdFromEngineId("xkb:us::eng")));
}

TEST_F(InputMethodManagerImplTest, AllowedKeyboardLayoutsInvalid) {
  InitComponentExtension();

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
  EXPECT_FALSE(
      manager_->GetActiveIMEState()->SetAllowedInputMethods(allowed, true));
  EXPECT_THAT(manager_->GetActiveIMEState()->GetCurrentInputMethod().id(),
              original_input_method);
  EXPECT_THAT(manager_->GetActiveIMEState()->GetAllowedInputMethods(),
              testing::IsEmpty());
}

TEST_F(InputMethodManagerImplTest, AllowedKeyboardLayoutsValidAndInvalid) {
  InitComponentExtension();

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
  EXPECT_TRUE(
      manager_->GetActiveIMEState()->SetAllowedInputMethods(allowed, true));
  EXPECT_THAT(manager_->GetActiveIMEState()->GetCurrentInputMethod().id(),
              original_input_method_1);
  EXPECT_THAT(manager_->GetActiveIMEState()->GetAllowedInputMethods(),
              testing::ElementsAre(original_input_method_1));

  // Try to re-enable xkb:de::ger
  EXPECT_FALSE(manager_->GetActiveIMEState()->EnableInputMethod(
      original_input_method_2));
}

TEST_F(InputMethodManagerImplTest, AllowedKeyboardLayoutsAndExtensions) {
  InitComponentExtension();

  EXPECT_TRUE(manager_->GetActiveIMEState()->EnableInputMethod(
      ImeIdFromEngineId(kNaclMozcJpId)));
  EXPECT_TRUE(manager_->GetActiveIMEState()->EnableInputMethod(
      ImeIdFromEngineId("xkb:fr::fra")));

  std::vector<std::string> allowed = {"xkb:us::eng"};
  EXPECT_TRUE(
      manager_->GetActiveIMEState()->SetAllowedInputMethods(allowed, true));

  EXPECT_TRUE(manager_->GetActiveIMEState()->EnableInputMethod(
      ImeIdFromEngineId(kNaclMozcUsId)));
  EXPECT_THAT(manager_->GetActiveIMEState()->GetActiveInputMethodIds(),
              testing::ElementsAre(ImeIdFromEngineId("xkb:us::eng"),
                                   ImeIdFromEngineId(kNaclMozcJpId),
                                   ImeIdFromEngineId(kNaclMozcUsId)));
}

TEST_F(InputMethodManagerImplTest, SetLoginDefaultWithAllowedKeyboardLayouts) {
  InitComponentExtension();

  std::vector<std::string> allowed = {"xkb:us::eng", "xkb:de::ger",
                                      "xkb:fr::fra"};
  EXPECT_TRUE(
      manager_->GetActiveIMEState()->SetAllowedInputMethods(allowed, true));
  manager_->GetActiveIMEState()->SetInputMethodLoginDefault();
  EXPECT_THAT(manager_->GetActiveIMEState()->GetActiveInputMethodIds(),
              testing::ElementsAre(ImeIdFromEngineId("xkb:us::eng"),
                                   ImeIdFromEngineId("xkb:de::ger"),
                                   ImeIdFromEngineId("xkb:fr::fra")));
}

// Verifies that the combination of InputMethodManagerImpl and
// ImeControllerClient sends the correct data to ash.
TEST_F(InputMethodManagerImplTest, IntegrationWithAsh) {
  TestImeController ime_controller;  // Mojo interface to ash.
  ImeControllerClient ime_controller_client(manager_.get());
  ime_controller_client.InitForTesting(ime_controller.CreateRemote());

  // Setup 3 IMEs.
  InitComponentExtension();
  manager_->SetUISessionState(InputMethodManager::STATE_BROWSER_SCREEN);
  std::vector<std::string> ids;
  ids.push_back(ImeIdFromEngineId("xkb:us:dvorak:eng"));
  ids.push_back(ImeIdFromEngineId(kExt2Engine2Id));
  ids.push_back(ImeIdFromEngineId(kExt2Engine1Id));
  manager_->GetActiveIMEState()->ReplaceEnabledInputMethods(ids);
  ime_controller_client.FlushMojoForTesting();

  // Ash received the IMEs.
  ASSERT_EQ(3u, ime_controller.available_imes_.size());
  EXPECT_EQ(ImeIdFromEngineId(ids[0]), ime_controller.current_ime_id_);

  // Switch to Mozc.
  manager_->GetActiveIMEState()->SwitchToNextInputMethod();
  ime_controller_client.FlushMojoForTesting();
  EXPECT_EQ(ImeIdFromEngineId(ids[1]), ime_controller.current_ime_id_);

  // Lock the screen.
  scoped_refptr<input_method::InputMethodManager::State> saved_ime_state =
      manager_->GetActiveIMEState();
  manager_->SetState(saved_ime_state->Clone());
  manager_->GetActiveIMEState()->EnableLockScreenLayouts();
  manager_->SetUISessionState(InputMethodManager::STATE_LOCK_SCREEN);
  ime_controller_client.FlushMojoForTesting();
  EXPECT_EQ(2u, ime_controller.available_imes_.size());  // Qwerty+Dvorak.
  EXPECT_EQ(ImeIdFromEngineId("xkb:us:dvorak:eng"),
            ime_controller.current_ime_id_);

  manager_->GetActiveIMEState()->SwitchToNextInputMethod();
  ime_controller_client.FlushMojoForTesting();
  EXPECT_EQ(ImeIdFromEngineId("xkb:us::eng"),  // The hardware keyboard layout.
            ime_controller.current_ime_id_);

  // Unlock screen. The original state, pinyin-dv, is restored.
  manager_->SetState(saved_ime_state);
  manager_->SetUISessionState(InputMethodManager::STATE_BROWSER_SCREEN);
  ime_controller_client.FlushMojoForTesting();
  ASSERT_EQ(3u, ime_controller.available_imes_.size());  // Dvorak and 2 IMEs.
  EXPECT_EQ(ImeIdFromEngineId(ids[1]), ime_controller.current_ime_id_);
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
  InitComponentExtension();
  manager_->SetUISessionState(InputMethodManager::STATE_BROWSER_SCREEN);

  // There is one default IME
  EXPECT_EQ(1u, manager_->GetActiveIMEState()->GetNumActiveInputMethods());

  // Add an ARC IMEs.
  std::vector<std::string> layouts({"us"});
  std::vector<std::string> languages({"en-US"});

  MockInputMethodEngine engine;

  const std::string ime_id =
      extension_ime_util::GetArcInputMethodID(kExtensionId1, "engine_id");
  const InputMethodDescriptor descriptor(
      ime_id, "arc ime", "AI", layouts, languages,
      false /* is_login_keyboard */, GURL(), GURL());
  InputMethodDescriptors descriptors({descriptor});
  manager_->GetActiveIMEState()->AddInputMethodExtension(kExtensionId1,
                                                         descriptors, &engine);

  InputMethodDescriptors result;
  manager_->GetActiveIMEState()->GetInputMethodExtensions(&result);
  EXPECT_EQ(1u, result.size());
  EXPECT_EQ(ime_id, result[0].id());
  result.clear();

  // The ARC IME is not enabled by default.
  EXPECT_EQ(1u, manager_->GetActiveIMEState()->GetNumActiveInputMethods());

  // Enable it.
  std::vector<std::string> extension_ime_ids({ime_id});
  manager_->GetActiveIMEState()->SetEnabledExtensionImes(&extension_ime_ids);
  EXPECT_EQ(2u, manager_->GetActiveIMEState()->GetNumActiveInputMethods());
  {
    std::unique_ptr<InputMethodDescriptors> methods =
        manager_->GetActiveIMEState()->GetActiveInputMethods();
    EXPECT_EQ(2u, methods->size());
    EXPECT_EQ(ime_id, methods->at(1).id());
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

}  // namespace input_method
}  // namespace chromeos
