// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/printing/enterprise/enterprise_printers_provider.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/to_vector.h"
#include "base/memory/weak_ptr.h"
#include "base/ranges/algorithm.h"
#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "base/values.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/ash/printing/enterprise/bulk_printers_calculator.h"
#include "chrome/browser/ash/printing/enterprise/bulk_printers_calculator_factory.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/components/settings/cros_settings.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "chromeos/printing/printer_configuration.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/user_manager/scoped_user_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace {

using ::chromeos::Printer;
using ::testing::UnorderedElementsAre;

std::vector<std::string> GetPrinterIds(const std::vector<Printer>& printers) {
  return base::ToVector(printers, &Printer::id);
}

base::Value::List StringsToValueList(const std::vector<std::string>& strings) {
  base::Value::List list;
  list.reserve(strings.size());
  for (const std::string& s : strings) {
    list.Append(s);
  }
  return list;
}

constexpr char kFirstId[] = "First";
constexpr char kSecondId[] = "Second";
constexpr char kThirdId[] = "Third";

constexpr char kRecommendedPrinter[] = R"json(
  {
    "id": "First",
    "display_name": "LexaPrint",
    "description": "Laser on the test shelf",
    "manufacturer": "LexaPrint, Inc.",
    "model": "MS610de",
    "uri": "ipp://192.168.1.5",
    "ppd_resource": {
      "effective_model": "MS610de"
    }
  }
)json";

// Bulk printer configuration file.
constexpr char kBulkPolicyContents[] = R"json(
[
  {
    "guid": "First",
    "display_name": "LexaPrint",
    "description": "Laser on the test shelf",
    "uri": "ipp://192.168.1.5",
    "ppd_resource": {
      "effective_model": "MS610de"
    }
  }, {
    "guid": "Incorrect uri",
    "display_name": "aaa",
    "description": "bbbb",
    "uri":"ipp://:",
    "ppd_resource":{
      "effective_model": "fff"
    }
  }, {
    "guid": "Second",
    "display_name": "Color Laser",
    "description": "The printer next to the water cooler.",
    "uri":"ipps://print-server.intranet.example.com:443/ipp/cl2k4",
    "ppd_resource":{
      "effective_model": "ColorLaser2k4"
    }
  }, {
    "guid": "Third",
    "display_name": "YaLP",
    "description": "Fancy Fancy Fancy",
    "uri": "ipp://192.168.1.8",
    "ppd_resource": {
      "effective_model": "MS610de"
    }
  }
])json";

// Helper class to record observed events.
class LoggingObserver : public EnterprisePrintersProvider::Observer {
 public:
  explicit LoggingObserver(EnterprisePrintersProvider* source) {
    observation_.Observe(source);
  }

  void OnPrintersChanged(bool complete,
                         const std::vector<Printer>& printers) override {
    complete_ = complete;
    printers_ = printers;
  }

  const std::vector<Printer>& printers() const { return printers_; }

 private:
  bool complete_ = false;
  std::vector<Printer> printers_;
  base::ScopedObservation<EnterprisePrintersProvider,
                          EnterprisePrintersProvider::Observer>
      observation_{this};
};

class EnterprisePrintersProviderTest : public testing::Test {
 protected:
  EnterprisePrintersProviderTest() {
    const AccountId kUserAccountId =
        AccountId::FromUserEmail(TestingProfile::kDefaultProfileUserName);
    fake_user_manager_->AddUser(kUserAccountId);
    fake_user_manager_->SwitchActiveUser(kUserAccountId);
  }

  void SetUp() override {
    device_printers_calculator_ = bulk_factory.GetForDevice();
    ASSERT_TRUE(device_printers_calculator_);
    user_printers_calculator_ = bulk_factory.GetForAccountId(
        AccountId::FromUserEmail(TestingProfile::kDefaultProfileUserName));
    ASSERT_TRUE(user_printers_calculator_);

    prefs_ = profile_.GetTestingPrefService();
    cros_settings_ = profile_.ScopedCrosSettingsTestHelper();
    cros_settings_->ReplaceDeviceSettingsProviderWithStub();

    // Explicitly set empty allowlist to disable all printers. Individual test
    // cases will override this.
    prefs_->SetManagedPref(prefs::kRecommendedPrintersAllowlist,
                           base::Value::List());
    cros_settings_->Set(kDevicePrintersAllowlist, base::Value());

    prefs_->SetManagedPref(
        prefs::kRecommendedPrintersAccessMode,
        base::Value(BulkPrintersCalculator::AccessMode::ALLOWLIST_ONLY));
    cros_settings_->Set(
        kDevicePrintersAccessMode,
        base::Value(BulkPrintersCalculator::AccessMode::ALLOWLIST_ONLY));

    device_printers_calculator_->SetData(
        std::make_unique<std::string>(kBulkPolicyContents));
    user_printers_calculator_->SetData(
        std::make_unique<std::string>(kBulkPolicyContents));

    provider_ =
        EnterprisePrintersProvider::Create(CrosSettings::Get(), &profile_);
    base::RunLoop().RunUntilIdle();
  }

