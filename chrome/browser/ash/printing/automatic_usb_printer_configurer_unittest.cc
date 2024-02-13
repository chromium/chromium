// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/printing/automatic_usb_printer_configurer.h"

#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "ash/constants/ash_features.h"
#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/printing/fake_cups_printers_manager.h"
#include "chrome/browser/ash/printing/printers_map.h"
#include "chrome/browser/ash/printing/usb_printer_notification_controller.h"
#include "chromeos/printing/ppd_provider.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace {

using ::chromeos::Printer;
using ::chromeos::PrinterClass;

PrinterDetector::DetectedPrinter CreateUsbPrinter(
    const std::string& id,
    const std::string& make_and_model) {
  PrinterDetector::DetectedPrinter detected;
  detected.printer.set_id(id);
  detected.printer.SetUri("usb://usb/printer");
  detected.printer.set_supports_ippusb(false);
  detected.ppd_search_data.make_and_model.push_back(make_and_model);
  return detected;
}

PrinterDetector::DetectedPrinter CreateIppUsbPrinter(
    const std::string& id,
    const std::string& make_and_model) {
  PrinterDetector::DetectedPrinter detected;
  detected.printer.set_id(id);
  detected.printer.SetUri("usb://usb/printer");
  detected.printer.set_supports_ippusb(true);
  detected.ppd_search_data.make_and_model.push_back(make_and_model);
  return detected;
}

}  // namespace

class FakeUsbPrinterNotificationController
    : public UsbPrinterNotificationController {
 public:
  FakeUsbPrinterNotificationController() = default;
  ~FakeUsbPrinterNotificationController() override = default;

  void ShowEphemeralNotification(const Printer& printer) override {
    open_notifications_.insert(printer.id());
  }

  void ShowSavedNotification(const Printer& printer) override {
    NOTIMPLEMENTED();
  }

  void ShowConfigurationNotification(const Printer& printer) override {
    open_notifications_.insert(printer.id());
  }

  void RemoveNotification(const std::string& printer_id) override {
    open_notifications_.erase(printer_id);
  }

  bool IsNotificationDisplayed(const std::string& printer_id) const override {
    return open_notifications_.contains(printer_id);
  }

 private:
  base::flat_set<std::string> open_notifications_;
};

// Fake PpdProvider backend.
class FakePpdProvider : public chromeos::PpdProvider {
 public:
  FakePpdProvider() = default;

  void SetPpd(const std::string& make_and_model,
              const std::string& effective_make_and_model) {
    ppds_[make_and_model] = effective_make_and_model;
  }

  void ResolvePpdReference(const chromeos::PrinterSearchData& search_data,
                           ResolvePpdReferenceCallback cb) override {
    chromeos::PpdProvider::CallbackResultCode code =
        chromeos::PpdProvider::NOT_FOUND;
    Printer::PpdReference ret;

    for (const std::string& make_and_model : search_data.make_and_model) {
      auto it = ppds_.find(make_and_model);
      if (it != ppds_.end()) {
        code = chromeos::PpdProvider::SUCCESS;
        ret.effective_make_and_model = it->second;
        break;
      }
    }

    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(cb), code, ret, /*usb_manufacturer=*/""));
  }

  void ResolvePpd(const chromeos::Printer::PpdReference& reference,
                  ResolvePpdCallback cb) override {
    for (auto kv : ppds_) {
      if (kv.second == reference.effective_make_and_model) {
        std::move(cb).Run(chromeos::PpdProvider::CallbackResultCode::SUCCESS,
                          "ppd content");
        return;
      }
    }
    std::move(cb).Run(chromeos::PpdProvider::CallbackResultCode::NOT_FOUND, "");
  }

  // These methods are not used by AutomaticUsbPrinterConfigurer.
  void ResolveManufacturers(ResolveManufacturersCallback cb) override {}
  void ResolvePrinters(const std::string& manufacturer,
                       ResolvePrintersCallback cb) override {}
  void ResolvePpdLicense(std::string_view effective_make_and_model,
                         ResolvePpdLicenseCallback cb) override {}
  void ReverseLookup(const std::string& effective_make_and_model,
                     ReverseLookupCallback cb) override {}

 private:
  ~FakePpdProvider() override {}
  base::flat_map<std::string, std::string> ppds_;
};

class AutomaticUsbPrinterConfigurerTest : public testing::TestWithParam<bool> {
 public:
  AutomaticUsbPrinterConfigurerTest() {
    if (GetParam()) {
      scoped_feature_list_.InitWithFeatures(
          {features::kIppFirstSetupForUsbPrinters}, {});
    } else {
      scoped_feature_list_.InitWithFeatures(
          {}, {features::kIppFirstSetupForUsbPrinters});
    }
  }

