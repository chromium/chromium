// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/eche_app_ui/eche_uid_provider.h"

#include <base/base64.h>

#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace eche_app {

class Callback {
 public:
  static void GetUidCallback(const std::string& uid) { uid_ = uid; }
  static std::string GetUid() { return uid_; }
  static void ResetUid() { uid_ = ""; }

 private:
  static std::string uid_;
};

std::string ash::eche_app::Callback::uid_ = "";

class EcheUidProviderTest : public testing::Test {
 protected:
  EcheUidProviderTest() = default;
  EcheUidProviderTest(const EcheUidProviderTest&) = delete;
  EcheUidProviderTest& operator=(const EcheUidProviderTest&) = delete;
  ~EcheUidProviderTest() override = default;

  // testing::Test:
  void SetUp() override {
    pref_service_.registry()->RegisterStringPref(kEcheAppSeedPref, "");
    uid_provider_ = std::make_unique<EcheUidProvider>(&pref_service_);
  }
  void TearDown() override {
    uid_provider_.reset();
    Callback::ResetUid();
  }
  void ResetPrefString(const std::string& path, const std::string& value) {
    pref_service_.SetString(path, value);
    uid_provider_.reset();
    uid_provider_ = std::make_unique<EcheUidProvider>(&pref_service_);
  }
  void ResetUidProvider() {
    uid_provider_.reset();
    uid_provider_ = std::make_unique<EcheUidProvider>(&pref_service_);
  }
  void GetUid() {
    uid_provider_->GetUid(base::BindOnce(&Callback::GetUidCallback));
  }

 private:
  TestingPrefServiceSimple pref_service_;
  std::unique_ptr<EcheUidProvider> uid_provider_;
};

TEST_F(EcheUidProviderTest, GetUidHasValue) {
  GetUid();
  EXPECT_NE(Callback::GetUid(), "");
}

TEST_F(EcheUidProviderTest, GetUidFromCacheShouldBeTheSameOne) {
  GetUid();
  std::string uid = Callback::GetUid();
  GetUid();
  EXPECT_EQ(Callback::GetUid(), uid);
}

TEST_F(EcheUidProviderTest, GetUidFromPrefShouldBeTheSameOne) {
  GetUid();
  std::string uid = Callback::GetUid();
  ResetUidProvider();
  GetUid();
  EXPECT_EQ(Callback::GetUid(), uid);
}

TEST_F(EcheUidProviderTest, GetUidWithWrongKeyShouldNotBeTheSame) {
  GetUid();
  std::string uid = Callback::GetUid();
  ResetPrefString(kEcheAppSeedPref, "wrong seed");
  GetUid();
  EXPECT_NE(Callback::GetUid(), uid);
}

}  // namespace eche_app
}  // namespace ash
