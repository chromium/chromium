// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/printing/automatic_usb_printer_configurer.h"

#include <string>
#include <utility>
#include <vector>

#include "base/containers/flat_set.h"
#include "chrome/browser/ash/printing/cups_printers_manager.h"
#include "chrome/browser/ash/printing/printers_map.h"
#include "chrome/browser/ash/printing/test_printer_configurer.h"
#include "chrome/browser/ash/printing/usb_printer_notification_controller.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash {
namespace {

using ::chromeos::Printer;
using ::chromeos::PrinterClass;

Printer CreateUsbPrinter(const std::string& id) {
  Printer printer;
  printer.set_id(id);
  printer.SetUri("usb://usb/printer");
  return printer;
}

Printer CreateIppUsbPrinter(const std::string& id) {
  Printer printer;
  printer.set_id(id);
  printer.SetUri("ippusb://usb/printer");
  return printer;
}

Printer CreateIppPrinter(const std::string& id) {
  Printer printer;
  printer.set_id(id);
  printer.SetUri("ipp://usb/printer");
  return printer;
}

}  // namespace

// Provides a mechanism to interact with AutomaticPrinterConfigurer.
// Adding a printer causes the observer's OnPrintersChanged() method to run.
class FakeObservablePrintersManager {
 public:
  void SetObserver(CupsPrintersManager::Observer* observer) {
    DCHECK(observer);
    observer_ = observer;
  }

  // Simulates a nearby kAutomatic printer.
  void AddNearbyAutomaticPrinter(const Printer& printer) {
    AddPrinter(PrinterClass::kAutomatic, printer);
  }

  // Simulates a nearby kDiscovered printer.
  void AddNearbyDiscoveredPrinter(const Printer& printer) {
    AddPrinter(PrinterClass::kDiscovered, printer);
  }

  void RemoveAutomaticPrinter(const std::string& printer_id) {
    RemovePrinter(PrinterClass::kAutomatic, printer_id);
  }

  void RemoveDiscoveredPrinter(const std::string& printer_id) {
    RemovePrinter(PrinterClass::kDiscovered, printer_id);
  }

 private:
  // Add |printer| to the corresponding list in |printers_| bases on the given
  // |printer_class|.
  void AddPrinter(PrinterClass printer_class, const Printer& printer) {
    printers_.Insert(printer_class, printer);
    observer_->OnPrintersChanged(printer_class, printers_.Get(printer_class));
  }

  // Remove |printer_id| from |printer_class|.
  void RemovePrinter(PrinterClass printer_class,
                     const std::string& printer_id) {
    printers_.Remove(printer_class, printer_id);
    observer_->OnPrintersChanged(printer_class, printers_.Get(printer_class));
  }

  CupsPrintersManager::Observer* observer_;
  PrintersMap printers_;
};

class FakePrinterInstallationManager : public PrinterInstallationManager {
 public:
  FakePrinterInstallationManager() = default;
  ~FakePrinterInstallationManager() override = default;

  // CupsPrintersManager overrides
  void PrinterInstalled(const Printer& printer, bool is_automatic) override {
    DCHECK(is_automatic);

    installed_printers_.insert(printer.id());
  }

  bool IsPrinterInstalled(const Printer& printer) const override {
    return installed_printers_.contains(printer.id());
  }

  void PrinterIsNotAutoconfigurable(const Printer& printer) override {
    printers_marked_as_not_autoconf_.insert(printer.id());
  }

  bool IsMarkedAsNotAutoconfigurable(const Printer& printer) {
    return printers_marked_as_not_autoconf_.contains(printer.id());
  }

 private:
  base::flat_set<std::string> installed_printers_;
  base::flat_set<std::string> printers_marked_as_not_autoconf_;
};

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

class AutomaticUsbPrinterConfigurerTest : public testing::Test {
 public:
  AutomaticUsbPrinterConfigurerTest() {
    fake_installation_manager_ =
        std::make_unique<FakePrinterInstallationManager>();
    auto printer_configurer = std::make_unique<TestPrinterConfigurer>();
    fake_printer_configurer_ = printer_configurer.get();
    fake_notification_controller_ =
        std::make_unique<FakeUsbPrinterNotificationController>();

    auto_usb_printer_configurer_ =
        std::make_unique<AutomaticUsbPrinterConfigurer>(
            std::move(printer_configurer), fake_installation_manager_.get(),
            fake_notification_controller_.get());

    fake_observable_printers_manager_.SetObserver(
        auto_usb_printer_configurer_.get());
  }

