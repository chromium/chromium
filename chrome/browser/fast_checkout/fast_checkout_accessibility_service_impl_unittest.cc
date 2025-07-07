// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/fast_checkout/fast_checkout_accessibility_service_impl.h"

#include <memory>
#include <string>

#include "chrome/browser/ui/android/autofill/autofill_accessibility_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class MockAutofillAccessibilityHelper
    : public autofill::AutofillAccessibilityHelper {
 public:
  MockAutofillAccessibilityHelper() = default;
  ~MockAutofillAccessibilityHelper() override = default;

  MOCK_METHOD(void,
              AnnounceTextForA11y,
              (const std::u16string& message),
              (override));
};

class FastCheckoutAccessibilityServiceImplTest : public testing::Test {
 public:
  FastCheckoutAccessibilityServiceImplTest() = default;
  ~FastCheckoutAccessibilityServiceImplTest() override = default;

 protected:
  void SetUp() override {
    mock_accessibility_helper_ =
        std::make_unique<MockAutofillAccessibilityHelper>();
    autofill::AutofillAccessibilityHelper::SetInstanceForTesting(
        mock_accessibility_helper_.get());

    service_ = std::make_unique<FastCheckoutAccessibilityServiceImpl>();
  }

  void TearDown() override {
    autofill::AutofillAccessibilityHelper::SetInstanceForTesting(nullptr);
    mock_accessibility_helper_.reset();
    service_.reset();
  }

  MockAutofillAccessibilityHelper* mock_accessibility_helper() {
    return mock_accessibility_helper_.get();
  }

  FastCheckoutAccessibilityServiceImpl* service() { return service_.get(); }

 private:
  std::unique_ptr<MockAutofillAccessibilityHelper> mock_accessibility_helper_;
  std::unique_ptr<FastCheckoutAccessibilityServiceImpl> service_;
};

// Verifies that the Announce method properly delegates to
// AutofillAccessibilityHelper.
TEST_F(FastCheckoutAccessibilityServiceImplTest,
       Announce_CallsAutofillAccessibilityHelper) {
  const std::u16string test_message = u"Fast checkout completed successfully";

  EXPECT_CALL(*mock_accessibility_helper(), AnnounceTextForA11y(test_message))
      .Times(1);

  service()->Announce(test_message);
}

// Tests that empty messages are handled gracefully without crashes.
TEST_F(FastCheckoutAccessibilityServiceImplTest, Announce_WithEmptyMessage) {
  const std::u16string empty_message = u"";

  EXPECT_CALL(*mock_accessibility_helper(), AnnounceTextForA11y(empty_message))
      .Times(1);

  service()->Announce(empty_message);
}

// Ensures that lengthy accessibility messages are processed correctly.
TEST_F(FastCheckoutAccessibilityServiceImplTest, Announce_WithLongMessage) {
  const std::u16string long_message =
      u"This is a very long accessibility message for fast checkout that "
      u"should still be properly announced to users with accessibility needs "
      u"to ensure they understand what action was completed.";

  EXPECT_CALL(*mock_accessibility_helper(), AnnounceTextForA11y(long_message))
      .Times(1);

  service()->Announce(long_message);
}

// Tests that multiple consecutive calls to Announce work correctly.
TEST_F(FastCheckoutAccessibilityServiceImplTest, Announce_MultipleCalls) {
  const std::u16string message1 = u"Starting fast checkout";
  const std::u16string message2 = u"Processing payment";
  const std::u16string message3 = u"Fast checkout completed";

  EXPECT_CALL(*mock_accessibility_helper(), AnnounceTextForA11y(message1))
      .Times(1);
  EXPECT_CALL(*mock_accessibility_helper(), AnnounceTextForA11y(message2))
      .Times(1);
  EXPECT_CALL(*mock_accessibility_helper(), AnnounceTextForA11y(message3))
      .Times(1);

  service()->Announce(message1);
  service()->Announce(message2);
  service()->Announce(message3);
}

// Tests handling of messages containing special characters, symbols, and
// emojis.
TEST_F(FastCheckoutAccessibilityServiceImplTest,
       Announce_WithSpecialCharacters) {
  const std::u16string message_with_special_chars =
      u"Fast checkout: 100% complete! ✓ Payment of $29.99 processed "
      u"successfully.";

  EXPECT_CALL(*mock_accessibility_helper(),
              AnnounceTextForA11y(message_with_special_chars))
      .Times(1);

  service()->Announce(message_with_special_chars);
}

// Tests with realistic messages that would be used during actual fast checkout
// flow.
TEST_F(FastCheckoutAccessibilityServiceImplTest,
       Announce_RealisticFastCheckoutMessages) {
  const std::vector<std::u16string> realistic_messages = {
      u"Fast checkout started", u"Autofilling shipping address",
      u"Autofilling payment information", u"Submitting order",
      u"Order submitted successfully"};

  for (const auto& message : realistic_messages) {
    EXPECT_CALL(*mock_accessibility_helper(), AnnounceTextForA11y(message))
        .Times(1);
    service()->Announce(message);
    testing::Mock::VerifyAndClearExpectations(mock_accessibility_helper());
  }
}

// Verifies that the implementation correctly inherits from base interface.
TEST_F(FastCheckoutAccessibilityServiceImplTest, InheritsFromBaseClass) {
  FastCheckoutAccessibilityService* base_service = service();
  ASSERT_NE(base_service, nullptr);

  const std::u16string test_message = u"Interface compatibility test";
  EXPECT_CALL(*mock_accessibility_helper(), AnnounceTextForA11y(test_message))
      .Times(1);

  base_service->Announce(test_message);
}

}  // namespace
