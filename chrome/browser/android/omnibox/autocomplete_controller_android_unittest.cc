// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/omnibox/autocomplete_controller_android.h"

#include <memory>
#include <optional>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/omnibox/browser/autocomplete_controller.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/fake_autocomplete_provider_client.h"
#include "components/omnibox/common/omnibox_features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/metrics_proto/omnibox_event.pb.h"
#include "third_party/metrics_proto/omnibox_focus_type.pb.h"
#include "url/gurl.h"

namespace {

// Aliases.
using ::testing::AllOf;
using ::testing::Bool;
using ::testing::Combine;
using ::testing::Conditional;
using ::testing::Eq;
using ::testing::IsEmpty;
using ::testing::NiceMock;
using ::testing::Property;
using ::testing::Values;
using ::testing::WithParamInterface;

// Mocks -----------------------------------------------------------------------

class MockAutocompleteController : public AutocompleteController {
 public:
  MockAutocompleteController()
      : AutocompleteController(
            std::make_unique<FakeAutocompleteProviderClient>(),
            /*provider_types=*/0) {}

  // AutocompleteController:
  MOCK_METHOD(void, Start, (const AutocompleteInput&), (override));
};

}  // namespace

// AutocompleteControllerAndroidTest -------------------------------------------

// Base class for tests of `AutocompleteControllerAndroid`.
class AutocompleteControllerAndroidTest
    : public ChromeRenderViewHostTestHarness {
 public:
  AutocompleteControllerAndroid* controller() { return controller_; }
  NiceMock<MockAutocompleteController>* mock() { return mock_; }

 private:
  // ChromeRenderViewHostTestHarness:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    // Initialize autocomplete controller/mock.
    controller_ =
        AutocompleteControllerAndroid::Factory::GetForProfile(profile());
    ASSERT_TRUE(controller_);
    mock_ = controller_->SetAutocompleteControllerForTesting(
        std::make_unique<NiceMock<MockAutocompleteController>>());
  }

  raw_ptr<AutocompleteControllerAndroid> controller_ = nullptr;
  raw_ptr<NiceMock<MockAutocompleteController>> mock_ = nullptr;
};

// AutocompleteControllerAndroidOmniboxFocusTest -------------------------------

// Base class for parameterized tests of `AutocompleteControllerAndroid` which
// assert expectations regarding omnibox focus behavior.
class AutocompleteControllerAndroidOmniboxFocusTest
    : public AutocompleteControllerAndroidTest,
      public WithParamInterface<std::tuple<
          /*is_ntp_page=*/bool,
          /*is_on_focus_context=*/bool,
          /*is_retain_omnibox_on_focus_enabled=*/std::optional<bool>>> {
 public:
  AutocompleteControllerAndroidOmniboxFocusTest() {
    if (const auto& is_retain_omnibox_on_focus_enabled =
            IsRetainOmniboxOnFocusEnabled()) {
      scoped_feature_list_.InitWithFeatureState(
          omnibox::kRetainOmniboxOnFocus,
          is_retain_omnibox_on_focus_enabled.value());
    }
  }

  bool IsNtpPage() const { return std::get<0>(GetParam()); }

  bool IsOnFocusContext() const { return std::get<1>(GetParam()); }

  const std::optional<bool>& IsRetainOmniboxOnFocusEnabled() const {
    return std::get<2>(GetParam());
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         AutocompleteControllerAndroidOmniboxFocusTest,
                         Combine(/*is_ntp_page=*/Bool(),
                                 /*is_on_focus_context=*/Bool(),
                                 /*is_retain_omnibox_on_focus_enabled=*/
                                 Values(true, false, std::nullopt)));

// Tests -----------------------------------------------------------------------

TEST_P(AutocompleteControllerAndroidOmniboxFocusTest, OnOmniboxFocused) {
  using OEP = metrics::OmniboxEventProto;
  using OFT = metrics::OmniboxFocusType;

  bool is_ntp_page = IsNtpPage();
  bool is_on_focus_context = IsOnFocusContext();
  bool is_retain_omnibox_on_focus_enabled =
      IsRetainOmniboxOnFocusEnabled().value_or(false);

  JNIEnv* env = base::android::AttachCurrentThread();
  auto j_omnibox_text = base::android::ConvertUTF16ToJavaString(env, u"text");
  auto j_current_url = base::android::ConvertUTF16ToJavaString(env, u"url");
  auto j_current_title = base::android::ConvertUTF16ToJavaString(env, u"title");
  jint j_page_classification = IsNtpPage() ? OEP::NTP : OEP::OTHER;

  bool expect_interaction_clobber_focus_type =
      !(is_ntp_page ||
        (is_on_focus_context && is_retain_omnibox_on_focus_enabled));

  EXPECT_CALL(
      *mock(),
      Start(AllOf(Property(&AutocompleteInput::text,
                           Conditional(expect_interaction_clobber_focus_type,
                                       IsEmpty(), Eq(u"text"))),
                  Property(&AutocompleteInput::current_url, Eq(GURL("url"))),
                  Property(&AutocompleteInput::current_title, Eq(u"title")),
                  Property(&AutocompleteInput::focus_type,
                           Conditional(expect_interaction_clobber_focus_type,
                                       Eq(OFT::INTERACTION_CLOBBER),
                                       Eq(OFT::INTERACTION_FOCUS))))));

  controller()->OnOmniboxFocused(
      env, base::android::JavaParamRef<jstring>(env, j_omnibox_text.obj()),
      base::android::JavaParamRef<jstring>(env, j_current_url.obj()),
      j_page_classification,
      base::android::JavaParamRef<jstring>(env, j_current_title.obj()),
      is_on_focus_context);
}