  AutomaticUsbPrinterConfigurerTest(const AutomaticUsbPrinterConfigurerTest&) =
      delete;
  AutomaticUsbPrinterConfigurerTest& operator=(
      const AutomaticUsbPrinterConfigurerTest&) = delete;

  ~AutomaticUsbPrinterConfigurerTest() override = default;

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<FakeCupsPrintersManager> fake_installation_manager_ =
      std::make_unique<FakeCupsPrintersManager>();
  std::unique_ptr<FakeUsbPrinterNotificationController>
      fake_notification_controller_ =
          std::make_unique<FakeUsbPrinterNotificationController>();
  scoped_refptr<FakePpdProvider> fake_ppd_provider_ =
      base::MakeRefCounted<FakePpdProvider>();
  std::unique_ptr<AutomaticUsbPrinterConfigurer> auto_usb_printer_configurer_ =
      std::make_unique<AutomaticUsbPrinterConfigurer>(
          fake_installation_manager_.get(),
          fake_notification_controller_.get(),
          fake_ppd_provider_.get(),
          base::DoNothing());
};

TEST_P(AutomaticUsbPrinterConfigurerTest,
       AutoUsbPrinterInstalledAutomatically) {
  const std::string printer_id = "id";
  const PrinterDetector::DetectedPrinter printer =
      CreateUsbPrinter(printer_id, "make-and-model");

  // Adding a USB printer should result in the printer becoming
  // configured and getting marked as installed.
  fake_ppd_provider_->SetPpd("make-and-model", "effective-make-and-model");
  auto_usb_printer_configurer_->UpdateListOfConnectedPrinters({printer});
  task_environment_.RunUntilIdle();

  EXPECT_TRUE(fake_installation_manager_->IsPrinterInstalled(printer.printer));
  ASSERT_TRUE(auto_usb_printer_configurer_->ConfiguredPrintersIds().contains(
      printer_id));
  const chromeos::Printer::PpdReference& ppd_ref =
      auto_usb_printer_configurer_->Printer(printer_id).ppd_reference();
  EXPECT_EQ(ppd_ref.effective_make_and_model, "effective-make-and-model");
  EXPECT_TRUE(ppd_ref.user_supplied_ppd_url.empty());
  EXPECT_FALSE(ppd_ref.autoconf);
}

TEST_P(AutomaticUsbPrinterConfigurerTest,
       AutoIppUsbPrinterInstalledAutomatically) {
  const std::string printer_id = "id";
  const PrinterDetector::DetectedPrinter printer =
      CreateIppUsbPrinter(printer_id, "make-and-model");

  // Adding an automatic IPPUSB printer should result in the printer becoming
  // configured and getting marked as installed.
  auto_usb_printer_configurer_->UpdateListOfConnectedPrinters({printer});
  task_environment_.RunUntilIdle();

  EXPECT_TRUE(fake_installation_manager_->IsPrinterInstalled(printer.printer));
  ASSERT_TRUE(auto_usb_printer_configurer_->ConfiguredPrintersIds().contains(
      printer_id));
  const chromeos::Printer::PpdReference& ppd_ref =
      auto_usb_printer_configurer_->Printer(printer_id).ppd_reference();
  EXPECT_TRUE(ppd_ref.effective_make_and_model.empty());
  EXPECT_TRUE(ppd_ref.user_supplied_ppd_url.empty());
  EXPECT_TRUE(ppd_ref.autoconf);
}

TEST_P(AutomaticUsbPrinterConfigurerTest, DiscoveredUsbPrinterNotInstalled) {
  const std::string printer_id = "id";
  const PrinterDetector::DetectedPrinter printer =
      CreateUsbPrinter(printer_id, "make-and-model");

  // Adding a discovered USB printer should not result in the printer getting
  // installed.
  auto_usb_printer_configurer_->UpdateListOfConnectedPrinters({printer});
  task_environment_.RunUntilIdle();

  EXPECT_FALSE(fake_installation_manager_->IsPrinterInstalled(printer.printer));
}

