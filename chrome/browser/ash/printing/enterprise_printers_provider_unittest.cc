// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/printing/enterprise_printers_provider.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "base/debug/dump_without_crashing.h"
#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "chrome/browser/ash/printing/bulk_printers_calculator_factory.h"
#include "chrome/browser/ash/printing/printers_sync_bridge.h"
#include "chrome/browser/ash/printing/synced_printers_manager_factory.h"
#include "chrome/browser/ash/settings/cros_settings.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "components/sync/model/model_type_store.h"
#include "components/sync/test/model_type_store_test_util.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash {

namespace {

using ::chromeos::Printer;

constexpr char kLexJson[] = R"({
        "display_name": "LexaPrint",
        "description": "Laser on the test shelf",
        "manufacturer": "LexaPrint, Inc.",
        "model": "MS610de",
        "uri": "ipp://192.168.1.5",
        "ppd_resource": {
          "effective_manufacturer": "LexaPrint",
          "effective_model": "MS610de",
        },
      } )";

constexpr char kColorLaserJson[] = R"json({
      "display_name": "Color Laser",
      "description": "The printer next to the water cooler.",
      "manufacturer": "Printer Manufacturer",
      "model":"Color Laser 2004",
      "uri":"ipps://print-server.intranet.example.com:443/ipp/cl2k4",
      "uuid":"1c395fdb-5d93-4904-b246-b2c046e79d12",
      "ppd_resource":{
          "effective_manufacturer": "MakesPrinters",
          "effective_model":"ColorLaser2k4"
       }
      })json";

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
  EnterprisePrintersProviderTest()
      : provider_(EnterprisePrintersProvider::Create(CrosSettings::Get(),
                                                     &profile_)) {
    base::RunLoop().RunUntilIdle();
  }

  void SetPolicyPrinters(std::vector<std::string> printer_json_blobs) {
    base::Value::List value;
    for (std::string& blob : printer_json_blobs)
      value.Append(std::move(blob));

    sync_preferences::TestingPrefServiceSyncable* prefs =
        profile_.GetTestingPrefService();
    // TestingPrefSyncableService assumes ownership of |value|.
    prefs->SetManagedPref(prefs::kRecommendedPrinters,
                          base::Value(std::move(value)));
  }

  // Must outlive |profile_|.
  content::BrowserTaskEnvironment task_environment_;

  // Must outlive |manager_|.
  TestingProfile profile_;

  BulkPrintersCalculatorFactory bulk_factory;
  std::unique_ptr<EnterprisePrintersProvider> provider_;
};

TEST_F(EnterprisePrintersProviderTest, SinglePrinter) {
  LoggingObserver observer(provider_.get());
  SetPolicyPrinters({kLexJson});

  const Printer& from_list = observer.printers().front();
  EXPECT_EQ("LexaPrint", from_list.display_name());
  EXPECT_EQ(Printer::Source::SRC_POLICY, from_list.source());
}

TEST_F(EnterprisePrintersProviderTest, MultiplePrinters) {
  LoggingObserver observer(provider_.get());
  SetPolicyPrinters({kLexJson, kColorLaserJson});
  ASSERT_EQ(2U, observer.printers().size());

  const Printer& from_list = observer.printers().front();
  EXPECT_EQ(Printer::Source::SRC_POLICY, from_list.source());
}

TEST_F(EnterprisePrintersProviderTest, ChangingEnterprisePrinter) {
  LoggingObserver observer(provider_.get());
  SetPolicyPrinters({kLexJson});

  const Printer& from_list = observer.printers().front();
  EXPECT_EQ("LexaPrint", from_list.display_name());
  EXPECT_EQ(Printer::Source::SRC_POLICY, from_list.source());

  SetPolicyPrinters({kColorLaserJson});

  const Printer& from_list2 = observer.printers().front();
  EXPECT_EQ("Color Laser", from_list2.display_name());
  EXPECT_EQ(Printer::Source::SRC_POLICY, from_list2.source());
}

}  // namespace
}  // namespace ash
