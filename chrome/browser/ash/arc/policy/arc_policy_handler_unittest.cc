// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/policy/arc_policy_handler.h"

#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/json/json_string_value_serializer.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/ash/arc/policy/arc_policy_bridge.h"
#include "chrome/browser/ash/arc/policy/arc_policy_util.h"
#include "chrome/browser/ash/arc/policy/managed_configuration_variables.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace arc {

namespace {

constexpr char kKey1[] = "key1";
constexpr char kKey2[] = "key2";
constexpr char kKey3[] = "key3";
constexpr char kPackageName1[] = "com.dropbox.android";
constexpr char kPackageName2[] = "de.blinkt.openvpn";
constexpr char kPackageName3[] = "com.citrix.Receiver";
constexpr char kInvalidVariable[] = "klja3\"084t4k5~j1.,mm1!";
constexpr char kKnownVariable1Pattern[] = "known variable: ${%s}";
constexpr char kKnownVariable2Pattern[] = "known variable: ${%s:%s}";
constexpr char kUnknownVariable1Pattern[] = "unknown variable: ${%s}";

base::Value BuildArcPolicyFromApplications(base::Value::List applications) {
  base::Value::Dict arc_policy;
  arc_policy.Set(policy_util::kArcPolicyKeyApplications,
                 std::move(applications));

  std::string json;
  JSONStringValueSerializer serializer(&json);
  serializer.Serialize(arc_policy);
  return base::Value(std::move(json));
}

base::Value BuildApplication(const std::string& package_name,
                             base::Value::Dict managed_configuration) {
  base::Value::Dict application;
  application.Set(ArcPolicyBridge::kPackageName, package_name);
  application.Set(ArcPolicyBridge::kManagedConfiguration,
                  std::move(managed_configuration));
  return base::Value(std::move(application));
}

base::Value SampleArcPolicyWithoutIssues() {
  base::Value::Dict managed_configuration1;

  managed_configuration1.Set(
      kKey1,
      base::StringPrintf(kKnownVariable2Pattern, kDeviceAssetId, kUserEmail));

  base::Value::Dict managed_configuration2;
  managed_configuration2.Set(
      kKey2, base::StringPrintf(kKnownVariable1Pattern, kDeviceDirectoryId));

  base::Value::List applications;
  applications.Append(
      BuildApplication(kPackageName1, std::move(managed_configuration1)));
  applications.Append(
      BuildApplication(kPackageName2, std::move(managed_configuration2)));

  return BuildArcPolicyFromApplications(std::move(applications));
}

base::Value SampleArcPolicyWithIssues() {
  base::Value::Dict managed_configuration1;
  managed_configuration1.Set(
      kKey1,
      base::StringPrintf(kKnownVariable1Pattern, kDeviceAnnotatedLocation));

  base::Value::Dict managed_configuration2;
  managed_configuration2.Set(
      kKey2, base::StringPrintf(kUnknownVariable1Pattern, kInvalidVariable));

  base::Value::Dict managed_configuration3;
  managed_configuration3.Set(
      kKey3, base::StringPrintf(kKnownVariable2Pattern, kDeviceSerialNumber,
                                kUserEmailName));

  base::Value::List applications;
  applications.Append(
      BuildApplication(kPackageName1, std::move(managed_configuration1)));
  applications.Append(
      BuildApplication(kPackageName2, std::move(managed_configuration2)));
  applications.Append(
      BuildApplication(kPackageName3, std::move(managed_configuration3)));

  return BuildArcPolicyFromApplications(std::move(applications));
}

std::u16string FakeL10nLookup(int message_id) {
  return u"$1,$2";
}

}  // namespace

TEST(ArcPolicyHandlerTest, SampleWithoutIssues) {
  // Set up a PolicyMap without managed configuration issues.
  policy::PolicyMap::Entry entry;
  entry.set_value(SampleArcPolicyWithoutIssues());
  policy::PolicyMap policies;
  policies.Set(policy::key::kArcPolicy, std::move(entry));

  // Call the handler to find issues in policy.
  ArcPolicyHandler handler;
  handler.PrepareForDisplaying(&policies);

  // Verify no issues were found.
  EXPECT_FALSE(policies.Get(policy::key::kArcPolicy)
                   ->HasMessage(policy::PolicyMap::MessageType::kInfo));
  EXPECT_FALSE(policies.Get(policy::key::kArcPolicy)
                   ->HasMessage(policy::PolicyMap::MessageType::kError));
  EXPECT_FALSE(policies.Get(policy::key::kArcPolicy)
                   ->HasMessage(policy::PolicyMap::MessageType::kWarning));
}

TEST(ArcPolicyHandlerTest, SampleWithIssues) {
  // Set up a PolicyMap with managed configuration issues.
  policy::PolicyMap::Entry entry;
  entry.set_value(SampleArcPolicyWithIssues());
  policy::PolicyMap policies;
  policies.Set(policy::key::kArcPolicy, std::move(entry));

  // Call the handler to find issues in policy.
  const ArcPolicyHandler handler;
  handler.PrepareForDisplaying(&policies);

  // Verify warnings are found.
  EXPECT_FALSE(policies.Get(policy::key::kArcPolicy)
                   ->HasMessage(policy::PolicyMap::MessageType::kInfo));
  EXPECT_FALSE(policies.Get(policy::key::kArcPolicy)
                   ->HasMessage(policy::PolicyMap::MessageType::kError));
  ASSERT_TRUE(policies.Get(policy::key::kArcPolicy)
                  ->HasMessage(policy::PolicyMap::MessageType::kWarning));

  // Verify the warning messages contains the expected package name.
  const std::u16string messages =
      policies.Get(policy::key::kArcPolicy)
          ->GetLocalizedMessages(policy::PolicyMap::MessageType::kWarning,
                                 base::BindRepeating(&FakeL10nLookup));

  EXPECT_NE(base::UTF16ToUTF8(messages).find(kPackageName2), std::string::npos);
}

}  // namespace arc