  void SetRecommendedPrinters(const std::vector<std::string>& printer_jsons) {
    prefs_->SetManagedPref(prefs::kRecommendedPrinters,
                           StringsToValueList(printer_jsons));
    task_environment_.RunUntilIdle();
  }

  void SetDevicePolicyPrinters(
      const std::vector<std::string>& allowed_printer_ids) {
    profile_.ScopedCrosSettingsTestHelper()->Set(
        kDevicePrintersAllowlist,
        base::Value(StringsToValueList(allowed_printer_ids)));
    task_environment_.RunUntilIdle();
  }

  void SetUserPolicyPrinters(
      const std::vector<std::string>& allowed_printer_ids) {
    prefs_->SetManagedPref(prefs::kRecommendedPrintersAllowlist,
                           StringsToValueList(allowed_printer_ids));

    task_environment_.RunUntilIdle();
  }

  // Must outlive |profile_|.
  content::BrowserTaskEnvironment task_environment_;

  TestingProfile profile_;

  raw_ptr<ash::ScopedCrosSettingsTestHelper> cros_settings_;

  raw_ptr<sync_preferences::TestingPrefServiceSyncable> prefs_;

  // Must outlive |provider_|.
  user_manager::TypedScopedUserManager<ash::FakeChromeUserManager>
      fake_user_manager_{std::make_unique<ash::FakeChromeUserManager>()};

  BulkPrintersCalculatorFactory bulk_factory;
  base::WeakPtr<BulkPrintersCalculator> user_printers_calculator_;
  base::WeakPtr<BulkPrintersCalculator> device_printers_calculator_;
  std::unique_ptr<EnterprisePrintersProvider> provider_;
};

TEST_F(EnterprisePrintersProviderTest, SingleUserPrinter) {
  LoggingObserver observer(provider_.get());
  SetUserPolicyPrinters({kFirstId});
  EXPECT_THAT(GetPrinterIds(observer.printers()),
              UnorderedElementsAre(kFirstId));
}

TEST_F(EnterprisePrintersProviderTest, SingleDevicePrinter) {
  LoggingObserver observer(provider_.get());
  SetDevicePolicyPrinters({kThirdId});
  EXPECT_THAT(GetPrinterIds(observer.printers()),
              UnorderedElementsAre(kThirdId));
}

TEST_F(EnterprisePrintersProviderTest, RecommendedPrinters) {
  LoggingObserver observer(provider_.get());
  SetRecommendedPrinters({kRecommendedPrinter});

  ASSERT_EQ(1u, observer.printers().size());
  // Printer IDs are generated for recommended printers, so we check the display
  // name in this test.
  EXPECT_EQ("LexaPrint", observer.printers().front().display_name());
}

TEST_F(EnterprisePrintersProviderTest, MultiplePrinters) {
  LoggingObserver observer(provider_.get());
  SetDevicePolicyPrinters({kFirstId, kSecondId});
  EXPECT_THAT(GetPrinterIds(observer.printers()),
              UnorderedElementsAre(kFirstId, kSecondId));
}

TEST_F(EnterprisePrintersProviderTest, ChangingEnterprisePrinter) {
  LoggingObserver observer(provider_.get());
  SetUserPolicyPrinters({kFirstId});
  EXPECT_THAT(GetPrinterIds(observer.printers()),
              UnorderedElementsAre(kFirstId));

  SetUserPolicyPrinters({kSecondId});
  EXPECT_THAT(GetPrinterIds(observer.printers()),
              UnorderedElementsAre(kSecondId));
}

TEST_F(EnterprisePrintersProviderTest, PrintersMergedWithNoDuplicates) {
  LoggingObserver observer(provider_.get());
  SetUserPolicyPrinters({kFirstId, kSecondId});
  SetDevicePolicyPrinters({kSecondId, kThirdId});
  EXPECT_THAT(GetPrinterIds(observer.printers()),
              UnorderedElementsAre(kFirstId, kSecondId, kThirdId));
}

TEST_F(EnterprisePrintersProviderTest, PrinterSourceIsPolicy) {
  LoggingObserver observer(provider_.get());
  SetDevicePolicyPrinters({kFirstId});
  SetUserPolicyPrinters({kSecondId});

  std::vector<Printer> printers = observer.printers();
  ASSERT_EQ(2u, printers.size());
  EXPECT_THAT(printers[0].source(), Printer::SRC_POLICY);
  EXPECT_THAT(printers[1].source(), Printer::SRC_POLICY);
}

}  // namespace
}  // namespace ash
