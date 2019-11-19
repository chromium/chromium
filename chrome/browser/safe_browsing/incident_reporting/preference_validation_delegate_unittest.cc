// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/incident_reporting/preference_validation_delegate.h"

#include <stddef.h>

#include <memory>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/values.h"
#include "chrome/browser/safe_browsing/incident_reporting/incident.h"
#include "chrome/browser/safe_browsing/incident_reporting/mock_incident_receiver.h"
#include "components/safe_browsing/proto/csd.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::IsNull;
using ::testing::NiceMock;
using ::testing::WithArg;

using ValueState =
    prefs::mojom::TrackedPreferenceValidationDelegate::ValueState;

namespace {
const char kPrefPath[] = "atomic.pref";
}

// A basic test harness that creates a delegate instance for which it stores all
// incidents. Tests can push data to the delegate and verify that the test
// instance was provided with the expected data.
class PreferenceValidationDelegateTest : public testing::Test {
 protected:
  typedef std::vector<std::unique_ptr<safe_browsing::Incident>> IncidentVector;

  void SetUp() override {
    testing::Test::SetUp();
    invalid_keys_.push_back(std::string("one"));
    invalid_keys_.push_back(std::string("two"));
    external_validation_invalid_keys_.push_back(std::string("three"));
    std::unique_ptr<safe_browsing::MockIncidentReceiver> receiver(
        new NiceMock<safe_browsing::MockIncidentReceiver>());
    ON_CALL(*receiver, DoAddIncidentForProfile(IsNull(), _))
        .WillByDefault(WithArg<1>(TakeIncidentToVector(&incidents_)));
    instance_.reset(new safe_browsing::PreferenceValidationDelegate(
        nullptr, std::move(receiver)));
  }

  static void ExpectValueStatesEquate(
      ValueState store_state,
      ValueState external_validation_value_state,
      safe_browsing::
          ClientIncidentReport_IncidentData_TrackedPreferenceIncident_ValueState
              incident_state) {
    typedef safe_browsing::
        ClientIncidentReport_IncidentData_TrackedPreferenceIncident TPIncident;
    switch (store_state) {
      case ValueState::CLEARED:
        EXPECT_EQ(TPIncident::CLEARED, incident_state);
        break;
      case ValueState::CHANGED:
        EXPECT_EQ(TPIncident::CHANGED, incident_state);
        break;
      case ValueState::UNTRUSTED_UNKNOWN_VALUE:
        EXPECT_EQ(TPIncident::UNTRUSTED_UNKNOWN_VALUE, incident_state);
        break;
      default:
        if (external_validation_value_state == ValueState::CLEARED) {
          EXPECT_EQ(TPIncident::BYPASS_CLEARED, incident_state);
        } else if (external_validation_value_state == ValueState::CHANGED) {
          EXPECT_EQ(TPIncident::BYPASS_CHANGED, incident_state);
        } else {
          FAIL() << "unexpected store state";
        }
        break;
    }
  }

  static void ExpectKeysEquate(
      const std::vector<std::string>& store_keys,
      const google::protobuf::RepeatedPtrField<std::string>& incident_keys) {
    ASSERT_EQ(store_keys.size(), static_cast<size_t>(incident_keys.size()));
    for (int i = 0; i < incident_keys.size(); ++i) {
      EXPECT_EQ(store_keys[i], incident_keys.Get(i));
    }
  }

  IncidentVector incidents_;
  std::vector<std::string> invalid_keys_;
  std::vector<std::string> external_validation_invalid_keys_;
  std::unique_ptr<prefs::mojom::TrackedPreferenceValidationDelegate> instance_;
};

// Tests that a NULL value results in an incident with no value.
TEST_F(PreferenceValidationDelegateTest, NullValue) {
  instance_->OnAtomicPreferenceValidation(
      kPrefPath, base::nullopt, ValueState::CLEARED, ValueState::UNSUPPORTED,
      false /* is_personal */);
  std::unique_ptr<safe_browsing::ClientIncidentReport_IncidentData> incident(
      incidents_.back()->TakePayload());
  EXPECT_FALSE(incident->tracked_preference().has_atomic_value());
  EXPECT_EQ(
      safe_browsing::
          ClientIncidentReport_IncidentData_TrackedPreferenceIncident::CLEARED,
      incident->tracked_preference().value_state());
}

