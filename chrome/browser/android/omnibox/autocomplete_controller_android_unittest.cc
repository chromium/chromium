// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/omnibox/autocomplete_controller_android.h"

#include <memory>
#include <optional>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/memory/raw_ptr.h"
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

TEST_F(AutocompleteControllerAndroidTest, OnOmniboxFocused_NTP) {
  using OEP = metrics::OmniboxEventProto;
  using OFT = metrics::OmniboxFocusType;

  std::u16string url = u"chrome://newtab";

  JNIEnv* env = base::android::AttachCurrentThread();
  auto j_omnibox_text = base::android::ConvertUTF16ToJavaString(env, u"");
  auto j_current_url = base::android::ConvertUTF16ToJavaString(env, url);
  auto j_current_title = base::android::ConvertUTF16ToJavaString(env, u"title");
  jint j_page_classification = OEP::NTP;

  EXPECT_CALL(
      *mock(),
      Start(AllOf(Property(&AutocompleteInput::text, IsEmpty()),
                  Property(&AutocompleteInput::current_url, Eq(GURL(url))),
                  Property(&AutocompleteInput::current_title, Eq(u"title")),
                  Property(&AutocompleteInput::focus_type,
                           Eq(OFT::INTERACTION_FOCUS)))));

  controller()->OnOmniboxFocused(
      env, base::android::JavaParamRef<jstring>(env, j_omnibox_text.obj()),
      base::android::JavaParamRef<jstring>(env, j_current_url.obj()),
      j_page_classification,
      base::android::JavaParamRef<jstring>(env, j_current_title.obj()));
}

TEST_F(AutocompleteControllerAndroidTest, OnOmniboxFocused_OTHER) {
  using OEP = metrics::OmniboxEventProto;
  using OFT = metrics::OmniboxFocusType;

  std::u16string url = u"https://site.biz/";

  JNIEnv* env = base::android::AttachCurrentThread();
  auto j_omnibox_text = base::android::ConvertUTF16ToJavaString(env, u"text");
  auto j_current_url = base::android::ConvertUTF16ToJavaString(env, url);
  auto j_current_title = base::android::ConvertUTF16ToJavaString(env, u"title");
  jint j_page_classification = OEP::OTHER;

  EXPECT_CALL(
      *mock(),
      Start(AllOf(Property(&AutocompleteInput::text, IsEmpty()),
                  Property(&AutocompleteInput::current_url, Eq(GURL(url))),
                  Property(&AutocompleteInput::current_title, Eq(u"title")),
                  Property(&AutocompleteInput::focus_type,
                           Eq(OFT::INTERACTION_FOCUS)))));

  controller()->OnOmniboxFocused(
      env, base::android::JavaParamRef<jstring>(env, j_omnibox_text.obj()),
      base::android::JavaParamRef<jstring>(env, j_current_url.obj()),
      j_page_classification,
      base::android::JavaParamRef<jstring>(env, j_current_title.obj()));
}
