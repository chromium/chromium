// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/companion_app/companion_app_parser.h"

#include "ash/quick_pair/common/device.h"
#include "ash/quick_pair/common/logging.h"
#include "ash/quick_pair/repository/fake_fast_pair_repository.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
constexpr char kTestDeviceAddress[] = "11:12:13:14:15:16";
constexpr char kHasCompanionAppModelId[] = "718c17";
constexpr char kEmptyMetadataModelId[] = "718c16";
constexpr char kValidMetadataModelId[] = "718c15";
constexpr char kEmptyParameterMetadataModelId[] = "718c14";
constexpr char kNoMetadataModelId[] = "718c12";
constexpr char kValidIntentWithCompanionAppURI[] =
    "intent:#Intent;action=com.google.android.gms.nearby."
    "discovery%3AACTION_MAGIC_PAIR;package=com.google.android."
    "gms;component=com.google.android.gms/.nearby.discovery."
    "service.DiscoveryService;S.com.google.android.gms.nearby."
    "discovery%3AEXTRA_COMPANION_APP=com.bose.monet;end";
constexpr char kValidIntentURI[] =
    "intent:#Intent;action=com.google.android.gms.nearby."
    "discovery%3AACTION_MAGIC_PAIR;package=com.google.android."
    "gms;component=com.google.android.gms/.nearby.discovery."
    "service.DiscoveryService;end";
constexpr char kEmptyParameterIntentURI[] =
    "intent:#Intent;action=com.google.android.gms.nearby."
    "discovery%3AACTION_MAGIC_PAIR;package=com.google.android."
    "gms;component=com.google.android.gms/.nearby.discovery."
    "service.DiscoveryService;S.com.google.android.gms.nearby."
    "discovery%3AEXTRA_COMPANION_APP=com.bose.monet;;end";
constexpr char kEmptyIntentURI[] = "";
}  // namespace

namespace ash {
namespace quick_pair {

class CompanionAppParserUnitTest : public testing::Test {
 public:
  void SetUp() override {
    repository_ = std::make_unique<FakeFastPairRepository>();
    parser_ = std::make_unique<CompanionAppParser>();

    deviceWithCompanionApp_ = base::MakeRefCounted<Device>(
        kHasCompanionAppModelId, kTestDeviceAddress,
        Protocol::kFastPairInitial);
    deviceWithValidMetadata_ = base::MakeRefCounted<Device>(
        kValidMetadataModelId, kTestDeviceAddress, Protocol::kFastPairInitial);
    deviceWithEmptyParameterMetadata_ = base::MakeRefCounted<Device>(
        kEmptyParameterMetadataModelId, kTestDeviceAddress,
        Protocol::kFastPairInitial);
    deviceWithEmptyMetadata_ = base::MakeRefCounted<Device>(
        kEmptyMetadataModelId, kTestDeviceAddress, Protocol::kFastPairInitial);
    deviceWithNoMetadata_ = base::MakeRefCounted<Device>(
        kNoMetadataModelId, kTestDeviceAddress, Protocol::kFastPairInitial);

    metadataWithCompanionApp_.set_intent_uri(kValidIntentWithCompanionAppURI);
    emptyMetadata_.set_intent_uri(kEmptyIntentURI);
    validMetadata_.set_intent_uri(kValidIntentURI);
    emptyParameterMetadata_.set_intent_uri(kEmptyParameterIntentURI);
    repository_->SetFakeMetadata(kHasCompanionAppModelId,
                                 metadataWithCompanionApp_);
    repository_->SetFakeMetadata(kEmptyMetadataModelId, emptyMetadata_);
    repository_->SetFakeMetadata(kValidMetadataModelId, validMetadata_);
    repository_->SetFakeMetadata(kEmptyParameterMetadataModelId,
                                 emptyParameterMetadata_);
  }

 protected:
  base::test::SingleThreadTaskEnvironment task_enviornment_;
  std::unique_ptr<FakeFastPairRepository> repository_;
  std::unique_ptr<CompanionAppParser> parser_;
  nearby::fastpair::Device metadataWithCompanionApp_;
  nearby::fastpair::Device validMetadata_;
  nearby::fastpair::Device emptyParameterMetadata_;
  nearby::fastpair::Device emptyMetadata_;
  scoped_refptr<ash::quick_pair::Device> deviceWithCompanionApp_;
  scoped_refptr<ash::quick_pair::Device> deviceWithEmptyMetadata_;
  scoped_refptr<ash::quick_pair::Device> deviceWithValidMetadata_;
  scoped_refptr<ash::quick_pair::Device> deviceWithEmptyParameterMetadata_;
  scoped_refptr<ash::quick_pair::Device> deviceWithNoMetadata_;
};

TEST_F(CompanionAppParserUnitTest, MetadataContainsCompanionApp) {
  base::MockCallback<base::OnceCallback<void(std::optional<std::string>)>>
      on_companion_app_parsed;
  EXPECT_CALL(on_companion_app_parsed, Run(testing::Eq("com.bose.monet")))
      .Times(1);

  parser_->GetAppPackageName(deviceWithCompanionApp_,
                             on_companion_app_parsed.Get());
  base::RunLoop().RunUntilIdle();
}

TEST_F(CompanionAppParserUnitTest, MetadataIsEmpty) {
  base::MockCallback<base::OnceCallback<void(std::optional<std::string>)>>
      on_companion_app_parsed;
  EXPECT_CALL(on_companion_app_parsed, Run(testing::Eq(std::nullopt))).Times(1);

  parser_->GetAppPackageName(deviceWithEmptyMetadata_,
                             on_companion_app_parsed.Get());
  base::RunLoop().RunUntilIdle();
}

TEST_F(CompanionAppParserUnitTest, MetadataIsValid) {
  base::MockCallback<base::OnceCallback<void(std::optional<std::string>)>>
      on_companion_app_parsed;
  EXPECT_CALL(on_companion_app_parsed, Run(testing::Eq(std::nullopt))).Times(1);

  parser_->GetAppPackageName(deviceWithValidMetadata_,
                             on_companion_app_parsed.Get());
  base::RunLoop().RunUntilIdle();
}

TEST_F(CompanionAppParserUnitTest, MetadataHasEmptyParameter) {
  base::MockCallback<base::OnceCallback<void(std::optional<std::string>)>>
      on_companion_app_parsed;
  EXPECT_CALL(on_companion_app_parsed, Run(testing::Eq(std::nullopt))).Times(1);

  parser_->GetAppPackageName(deviceWithEmptyParameterMetadata_,
                             on_companion_app_parsed.Get());
  base::RunLoop().RunUntilIdle();
}

TEST_F(CompanionAppParserUnitTest, NoMetadata) {
  base::MockCallback<base::OnceCallback<void(std::optional<std::string>)>>
      on_companion_app_parsed;
  EXPECT_CALL(on_companion_app_parsed, Run(testing::Eq(std::nullopt))).Times(0);

  parser_->GetAppPackageName(deviceWithNoMetadata_,
                             on_companion_app_parsed.Get());
  base::RunLoop().RunUntilIdle();
}

}  // namespace quick_pair
}  // namespace ash