TEST_P(AutomaticUsbPrinterConfigurerTest, UsbPrinterAddedToSet) {
  const std::string automatic_printer_id = "auto_id";
  const PrinterDetector::DetectedPrinter automatic_printer =
      CreateIppUsbPrinter(automatic_printer_id, "");

  const std::string discovered_printer_id = "disco_id";
  const PrinterDetector::DetectedPrinter discovered_printer =
      CreateUsbPrinter(discovered_printer_id, "");

  // Adding an IPP USB printer should result in the printer getting added
  // to the list of configured printers.
  auto_usb_printer_configurer_->UpdateListOfConnectedPrinters(
      {automatic_printer});
  task_environment_.RunUntilIdle();

  EXPECT_TRUE(auto_usb_printer_configurer_->ConfiguredPrintersIds().contains(
      automatic_printer_id));
  EXPECT_TRUE(auto_usb_printer_configurer_->UnconfiguredPrintersIds().empty());

  // Adding a non-IPP USB printer should result in the printer getting added
  // to the list of unconfigured printers (no PPDs are available).
  auto_usb_printer_configurer_->UpdateListOfConnectedPrinters(
      {automatic_printer, discovered_printer});
  task_environment_.RunUntilIdle();

  EXPECT_TRUE(auto_usb_printer_configurer_->ConfiguredPrintersIds().contains(
      automatic_printer_id));
  EXPECT_TRUE(auto_usb_printer_configurer_->UnconfiguredPrintersIds().contains(
      discovered_printer_id));

  EXPECT_EQ(1u, auto_usb_printer_configurer_->ConfiguredPrintersIds().size());
  EXPECT_EQ(1u, auto_usb_printer_configurer_->UnconfiguredPrintersIds().size());

  // Removing the non-IPP printer should result in the printer getting
  // removed from the list of unconfigured printers.
  auto_usb_printer_configurer_->UpdateListOfConnectedPrinters(
      {automatic_printer});
  task_environment_.RunUntilIdle();

  EXPECT_EQ(0u, auto_usb_printer_configurer_->UnconfiguredPrintersIds().size());

  // Removing the IPP printer should result in the printer getting
  // removed from the list of configured printers.
  auto_usb_printer_configurer_->UpdateListOfConnectedPrinters({});
  task_environment_.RunUntilIdle();

  EXPECT_EQ(0u, auto_usb_printer_configurer_->ConfiguredPrintersIds().size());
}

TEST_P(AutomaticUsbPrinterConfigurerTest, NotificationOpenedForNewAutomatic) {
  const std::string printer_id = "id";
  const PrinterDetector::DetectedPrinter printer =
      CreateIppUsbPrinter(printer_id, "");

  auto_usb_printer_configurer_->UpdateListOfConnectedPrinters({printer});
  task_environment_.RunUntilIdle();

  EXPECT_TRUE(
      fake_notification_controller_->IsNotificationDisplayed(printer_id));
}

TEST_P(AutomaticUsbPrinterConfigurerTest, NotificationClosed) {
  const std::string printer_id = "id";
  const PrinterDetector::DetectedPrinter printer =
      CreateIppUsbPrinter(printer_id, "");

  auto_usb_printer_configurer_->UpdateListOfConnectedPrinters({printer});
  task_environment_.RunUntilIdle();

  EXPECT_TRUE(
      fake_notification_controller_->IsNotificationDisplayed(printer_id));

  auto_usb_printer_configurer_->UpdateListOfConnectedPrinters({});
  task_environment_.RunUntilIdle();

  EXPECT_FALSE(
      fake_notification_controller_->IsNotificationDisplayed(printer_id));
}

TEST_P(AutomaticUsbPrinterConfigurerTest, NotificationOpenedForNewDiscovered) {
  const std::string printer_id = "id";
  const PrinterDetector::DetectedPrinter printer =
      CreateUsbPrinter(printer_id, "");

  auto_usb_printer_configurer_->UpdateListOfConnectedPrinters({printer});
  task_environment_.RunUntilIdle();

  EXPECT_TRUE(
      fake_notification_controller_->IsNotificationDisplayed(printer_id));
}

TEST_P(AutomaticUsbPrinterConfigurerTest, IppUsbPrinterWithPpdDependsOnFlag) {
  const std::string printer_id = "id";
  const PrinterDetector::DetectedPrinter printer =
      CreateIppUsbPrinter(printer_id, "Make & Model");
  fake_ppd_provider_->SetPpd("Make & Model", "make-and-model");

  auto_usb_printer_configurer_->UpdateListOfConnectedPrinters({printer});
  task_environment_.RunUntilIdle();

  EXPECT_TRUE(fake_installation_manager_->IsPrinterInstalled(printer.printer));
  ASSERT_TRUE(auto_usb_printer_configurer_->ConfiguredPrintersIds().contains(
      printer_id));

  const chromeos::Printer::PpdReference& ppd_ref =
      auto_usb_printer_configurer_->Printer(printer_id).ppd_reference();
  EXPECT_TRUE(ppd_ref.user_supplied_ppd_url.empty());
  if (GetParam()) {
    // The printer should be set up as IPP Everywhere printer.
    EXPECT_TRUE(ppd_ref.effective_make_and_model.empty());
    EXPECT_TRUE(ppd_ref.autoconf);
  } else {
    // The printer should be set up via PPD file.
    EXPECT_EQ(ppd_ref.effective_make_and_model, "make-and-model");
    EXPECT_FALSE(ppd_ref.autoconf);
  }
}

INSTANTIATE_TEST_SUITE_P(IppFirstSetupForUsbPrinters,
                         AutomaticUsbPrinterConfigurerTest,
                         testing::Values(false, true));

}  // namespace ash
