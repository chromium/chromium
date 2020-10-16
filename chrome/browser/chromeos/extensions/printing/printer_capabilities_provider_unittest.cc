// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/printing/printer_capabilities_provider.h"

#include "base/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "chrome/browser/chromeos/printing/test_cups_printers_manager.h"
#include "chrome/browser/chromeos/printing/test_printer_configurer.h"
#include "content/public/test/browser_task_environment.h"
#include "printing/backend/test_print_backend.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

namespace {

class PrinterCapabilitiesProviderPrintBackend
    : public printing::TestPrintBackend {
 public:
  PrinterCapabilitiesProviderPrintBackend() = default;

  // PrintBackend:
  bool GetPrinterSemanticCapsAndDefaults(
      const std::string& printer_name,
      printing::PrinterSemanticCapsAndDefaults* printer_info) override {
    capabilities_requests_counter_++;
    return printing::TestPrintBackend::GetPrinterSemanticCapsAndDefaults(
        printer_name, printer_info);
  }

  int capabilities_requests_counter() { return capabilities_requests_counter_; }

 private:
  ~PrinterCapabilitiesProviderPrintBackend() override = default;

  int capabilities_requests_counter_ = 0;
};

constexpr char kPrinterId[] = "printer";

}  // namespace

class PrinterCapabilitiesProviderTest : public testing::Test {
 public:
  PrinterCapabilitiesProviderTest() = default;
  ~PrinterCapabilitiesProviderTest() override = default;

  PrinterCapabilitiesProviderTest(const PrinterCapabilitiesProviderTest&) =
      delete;
  PrinterCapabilitiesProviderTest& operator=(
      const PrinterCapabilitiesProviderTest&) = delete;

  void SetUp() override {
    printers_manager_ = std::make_unique<chromeos::TestCupsPrintersManager>();
    auto printer_configurer =
        std::make_unique<chromeos::TestPrinterConfigurer>();
    printer_configurer_ = printer_configurer.get();
    test_backend_ =
        base::MakeRefCounted<PrinterCapabilitiesProviderPrintBackend>();
    printing::PrintBackend::SetPrintBackendForTesting(test_backend_.get());
    printer_capabilities_provider_ =
        std::make_unique<PrinterCapabilitiesProvider>(
            printers_manager_.get(), std::move(printer_configurer));
  }

  void TearDown() override {
    printer_capabilities_provider_.reset();
    test_backend_.reset();
    printers_manager_.reset();
  }