// Tests that all supported value types can be stringified into an incident. The
// parameters for the test are the type of value to test and the expected value
// string.
class PreferenceValidationDelegateValues
    : public PreferenceValidationDelegateTest,
      public testing::WithParamInterface<
          std::tuple<base::Value::Type, const char*>> {
 protected:
  void SetUp() override {
    PreferenceValidationDelegateTest::SetUp();
    value_type_ = std::get<0>(GetParam());
    expected_value_ = std::get<1>(GetParam());
  }

  static base::Value MakeValue(base::Value::Type value_type) {
    using base::Value;
    switch (value_type) {
      case Value::Type::NONE:
        return Value();
      case Value::Type::BOOLEAN:
        return Value(false);
      case Value::Type::INTEGER:
        return Value(47);
      case Value::Type::DOUBLE:
        return Value(0.47);
      case Value::Type::STRING:
        return Value("i have a spleen");
      case Value::Type::DICTIONARY: {
        Value value(base::Value::Type::DICTIONARY);
        value.SetKey("twenty-two", Value(22));
        value.SetKey("forty-seven", Value(47));
        return value;
      }
      case Value::Type::LIST: {
        Value value(base::Value::Type::LIST);
        value.Append(22);
        value.Append(47);
        return value;
      }
      default:
        ADD_FAILURE() << "unsupported value type " << value_type;
    }
    return Value();
  }

  base::Value::Type value_type_;
  const char* expected_value_;
};

TEST_P(PreferenceValidationDelegateValues, Value) {
  instance_->OnAtomicPreferenceValidation(
      kPrefPath, MakeValue(value_type_), ValueState::CLEARED,
      ValueState::UNSUPPORTED, false /* is_personal */);
  ASSERT_EQ(1U, incidents_.size());
  std::unique_ptr<safe_browsing::ClientIncidentReport_IncidentData> incident(
      incidents_.back()->TakePayload());
  EXPECT_EQ(std::string(expected_value_),
            incident->tracked_preference().atomic_value());
}

INSTANTIATE_TEST_SUITE_P(
    Values,
    PreferenceValidationDelegateValues,
    // On Android, make_tuple(..., "null") doesn't compile due to the error:
    // testing/gtest/include/gtest/internal/gtest-tuple.h:246:48:
    //   error: array used as initializer
    testing::Values(
        std::make_tuple(base::Value::Type::NONE, const_cast<char*>("null")),
        std::make_tuple(base::Value::Type::BOOLEAN, const_cast<char*>("false")),
        std::make_tuple(base::Value::Type::INTEGER, const_cast<char*>("47")),
        std::make_tuple(base::Value::Type::DOUBLE, const_cast<char*>("0.47")),
        std::make_tuple(base::Value::Type::STRING,
                        const_cast<char*>("i have a spleen")),
        std::make_tuple(
            base::Value::Type::DICTIONARY,
            const_cast<char*>("{\"forty-seven\":47,\"twenty-two\":22}")),
        std::make_tuple(base::Value::Type::LIST,
                        const_cast<char*>("[22,47]"))));

// Tests that no incidents are reported for relevant combinations of ValueState.
class PreferenceValidationDelegateNoIncident
    : public PreferenceValidationDelegateTest,
      public testing::WithParamInterface<std::tuple<ValueState, ValueState>> {
 protected:
  void SetUp() override {
    PreferenceValidationDelegateTest::SetUp();
    value_state_ = std::get<0>(GetParam());
    external_validation_value_state_ = std::get<1>(GetParam());
  }

  ValueState value_state_;
  ValueState external_validation_value_state_;
};

TEST_P(PreferenceValidationDelegateNoIncident, Atomic) {
  instance_->OnAtomicPreferenceValidation(
      kPrefPath, base::make_optional<base::Value>(), value_state_,
      external_validation_value_state_, false /* is_personal */);
  EXPECT_EQ(0U, incidents_.size());
}

TEST_P(PreferenceValidationDelegateNoIncident, Split) {
  instance_->OnSplitPreferenceValidation(
      kPrefPath, invalid_keys_, external_validation_invalid_keys_, value_state_,
      external_validation_value_state_, false /* is_personal */);
  EXPECT_EQ(0U, incidents_.size());
}

INSTANTIATE_TEST_SUITE_P(
    NoIncident,
    PreferenceValidationDelegateNoIncident,
    testing::Combine(testing::Values(ValueState::UNCHANGED,
                                     ValueState::SECURE_LEGACY,
                                     ValueState::TRUSTED_UNKNOWN_VALUE),
                     testing::Values(ValueState::UNCHANGED,
                                     ValueState::UNSUPPORTED,
                                     ValueState::UNTRUSTED_UNKNOWN_VALUE)));

