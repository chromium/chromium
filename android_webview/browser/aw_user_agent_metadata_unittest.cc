// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "android_webview/browser/aw_client_hints_controller_delegate.h"
#include "android_webview/browser/aw_media_url_interceptor.h"
#include "android_webview/browser/aw_user_agent_metadata.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/user_agent/user_agent_metadata.h"

using testing::Test;

namespace android_webview {

class AwUserAgentMetadataTest : public testing::Test {
 public:
  AwUserAgentMetadataTest() : env_(base::android::AttachCurrentThread()) {}
  ~AwUserAgentMetadataTest() override = default;

  JNIEnv* env() { return env_; }

  void verifyUaMetadata(blink::UserAgentMetadata expect,
                        blink::UserAgentMetadata actual) {
    std::optional<std::string> expect_str =
        blink::UserAgentMetadata::Marshal(expect);
    std::optional<std::string> actual_str =
        blink::UserAgentMetadata::Marshal(actual);
    EXPECT_TRUE(expect_str.has_value() == actual_str.has_value());
    if (expect_str.has_value()) {
      EXPECT_EQ(expect_str.value(), actual_str.value());
    }

    EXPECT_EQ(expect.SerializeBrandMajorVersionList(),
              actual.SerializeBrandMajorVersionList());
    EXPECT_EQ(expect.SerializeBrandFullVersionList(),
              actual.SerializeBrandFullVersionList());
    EXPECT_EQ(expect.full_version, actual.full_version);
    EXPECT_EQ(expect.platform, actual.platform);
    EXPECT_EQ(expect.platform_version, actual.platform_version);
    EXPECT_EQ(expect.architecture, actual.architecture);
    EXPECT_EQ(expect.model, actual.model);
    EXPECT_EQ(expect.mobile, actual.mobile);
    EXPECT_EQ(expect.bitness, actual.bitness);
    EXPECT_EQ(expect.wow64, actual.wow64);
    EXPECT_EQ(expect.form_factors, actual.form_factors);
  }

  std::string RoundTripBitness(std::string_view input) {
    blink::UserAgentMetadata ua_metadata;
    ua_metadata.bitness = input;
    return FromJavaAwUserAgentMetadata(
               env(), ToJavaAwUserAgentMetadata(env(), ua_metadata))
        .bitness;
  }