  void OnPrinterCapabilitiesRetrieved(
      base::RepeatingClosure run_loop_closure,
      base::Optional<printing::PrinterSemanticCapsAndDefaults> capabilities) {
    capabilities_ = std::move(capabilities);
    run_loop_closure.Run();
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  scoped_refptr<PrinterCapabilitiesProviderPrintBackend> test_backend_;
  std::unique_ptr<chromeos::TestCupsPrintersManager> printers_manager_;
  chromeos::TestPrinterConfigurer* printer_configurer_;
  std::unique_ptr<PrinterCapabilitiesProvider> printer_capabilities_provider_;
  base::Optional<printing::PrinterSemanticCapsAndDefaults> capabilities_;
};

// Tests that no capabilities are returned if the printer is not added to
// printers manager.
TEST_F(PrinterCapabilitiesProviderTest, GetPrinterInfo_NotAddedPrinter) {
  base::RunLoop run_loop;
  printer_capabilities_provider_->GetPrinterCapabilities(
      kPrinterId,
      base::BindOnce(
          &PrinterCapabilitiesProviderTest::OnPrinterCapabilitiesRetrieved,
          base::Unretained(this), run_loop.QuitClosure()));
  run_loop.Run();

  EXPECT_FALSE(capabilities_);
}

// Tests that no capabilities are returned if printer is unreachable from CUPS.
TEST_F(PrinterCapabilitiesProviderTest, GetPrinterInfo_NoCapabilities) {
  chromeos::Printer printer = chromeos::Printer(kPrinterId);
  printers_manager_->AddPrinter(printer, chromeos::PrinterClass::kEnterprise);

  base::RunLoop run_loop;
  printer_capabilities_provider_->GetPrinterCapabilities(
      kPrinterId,
      base::BindOnce(
          &PrinterCapabilitiesProviderTest::OnPrinterCapabilitiesRetrieved,
          base::Unretained(this), run_loop.QuitClosure()));
  run_loop.Run();

  // No capabilities should be returned since PrintBackend doesn't know anything
  // about the printer with given id.
  EXPECT_FALSE(capabilities_);
}

// Tests that correct capabilities are returned if the printer is added to
// printers manager but not installed yet.
TEST_F(PrinterCapabilitiesProviderTest, GetPrinterInfo_NotInstalledPrinter) {
  chromeos::Printer printer = chromeos::Printer(kPrinterId);
  printers_manager_->AddPrinter(printer, chromeos::PrinterClass::kEnterprise);

  auto capabilities =
      std::make_unique<printing::PrinterSemanticCapsAndDefaults>();
  capabilities->copies_max = 2;
  capabilities->color_changeable = true;
  // Add printer capabilities to |test_backend_|.
  test_backend_->AddValidPrinter(
      kPrinterId, std::move(capabilities),
      std::make_unique<printing::PrinterBasicInfo>());

  base::RunLoop run_loop;
  printer_capabilities_provider_->GetPrinterCapabilities(
      kPrinterId,
      base::BindOnce(
          &PrinterCapabilitiesProviderTest::OnPrinterCapabilitiesRetrieved,
          base::Unretained(this), run_loop.QuitClosure()));
  run_loop.Run();

  EXPECT_TRUE(printer_configurer_->IsConfigured(kPrinterId));
  ASSERT_TRUE(capabilities_);
  EXPECT_EQ(2, capabilities_->copies_max);
  EXPECT_TRUE(capabilities_->color_changeable);
  EXPECT_FALSE(capabilities_->color_default);
}

// Tests that correct capabilities are returned if the printer is added to
// printers manager and installed.
TEST_F(PrinterCapabilitiesProviderTest, GetPrinterInfo_InstalledPrinter) {
  chromeos::Printer printer = chromeos::Printer(kPrinterId);
  printers_manager_->AddPrinter(printer, chromeos::PrinterClass::kEnterprise);
  printers_manager_->InstallPrinter(kPrinterId);

  // Add printer capabilities to |test_backend_|.
  test_backend_->AddValidPrinter(
      kPrinterId, std::make_unique<printing::PrinterSemanticCapsAndDefaults>(),
      std::make_unique<printing::PrinterBasicInfo>());

  base::RunLoop run_loop;
  printer_capabilities_provider_->GetPrinterCapabilities(
      kPrinterId,
      base::BindOnce(
          &PrinterCapabilitiesProviderTest::OnPrinterCapabilitiesRetrieved,
          base::Unretained(this), run_loop.QuitClosure()));
  run_loop.Run();

  EXPECT_FALSE(printer_configurer_->IsConfigured(kPrinterId));
  ASSERT_TRUE(capabilities_);
  EXPECT_EQ(1, capabilities_->copies_max);
}

// Tests that capabilities are cached but not fetched after every
// PrinterCapabilitiesProvider request.
TEST_F(PrinterCapabilitiesProviderTest, GetPrinterInfo_CachedCapabilities) {
  chromeos::Printer printer = chromeos::Printer(kPrinterId);
  printers_manager_->AddPrinter(printer, chromeos::PrinterClass::kEnterprise);
  printers_manager_->InstallPrinter(kPrinterId);

  // Add printer capabilities to |test_backend_|.
  test_backend_->AddValidPrinter(
      kPrinterId, std::make_unique<printing::PrinterSemanticCapsAndDefaults>(),
      std::make_unique<printing::PrinterBasicInfo>());

  base::RunLoop run_loop;
  printer_capabilities_provider_->GetPrinterCapabilities(
      kPrinterId,
      base::BindOnce(
          &PrinterCapabilitiesProviderTest::OnPrinterCapabilitiesRetrieved,
          base::Unretained(this), run_loop.QuitClosure()));
  run_loop.Run();

  // GetPrinterSemanticCapsAndDefaults() should be called when we fetch printer
  // capabilities.
  EXPECT_EQ(1, test_backend_->capabilities_requests_counter());
  ASSERT_TRUE(capabilities_);
  EXPECT_EQ(1, capabilities_->copies_max);

  base::RunLoop cached_capabilities_run_loop;
  printer_capabilities_provider_->GetPrinterCapabilities(
      kPrinterId,
      base::BindOnce(
          &PrinterCapabilitiesProviderTest::OnPrinterCapabilitiesRetrieved,
          base::Unretained(this), cached_capabilities_run_loop.QuitClosure()));
  cached_capabilities_run_loop.Run();

  // GetPrinterSemanticCapsAndDefaults() shouldn't be called again.
  EXPECT_EQ(1, test_backend_->capabilities_requests_counter());
  ASSERT_TRUE(capabilities_);
  EXPECT_EQ(1, capabilities_->copies_max);
}

}  // namespace extensions