// Tests that incidents are reported for relevant combinations of ValueState and
// impersonal/personal.
class PreferenceValidationDelegateWithIncident
    : public PreferenceValidationDelegateTest,
      public testing::WithParamInterface<
          std::tuple<ValueState, ValueState, bool>> {
 protected:
  void SetUp() override {
    PreferenceValidationDelegateTest::SetUp();
    value_state_ = std::get<0>(GetParam());
    external_validation_value_state_ = std::get<1>(GetParam());
    is_personal_ = std::get<2>(GetParam());
  }

  ValueState value_state_;
  ValueState external_validation_value_state_;
  bool is_personal_;
};

TEST_P(PreferenceValidationDelegateWithIncident, Atomic) {
  instance_->OnAtomicPreferenceValidation(
      kPrefPath, base::make_optional<base::Value>(), value_state_,
      external_validation_value_state_, is_personal_);
  ASSERT_EQ(1U, incidents_.size());
  std::unique_ptr<safe_browsing::ClientIncidentReport_IncidentData> incident(
      incidents_.back()->TakePayload());
  EXPECT_TRUE(incident->has_tracked_preference());
  const safe_browsing::
      ClientIncidentReport_IncidentData_TrackedPreferenceIncident& tp_incident =
          incident->tracked_preference();
  EXPECT_EQ(kPrefPath, tp_incident.path());
  EXPECT_EQ(0, tp_incident.split_key_size());
  if (!is_personal_) {
    EXPECT_TRUE(tp_incident.has_atomic_value());
    EXPECT_EQ(std::string("null"), tp_incident.atomic_value());
  } else {
    EXPECT_FALSE(tp_incident.has_atomic_value());
  }
  EXPECT_TRUE(tp_incident.has_value_state());
  ExpectValueStatesEquate(value_state_, external_validation_value_state_,
                          tp_incident.value_state());
}

TEST_P(PreferenceValidationDelegateWithIncident, Split) {
  instance_->OnSplitPreferenceValidation(
      kPrefPath, invalid_keys_, external_validation_invalid_keys_, value_state_,
      external_validation_value_state_, is_personal_);
  ASSERT_EQ(1U, incidents_.size());
  std::unique_ptr<safe_browsing::ClientIncidentReport_IncidentData> incident(
      incidents_.back()->TakePayload());
  EXPECT_TRUE(incident->has_tracked_preference());
  const safe_browsing::
      ClientIncidentReport_IncidentData_TrackedPreferenceIncident& tp_incident =
          incident->tracked_preference();
  EXPECT_EQ(kPrefPath, tp_incident.path());
  EXPECT_FALSE(tp_incident.has_atomic_value());
  if (!is_personal_) {
    if (value_state_ == ValueState::CLEARED ||
        value_state_ == ValueState::CHANGED ||
        value_state_ == ValueState::UNTRUSTED_UNKNOWN_VALUE) {
      ExpectKeysEquate(invalid_keys_, tp_incident.split_key());
    } else {
      ExpectKeysEquate(external_validation_invalid_keys_,
                       tp_incident.split_key());
    }
  } else {
    EXPECT_EQ(0, tp_incident.split_key_size());
  }
  EXPECT_TRUE(tp_incident.has_value_state());
  ExpectValueStatesEquate(value_state_, external_validation_value_state_,
                          tp_incident.value_state());
}

INSTANTIATE_TEST_SUITE_P(
    WithIncident,
    PreferenceValidationDelegateWithIncident,
    testing::Combine(testing::Values(ValueState::CLEARED,
                                     ValueState::CHANGED,
                                     ValueState::UNTRUSTED_UNKNOWN_VALUE),
                     testing::Values(ValueState::UNCHANGED,
                                     ValueState::UNSUPPORTED,
                                     ValueState::UNTRUSTED_UNKNOWN_VALUE),
                     testing::Bool()));

INSTANTIATE_TEST_SUITE_P(
    WithBypassIncident,
    PreferenceValidationDelegateWithIncident,
    testing::Combine(testing::Values(ValueState::UNCHANGED,
                                     ValueState::SECURE_LEGACY,
                                     ValueState::TRUSTED_UNKNOWN_VALUE),
                     testing::Values(ValueState::CHANGED, ValueState::CLEARED),
                     testing::Bool()));

INSTANTIATE_TEST_SUITE_P(
    WithIncidentIgnoreBypass,
    PreferenceValidationDelegateWithIncident,
    testing::Combine(testing::Values(ValueState::CLEARED,
                                     ValueState::CHANGED,
                                     ValueState::UNTRUSTED_UNKNOWN_VALUE),
                     testing::Values(ValueState::CHANGED, ValueState::CLEARED),
                     testing::Bool()));
