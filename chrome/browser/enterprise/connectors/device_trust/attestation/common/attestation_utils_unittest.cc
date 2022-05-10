// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/attestation/common/attestation_utils.h"

#include <string>
#include <utility>

#include "base/values.h"
#include "components/device_signals/core/common/signals_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace enterprise_connectors {

namespace {

using google::protobuf::RepeatedPtrField;

constexpr char kFakeDeviceId[] = "FakeID";
constexpr bool kFakeCleanupEnabled = true;
constexpr int kFakePasswordProtectionWarningTrigger = 12345;

}  // namespace

class AttestationUtilsTest : public testing::Test {
 protected:
  void SetUp() override {
    signals_dict.Set(device_signals::names::kDeviceId, kFakeDeviceId);
    signals_dict.Set(device_signals::names::kChromeCleanupEnabled,
                     kFakeCleanupEnabled);
    signals_dict.Set(device_signals::names::kPasswordProtectionWarningTrigger,
                     kFakePasswordProtectionWarningTrigger);

    base::Value::List imei_list;
    imei_list.Append(base::Value("imei1"));
    imei_list.Append(base::Value("imei2"));
    imei_list.Append(base::Value("imei3"));
    imei_list_clone = imei_list.Clone();
    signals_dict.Set(device_signals::names::kImei, std::move(imei_list));
  }

  base::Value::List RepeatedFieldToList(
      const RepeatedPtrField<std::string>& values) {
    base::Value::List list;
    if (values.empty()) {
      return list;
    }

    for (const auto& value : values) {
      list.Append(base::Value(value.c_str()));
    }

    return list;
  }
  base::Value::List imei_list_clone;
  base::Value::Dict signals_dict;
};

TEST_F(AttestationUtilsTest, DictToProtoConversion_SignalsContainsAllTypes) {
  SignalsType signals_proto =
      *enterprise_connectors::DictionarySignalsToProtobufSignals(signals_dict);
  EXPECT_EQ(kFakeDeviceId, signals_proto.device_id());
  EXPECT_EQ(kFakeCleanupEnabled, signals_proto.chrome_cleanup_enabled());
  EXPECT_EQ(kFakePasswordProtectionWarningTrigger,
            signals_proto.password_protection_warning_trigger());
  EXPECT_EQ(imei_list_clone, RepeatedFieldToList(signals_proto.imei()));
}

TEST_F(AttestationUtilsTest, DictToProtoConversion_SignalsOnlyImei) {
  signals_dict.Remove(device_signals::names::kDeviceId);
  signals_dict.Remove(device_signals::names::kChromeCleanupEnabled);
  signals_dict.Remove(device_signals::names::kPasswordProtectionWarningTrigger);
  SignalsType signals_proto =
      *enterprise_connectors::DictionarySignalsToProtobufSignals(signals_dict);
  EXPECT_FALSE(signals_proto.has_device_id());
  EXPECT_FALSE(signals_proto.has_chrome_cleanup_enabled());
  EXPECT_FALSE(signals_proto.has_password_protection_warning_trigger());
  EXPECT_EQ(imei_list_clone, RepeatedFieldToList(signals_proto.imei()));
}

TEST_F(AttestationUtilsTest, DictToProtoConversion_EmptySignals) {
  signals_dict.Remove(device_signals::names::kDeviceId);
  signals_dict.Remove(device_signals::names::kChromeCleanupEnabled);
  signals_dict.Remove(device_signals::names::kPasswordProtectionWarningTrigger);
  signals_dict.Remove(device_signals::names::kImei);
  SignalsType signals_proto =
      *enterprise_connectors::DictionarySignalsToProtobufSignals(signals_dict);
  EXPECT_FALSE(signals_proto.has_device_id());
  EXPECT_FALSE(signals_proto.has_chrome_cleanup_enabled());
  EXPECT_FALSE(signals_proto.has_password_protection_warning_trigger());

  base::Value::List list;
  EXPECT_EQ(list, RepeatedFieldToList(signals_proto.imei()));
}

}  // namespace enterprise_connectors