  AutomaticUsbPrinterConfigurerTest(const AutomaticUsbPrinterConfigurerTest&) =
      delete;
  AutomaticUsbPrinterConfigurerTest& operator=(
      const AutomaticUsbPrinterConfigurerTest&) = delete;

  ~AutomaticUsbPrinterConfigurerTest() override = default;

 protected:
  FakeObservablePrintersManager fake_observable_printers_manager_;
  TestPrinterConfigurer* fake_printer_configurer_;  // not owned.
  std::unique_ptr<FakePrinterInstallationManager> fake_installation_manager_;
  std::unique_ptr<FakeUsbPrinterNotificationController>
      fake_notification_controller_;
  std::unique_ptr<AutomaticUsbPrinterConfigurer> auto_usb_printer_configurer_;
};

TEST_F(AutomaticUsbPrinterConfigurerTest,
       AutoUsbPrinterInstalledAutomatically) {
  const std::string printer_id = "id";
  const Printer printer = CreateUsbPrinter(printer_id);

  // Adding an automatic USB printer should result in the printer becoming
  // configured and getting marked as installed.
  fake_observable_printers_manager_.AddNearbyAutomaticPrinter(printer);

  EXPECT_TRUE(fake_installation_manager_->IsPrinterInstalled(printer));
  EXPECT_TRUE(fake_printer_configurer_->IsConfigured(printer_id));
}

TEST_F(AutomaticUsbPrinterConfigurerTest,
       AutoIppUsbPrinterInstalledAutomatically) {
  const std::string printer_id = "id";
  const Printer printer = CreateIppUsbPrinter(printer_id);

  // Adding an automatic IPPUSB printer should result in the printer becoming
  // configured and getting marked as installed.
  fake_observable_printers_manager_.AddNearbyAutomaticPrinter(printer);

  EXPECT_TRUE(fake_installation_manager_->IsPrinterInstalled(printer));
  EXPECT_TRUE(fake_printer_configurer_->IsConfigured(printer_id));
}

TEST_F(AutomaticUsbPrinterConfigurerTest, AutoIppPrinterNotInstalled) {
  const std::string printer_id = "id";
  const Printer printer = CreateIppPrinter(printer_id);

  // Adding an automatic Ipp printer should *not* result in the printer becoming
  // configured and getting marked as installed.
  fake_observable_printers_manager_.AddNearbyAutomaticPrinter(printer);

  EXPECT_FALSE(fake_installation_manager_->IsPrinterInstalled(printer));
  EXPECT_FALSE(fake_printer_configurer_->IsConfigured(printer_id));
}

TEST_F(AutomaticUsbPrinterConfigurerTest, DiscoveredUsbPrinterNotInstalled) {
  const std::string printer_id = "id";
  const Printer printer = CreateUsbPrinter(printer_id);

  // Adding a discovered USB printer should not result in the printer getting
  // installed.
  fake_observable_printers_manager_.AddNearbyDiscoveredPrinter(printer);

  EXPECT_FALSE(fake_installation_manager_->IsPrinterInstalled(printer));
  EXPECT_FALSE(fake_printer_configurer_->IsConfigured(printer_id));
}

TEST_F(AutomaticUsbPrinterConfigurerTest,
       ConfiguredAutoUsbPrinterGetsInstalled) {
  const std::string printer_id = "id";
  const Printer printer = CreateUsbPrinter(printer_id);

  fake_printer_configurer_->MarkConfigured(printer_id);

  // Adding an automatic USB printer that's already configured should still
  // result in the printer getting marked as installed.
  fake_observable_printers_manager_.AddNearbyAutomaticPrinter(printer);

  EXPECT_TRUE(fake_installation_manager_->IsPrinterInstalled(printer));
}

