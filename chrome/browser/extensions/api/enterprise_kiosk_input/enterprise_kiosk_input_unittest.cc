// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/api/enterprise_kiosk_input.h"

#include <string>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/ash/crosapi/test_crosapi_dependency_registry.h"
#include "chrome/browser/ash/crosapi/test_crosapi_environment.h"
#include "chrome/browser/extensions/api/enterprise_kiosk_input/enterprise_kiosk_input_api.h"
#include "components/crx_file/id_util.h"
#include "extensions/browser/api_test_utils.h"
#include "extensions/browser/api_unittest.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ime/ash/extension_ime_util.h"
#include "ui/base/ime/ash/fake_input_method_delegate.h"
#include "ui/base/ime/ash/input_method_descriptor.h"
#include "ui/base/ime/ash/input_method_manager.h"
#include "ui/base/ime/ash/input_method_util.h"
#include "ui/base/ime/ash/mock_input_method_manager.h"

namespace extensions {

namespace {

constexpr char kUsLayout[] = "xkb:us::eng";
constexpr char kLatamLayout[] = "xkb:latam::spa";
constexpr char kFrLayout[] = "xkb:fr::fra";
constexpr char kDeLayout[] = "xkb:de::ger";
constexpr char kInvalidLayout[] = "asdf_invalid_pinyin";
constexpr char kNoError[] = "";
constexpr char kErrorMessageTemplate[] =
    "Could not change current input method. Invalid input method id: %s.";

namespace im = ::ash::input_method;
namespace ime_util = ::ash::extension_ime_util;

std::string GetComponentExtensionImeId(const std::string& layout) {
  return ime_util::GetComponentInputMethodID(ime_util::kXkbExtensionId, layout);
}

class TestInputMethodManager : public im::MockInputMethodManager {
 public:
  class TestState : public im::MockInputMethodManager::State {
   public:
    TestState() {
      enabled_input_method_ids_.reserve(3);

      enabled_input_method_ids_.emplace_back(
          GetComponentExtensionImeId(kUsLayout));
      enabled_input_method_ids_.emplace_back(
          GetComponentExtensionImeId(kLatamLayout));
      enabled_input_method_ids_.emplace_back(
          GetComponentExtensionImeId(kFrLayout));

      current_ime_id_ = enabled_input_method_ids_[0];
    }

    TestState(const TestState&) = delete;
    TestState& operator=(const TestState&) = delete;

    void ChangeInputMethod(const std::string& input_method_id,
                           bool show_message) override {
      for (const auto& enabled_id : enabled_input_method_ids_) {
        if (input_method_id == enabled_id) {
          current_ime_id_ = input_method_id;
        }
      }
    }

    im::InputMethodDescriptor GetCurrentInputMethod() const override {
      return im::InputMethodDescriptor(
          /*id=*/current_ime_id_,
          /*name=*/"",
          /*indicator=*/"",
          /*keyboard_layout=*/"",
          /*language_codes=*/std::vector<std::string>(),
          /*is_login_keyboard=*/false,
          /*options_page_url=*/GURL(),
          /*input_view_url=*/GURL(),
          /*handwriting_language=*/std::nullopt);
    }

    const std::vector<std::string>& GetEnabledInputMethodIds() const override {
      return enabled_input_method_ids_;
    }

   protected:
    friend base::RefCounted<im::InputMethodManager::State>;
    ~TestState() override = default;

   private:
    std::vector<std::string> enabled_input_method_ids_;
    std::string current_ime_id_;
  };

  TestInputMethodManager() : state_(new TestState) {}

  TestInputMethodManager(const TestInputMethodManager&) = delete;
  TestInputMethodManager& operator=(const TestInputMethodManager&) = delete;

  scoped_refptr<im::InputMethodManager::State> GetActiveIMEState() override {
    return state_;
  }

  std::string GetMigratedInputMethodID(
      const std::string& input_method_id) override {
    if (input_method_id == kUsLayout || input_method_id == kLatamLayout ||
        input_method_id == kFrLayout) {
      return GetComponentExtensionImeId(input_method_id);
    }
    return "";
  }

 private:
  scoped_refptr<TestState> state_;
};

}  // namespace

class EnterpriseKioskInputTest : public extensions::ApiUnitTest {
 public:
  void SetUp() override {
    ApiUnitTest::SetUp();
    crosapi_environment_.SetUp();
  }

  void TearDown() override {
    crosapi_environment_.TearDown();
    ApiUnitTest::TearDown();
  }

 protected:
  base::Value::List CreateArgs(const std::string& input_method_id) {
    api::enterprise_kiosk_input::SetCurrentInputMethodOptions options;
    options.input_method_id = input_method_id;
    base::Value::List args;
    args.Append(options.ToValue());
    return args;
  }
  crosapi::TestCrosapiEnvironment crosapi_environment_;
};

// Test for API enterprise.kioskInput.setCurrentInputMethod
TEST_F(EnterpriseKioskInputTest, SetCurrentInputMethodFunctionTest) {
  TestInputMethodManager::Initialize(new TestInputMethodManager);

  scoped_refptr<im::InputMethodManager::State> ime_state =
      im::InputMethodManager::Get()->GetActiveIMEState();
  ASSERT_TRUE(ime_state);

  EXPECT_EQ(GetComponentExtensionImeId(kUsLayout),
            ime_state->GetCurrentInputMethod().id());

  {
    auto function = base::MakeRefCounted<
        EnterpriseKioskInputSetCurrentInputMethodFunction>();
    api_test_utils::RunFunction(function.get(), CreateArgs(kLatamLayout),
                                browser_context(),
                                api_test_utils::FunctionMode::kNone);
    EXPECT_EQ(function->GetError(), kNoError);
    EXPECT_EQ(GetComponentExtensionImeId(kLatamLayout),
              ime_state->GetCurrentInputMethod().id());
  }

  {
    auto function = base::MakeRefCounted<
        EnterpriseKioskInputSetCurrentInputMethodFunction>();
    api_test_utils::RunFunction(function.get(), CreateArgs(kDeLayout),
                                browser_context(),
                                api_test_utils::FunctionMode::kNone);
    EXPECT_EQ(function->GetError(),
              base::StringPrintf(kErrorMessageTemplate, kDeLayout));
    EXPECT_EQ(GetComponentExtensionImeId(kLatamLayout),
              ime_state->GetCurrentInputMethod().id());
  }

  {
    auto function = base::MakeRefCounted<
        EnterpriseKioskInputSetCurrentInputMethodFunction>();
    api_test_utils::RunFunction(function.get(), CreateArgs(kInvalidLayout),
                                browser_context(),
                                api_test_utils::FunctionMode::kNone);
    EXPECT_EQ(function->GetError(),
              base::StringPrintf(kErrorMessageTemplate, kInvalidLayout));
    EXPECT_EQ(GetComponentExtensionImeId(kLatamLayout),
              ime_state->GetCurrentInputMethod().id());
  }
}

}  // namespace extensions