 private:
  raw_ptr<JNIEnv> env_;
};

TEST_F(AwUserAgentMetadataTest, TestJavaObjectCppObjectMetadataEmpty) {
  blink::UserAgentMetadata ua_metadata;
  verifyUaMetadata(ua_metadata,
                   FromJavaAwUserAgentMetadata(
                       env(), ToJavaAwUserAgentMetadata(env(), ua_metadata)));
}

TEST_F(AwUserAgentMetadataTest, TestJavaObjectCppObjectMetadataFull) {
  blink::UserAgentMetadata ua_metadata = {
      .brand_version_list = {{"b1", "mv1"}, {"b2", "mv2"}},
      .brand_full_version_list = {{"b1", "fv1"}, {"b2", "fv2"}},
      .full_version = "full_version",
      .platform = "platform",
      .platform_version = "platform_version",
      .architecture = "architecture",
      .model = "model",
      .mobile = true,
      .bitness = "64",
      .wow64 = false,
      .form_factors = {"Desktop", "Mobile"}};
  verifyUaMetadata(ua_metadata,
                   FromJavaAwUserAgentMetadata(
                       env(), ToJavaAwUserAgentMetadata(env(), ua_metadata)));
}

TEST_F(AwUserAgentMetadataTest, TestJavaObjectCppObjectMetadataPartFullBrand) {
  blink::UserAgentMetadata ua_metadata = {
      .brand_version_list = {{"b1", "mv1"}, {"b2", "mv2"}},
      .brand_full_version_list = {{"b1", "fv1"}},
      .full_version = "full_version",
      .platform = "platform",
      .platform_version = "platform_version",
      .architecture = "architecture",
      .model = "model",
      .mobile = true,
      .bitness = "64",
      .wow64 = false,
      .form_factors = {"Desktop"}};
  verifyUaMetadata(ua_metadata,
                   FromJavaAwUserAgentMetadata(
                       env(), ToJavaAwUserAgentMetadata(env(), ua_metadata)));
}

TEST_F(AwUserAgentMetadataTest,
       TestJavaObjectCppObjectMetadataNoFullBrandList) {
  blink::UserAgentMetadata ua_metadata = {
      .brand_version_list = {{"b1", "mv1"}, {"b2", "mv2"}},
      .brand_full_version_list = {},
      .full_version = "full_version",
      .platform = "platform",
      .platform_version = "platform_version",
      .architecture = "architecture",
      .model = "model",
      .mobile = true,
      .bitness = "64",
      .wow64 = false,
      .form_factors = {}};
  verifyUaMetadata(ua_metadata,
                   FromJavaAwUserAgentMetadata(
                       env(), ToJavaAwUserAgentMetadata(env(), ua_metadata)));
}

TEST_F(AwUserAgentMetadataTest, TestJavaObjectCppObjectMetadataNoBrandList) {
  blink::UserAgentMetadata ua_metadata = {
      .brand_version_list = {},
      .brand_full_version_list = {},
      .full_version = "full_version",
      .platform = "platform",
      .platform_version = "platform_version",
      .architecture = "architecture",
      .model = "model",
      .mobile = true,
      .bitness = "64",
      .wow64 = false,
      .form_factors = {"Desktop"}};
  verifyUaMetadata(ua_metadata,
                   FromJavaAwUserAgentMetadata(
                       env(), ToJavaAwUserAgentMetadata(env(), ua_metadata)));
}

TEST_F(AwUserAgentMetadataTest, TestJavaObjectCppObjectMetadataLowEntropy) {
  blink::UserAgentMetadata ua_metadata = {
      .brand_version_list = {{"b1", "mv1"}, {"b2", "mv2"}},
      .brand_full_version_list = {},
      .full_version = "",
      .platform = "platform",
      .platform_version = "",
      .architecture = "",
      .model = "",
      .mobile = false,
      .bitness = "",
      .wow64 = false,
      .form_factors = {"Desktop"}};
  verifyUaMetadata(ua_metadata,
                   FromJavaAwUserAgentMetadata(
                       env(), ToJavaAwUserAgentMetadata(env(), ua_metadata)));
}

TEST_F(AwUserAgentMetadataTest, TestJavaObjectCppObjectDefault) {
  blink::UserAgentMetadata ua_metadata =
      AwClientHintsControllerDelegate::GetUserAgentMetadataOverrideBrand();
  verifyUaMetadata(ua_metadata,
                   FromJavaAwUserAgentMetadata(
                       env(), ToJavaAwUserAgentMetadata(env(), ua_metadata)));
}

TEST_F(AwUserAgentMetadataTest, TestJavaObjectCppObjectInvalidBrandVersion) {
  blink::UserAgentMetadata ua_metadata;
  ua_metadata.brand_version_list = {{"b1", "mv1"}, {"b2", "mv2"}};
  ua_metadata.brand_full_version_list = {{"b3", "fv1"}, {"b2", "fv2"}};

  blink::UserAgentMetadata actual_metadata = FromJavaAwUserAgentMetadata(
      env(), ToJavaAwUserAgentMetadata(env(), ua_metadata));

  EXPECT_EQ(blink::UserAgentBrandList({{"b1", "mv1"}, {"b2", "mv2"}}),
            actual_metadata.brand_version_list);
  // As we set a brand `b3` with full version but it doesn't contains the major
  // version list, java will drop this brand version when transfer.
  EXPECT_EQ(blink::UserAgentBrandList({{"b2", "fv2"}}),
            actual_metadata.brand_full_version_list);
}

TEST_F(AwUserAgentMetadataTest, TestBitnessParsingInvalidValueString) {
  EXPECT_EQ("", RoundTripBitness("foo"));
}

TEST_F(AwUserAgentMetadataTest, TestBitnessParsingDefault) {
  EXPECT_EQ("", RoundTripBitness("0"));
}

TEST_F(AwUserAgentMetadataTest, TestBitnessParsingIntValue) {
  EXPECT_EQ("8", RoundTripBitness("8"));
  EXPECT_EQ("16", RoundTripBitness("16"));
  EXPECT_EQ("32", RoundTripBitness("32"));
  EXPECT_EQ("64", RoundTripBitness("64"));
  EXPECT_EQ("128", RoundTripBitness("128"));
  EXPECT_EQ("256", RoundTripBitness("256"));
  EXPECT_EQ("1000", RoundTripBitness("1000"));
}

}  // namespace android_webview