TEST_F(AutomaticUsbPrinterConfigurerTest, UsbPrinterAddedToSet) {
  const std::string automatic_printer_id = "auto_id";
  const Printer automatic_printer = CreateUsbPrinter(automatic_printer_id);

  const std::string discovered_printer_id = "disco_id";
  const Printer discovered_printer = CreateUsbPrinter(discovered_printer_id);

  // Adding an automatic USB printer should result in the printer getting added
  // to |auto_usb_printer_configurer_::configured_printers_|.
  fake_observable_printers_manager_.AddNearbyAutomaticPrinter(
      automatic_printer);

  EXPECT_TRUE(auto_usb_printer_configurer_->configured_printers_.contains(
      automatic_printer_id));

  // Adding a discovered USB printer should result in the printer getting added
  // to |auto_usb_printer_configurer_::unconfigured_printers_|.
  fake_observable_printers_manager_.AddNearbyDiscoveredPrinter(
      discovered_printer);

  EXPECT_TRUE(auto_usb_printer_configurer_->unconfigured_printers_.contains(
      discovered_printer_id));

  EXPECT_EQ(1u, auto_usb_printer_configurer_->configured_printers_.size());
  EXPECT_EQ(1u, auto_usb_printer_configurer_->unconfigured_printers_.size());

  // Removing the automatic printer should result in the printer getting
  // removed from |auto_usb_printer_configurer_::unconfigured_printers_|.
  fake_observable_printers_manager_.RemoveDiscoveredPrinter(
      discovered_printer_id);

  EXPECT_EQ(0u, auto_usb_printer_configurer_->unconfigured_printers_.size());

  // Removing the automatic printer should result in the printer getting
  // removed from |auto_usb_printer_configurer_::configured_printers_|.
  fake_observable_printers_manager_.RemoveAutomaticPrinter(
      automatic_printer_id);

  EXPECT_EQ(0u, auto_usb_printer_configurer_->configured_printers_.size());
}

TEST_F(AutomaticUsbPrinterConfigurerTest, NotificationOpenedForNewAutomatic) {
  const std::string printer_id = "id";
  const Printer printer = CreateUsbPrinter(printer_id);

  fake_observable_printers_manager_.AddNearbyAutomaticPrinter(printer);

  EXPECT_TRUE(
      fake_notification_controller_->IsNotificationDisplayed(printer_id));
}

TEST_F(AutomaticUsbPrinterConfigurerTest,
       NotificationOpenedForPreviouslyConfigured) {
  const std::string printer_id = "id";
  const Printer printer = CreateUsbPrinter(printer_id);

  // Mark the printer as configured.
  fake_printer_configurer_->MarkConfigured(printer_id);

  // Even though the printer is already configured, adding the printer should
  // result in a notification being shown.
  fake_observable_printers_manager_.AddNearbyAutomaticPrinter(printer);

  EXPECT_TRUE(
      fake_notification_controller_->IsNotificationDisplayed(printer_id));
}

TEST_F(AutomaticUsbPrinterConfigurerTest, NotificationClosed) {
  const std::string printer_id = "id";
  const Printer printer = CreateUsbPrinter(printer_id);

  fake_observable_printers_manager_.AddNearbyAutomaticPrinter(printer);

  EXPECT_TRUE(
      fake_notification_controller_->IsNotificationDisplayed(printer_id));

  fake_observable_printers_manager_.RemoveAutomaticPrinter(printer_id);

  EXPECT_FALSE(
      fake_notification_controller_->IsNotificationDisplayed(printer_id));
}

TEST_F(AutomaticUsbPrinterConfigurerTest, NotificationOpenedForNewDiscovered) {
  const std::string printer_id = "id";
  const Printer printer = CreateUsbPrinter(printer_id);

  fake_observable_printers_manager_.AddNearbyAutomaticPrinter(printer);

  EXPECT_TRUE(
      fake_notification_controller_->IsNotificationDisplayed(printer_id));
}

TEST_F(AutomaticUsbPrinterConfigurerTest, RegisterAutoconfFailureForIppUsb) {
  const std::string printer1_id = "id1";
  const std::string printer2_id = "id2";
  const Printer printer1 = CreateIppUsbPrinter(printer1_id);
  const Printer printer2 = CreateIppUsbPrinter(printer2_id);

  fake_printer_configurer_->AssignPrinterSetupResult(
      printer1_id, PrinterSetupResult::kPrinterIsNotAutoconfigurable);

  fake_observable_printers_manager_.AddNearbyAutomaticPrinter(printer1);
  fake_observable_printers_manager_.AddNearbyAutomaticPrinter(printer2);

  EXPECT_TRUE(
      fake_installation_manager_->IsMarkedAsNotAutoconfigurable(printer1));
  EXPECT_FALSE(
      fake_installation_manager_->IsMarkedAsNotAutoconfigurable(printer2));
}

}  // namespace ash
