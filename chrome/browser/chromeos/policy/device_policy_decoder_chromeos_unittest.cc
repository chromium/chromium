// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/device_policy_decoder_chromeos.h"

#include "base/bind.h"
#include "components/policy/core/common/policy_bundle.h"
#include "components/policy/policy_constants.h"
#include "components/policy/proto/chrome_device_policy.pb.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

namespace em = enterprise_management;

namespace policy {

namespace {

constexpr char kInvalidJson[] = R"({"foo": "bar")";

constexpr char kInvalidPolicyName[] = "invalid-policy-name";

constexpr char kWallpaperJson[] = R"({
      "url": "https://example.com/device_wallpaper.jpg",
      "hash": "examplewallpaperhash"
    })";

constexpr char kWallpaperJsonInvalidValue[] = R"({
      "url": 123,
      "hash": "examplewallpaperhash"
    })";

constexpr char kWallpaperJsonUnknownProperty[] = R"({
    "url": "https://example.com/device_wallpaper.jpg",
    "hash": "examplewallpaperhash",
    "unknown-field": "random-value"
  })";

constexpr char kWallpaperUrlPropertyName[] = "url";
constexpr char kWallpaperUrlPropertyValue[] =
    "https://example.com/device_wallpaper.jpg";
constexpr char kWallpaperHashPropertyName[] = "hash";
constexpr char kWallpaperHashPropertyValue[] = "examplewallpaperhash";
const char kUserWhitelist[] = "*@test-domain.com";

}  // namespace

class DevicePolicyDecoderChromeOSTest : public testing::Test {
 public:
  DevicePolicyDecoderChromeOSTest() = default;
  ~DevicePolicyDecoderChromeOSTest() override = default;

 protected:
  std::unique_ptr<base::Value> GetWallpaperDict() const;

 private:
  DISALLOW_COPY_AND_ASSIGN(DevicePolicyDecoderChromeOSTest);
};

std::unique_ptr<base::Value> DevicePolicyDecoderChromeOSTest::GetWallpaperDict()
    const {
  auto dict = std::make_unique<base::DictionaryValue>();
  dict->SetKey(kWallpaperUrlPropertyName,
               base::Value(kWallpaperUrlPropertyValue));
  dict->SetKey(kWallpaperHashPropertyName,
               base::Value(kWallpaperHashPropertyValue));
  return dict;
}

TEST_F(DevicePolicyDecoderChromeOSTest,
       DecodeJsonStringAndNormalizeJSONParseError) {
  std::string error;
  base::Optional<base::Value> decoded_json = DecodeJsonStringAndNormalize(
      kInvalidJson, key::kDeviceWallpaperImage, &error);
  EXPECT_FALSE(decoded_json.has_value());
  EXPECT_NE(std::string::npos,
            error.find("Invalid JSON string: Line: 1, column: 14"));
}

#if GTEST_HAS_DEATH_TEST
TEST_F(DevicePolicyDecoderChromeOSTest,
       DecodeJsonStringAndNormalizeInvalidSchema) {
  std::string error;
  EXPECT_DEATH(
      DecodeJsonStringAndNormalize(kWallpaperJson, kInvalidPolicyName, &error),
      "");
}
#endif

TEST_F(DevicePolicyDecoderChromeOSTest,
       DecodeJsonStringAndNormalizeInvalidValue) {
  std::string error;
  base::Optional<base::Value> decoded_json = DecodeJsonStringAndNormalize(
      kWallpaperJsonInvalidValue, key::kDeviceWallpaperImage, &error);
  EXPECT_FALSE(decoded_json.has_value());
  EXPECT_EQ(
      "Invalid policy value: The value type doesn't match the schema type. (at "
      "url)",
      error);
}

TEST_F(DevicePolicyDecoderChromeOSTest,
       DecodeJsonStringAndNormalizeUnknownProperty) {
  std::string error;
  base::Optional<base::Value> decoded_json = DecodeJsonStringAndNormalize(
      kWallpaperJsonUnknownProperty, key::kDeviceWallpaperImage, &error);
  EXPECT_EQ(*GetWallpaperDict(), decoded_json.value());
  EXPECT_EQ(
      "Dropped unknown properties: Unknown property: unknown-field (at "
      "toplevel)",
      error);
}

TEST_F(DevicePolicyDecoderChromeOSTest, DecodeJsonStringAndNormalizeSuccess) {
  std::string error;
  base::Optional<base::Value> decoded_json = DecodeJsonStringAndNormalize(
      kWallpaperJson, key::kDeviceWallpaperImage, &error);
  EXPECT_EQ(*GetWallpaperDict(), decoded_json.value());
  EXPECT_TRUE(error.empty());
}

TEST_F(DevicePolicyDecoderChromeOSTest, UserWhitelistWarning) {
  PolicyBundle bundle;
  PolicyMap& policies = bundle.Get(PolicyNamespace(POLICY_DOMAIN_CHROME, ""));

  base::WeakPtr<ExternalDataManager> external_data_manager;

  em::ChromeDeviceSettingsProto device_policy;
  device_policy.mutable_user_whitelist()->add_user_whitelist()->assign(
      kUserWhitelist);

  DecodeDevicePolicy(device_policy, external_data_manager, &policies);

  EXPECT_TRUE(policies.GetValue(key::kDeviceUserWhitelist));

  std::vector<base::Value> list;
  list.emplace_back(base::Value(kUserWhitelist));
  EXPECT_EQ(base::ListValue(list),
            *policies.GetValue(key::kDeviceUserWhitelist));

  base::RepeatingCallback<base::string16(int)> l10nlookup =
      base::BindRepeating(&l10n_util::GetStringUTF16);

  // Should have a deprecation warning.
  EXPECT_FALSE(policies.Get(key::kDeviceUserWhitelist)
                   ->GetLocalizedErrors(l10nlookup)
                   .empty());
}

}  // namespace policy
