// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/local_printer_ash.h"

#include <algorithm>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/memory/ref_counted.h"
#include "base/strings/string_piece.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "chrome/browser/ash/crosapi/test_local_printer_ash.h"
#include "chrome/browser/chromeos/printing/cups_printers_manager_factory.h"
#include "chrome/browser/chromeos/printing/test_cups_printers_manager.h"
#include "chrome/browser/chromeos/printing/test_printer_configurer.h"
#include "chrome/browser/printing/print_backend_service_manager.h"
#include "chrome/browser/printing/print_backend_service_test_impl.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/printing/printer_capabilities.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/crosapi/mojom/local_printer.mojom.h"
#include "chromeos/printing/cups_printer_status.h"
#include "chromeos/printing/ppd_provider.h"
#include "content/public/browser/browser_context.h"
#include "content/public/test/browser_task_environment.h"
#include "printing/backend/print_backend.h"
#include "printing/backend/printing_restrictions.h"
#include "printing/backend/test_print_backend.h"
#include "printing/printing_features.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using chromeos::CupsPrintersManager;
using chromeos::Printer;
using chromeos::PrinterClass;
using chromeos::PrinterConfigurer;
using chromeos::PrinterSetupCallback;
using chromeos::PrinterSetupResult;

namespace printing {

namespace {

// Used as a callback to `GetPrinters()` in tests.
// Records list returned by `GetPrinters()`.
void RecordPrinterList(
    std::vector<crosapi::mojom::LocalDestinationInfoPtr>& printers_out,
    std::vector<crosapi::mojom::LocalDestinationInfoPtr> printers) {
  printers_out = std::move(printers);
}

void RecordGetCapability(
    crosapi::mojom::CapabilitiesResponsePtr& capabilities_out,
    crosapi::mojom::CapabilitiesResponsePtr capability) {
  capabilities_out = std::move(capability);
}

void RecordGetEulaUrl(GURL& fetched_eula_url, const GURL& eula_url) {
  fetched_eula_url = eula_url;
}

Printer CreateTestPrinter(const std::string& id,
                          const std::string& name,
                          const std::string& description) {
  Printer printer;
  printer.SetUri("http://localhost");
  printer.set_id(id);
  printer.set_display_name(name);
  printer.set_description(description);
  return printer;
}

Printer CreateTestPrinterWithPpdReference(const std::string& id,
                                          const std::string& name,
                                          const std::string& description,
                                          Printer::PpdReference ref) {
  Printer printer = CreateTestPrinter(id, name, description);
  Printer::PpdReference* mutable_ppd_reference =
      printer.mutable_ppd_reference();
  *mutable_ppd_reference = ref;
  return printer;
}

Printer CreateEnterprisePrinter(const std::string& id,
                                const std::string& name,
                                const std::string& description) {
  Printer printer = CreateTestPrinter(id, name, description);
  printer.set_source(Printer::SRC_POLICY);
  return printer;
}

}  // namespace

// Fake `PpdProvider` backend. This fake `PpdProvider` is used to fake fetching
// the PPD EULA license of a destination. If `effective_make_and_model` is
// empty, it will return with NOT_FOUND and an empty string. Otherwise, it will
// return SUCCESS with `effective_make_and_model` as the PPD license.
class FakePpdProvider : public chromeos::PpdProvider {
 public:
  FakePpdProvider() = default;

  void ResolvePpdLicense(base::StringPiece effective_make_and_model,
                         ResolvePpdLicenseCallback cb) override {
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(cb),
                       effective_make_and_model.empty() ? PpdProvider::NOT_FOUND
                                                        : PpdProvider::SUCCESS,
                       std::string(effective_make_and_model)));
  }

  // These methods are not used by `CupsPrintersManager`.
  void ResolvePpd(const Printer::PpdReference& reference,
                  ResolvePpdCallback cb) override {}
  void ResolvePpdReference(const chromeos::PrinterSearchData& search_data,
                           ResolvePpdReferenceCallback cb) override {}
  void ResolveManufacturers(ResolveManufacturersCallback cb) override {}
  void ResolvePrinters(const std::string& manufacturer,
                       ResolvePrintersCallback cb) override {}
  void ReverseLookup(const std::string& effective_make_and_model,
                     ReverseLookupCallback cb) override {}

 private:
  ~FakePpdProvider() override = default;
};

class TestLocalPrinterAshWithPrinterConfigurer : public TestLocalPrinterAsh {
 public:
  TestLocalPrinterAshWithPrinterConfigurer(
      Profile* profile,
      scoped_refptr<chromeos::PpdProvider> ppd_provider,
      chromeos::TestCupsPrintersManager* manager)
      : TestLocalPrinterAsh(profile, ppd_provider), manager_(manager) {}
  TestLocalPrinterAshWithPrinterConfigurer(
      const TestLocalPrinterAshWithPrinterConfigurer&) = delete;
  TestLocalPrinterAshWithPrinterConfigurer& operator=(
      const TestLocalPrinterAshWithPrinterConfigurer&) = delete;
  ~TestLocalPrinterAshWithPrinterConfigurer() override = default;

 private:
  std::unique_ptr<chromeos::PrinterConfigurer> CreatePrinterConfigurer(
      Profile* profile) override {
    return std::make_unique<chromeos::TestPrinterConfigurer>(manager_);
  }

  chromeos::TestCupsPrintersManager* manager_;
};

// Base testing class for `LocalPrinterAsh`.  Contains the base
// logic to allow for using either a local task runner or a service to make
// print backend calls, and to possibly enable fallback when using a service.
// Tests to trigger those different paths can be done by overloading
// `UseService()` and `SupportFallback()`.
class LocalPrinterAshTestBase : public testing::Test {
 public:
  const std::vector<PrinterSemanticCapsAndDefaults::Paper> kPapers = {
      {"bar", "vendor", {600, 600}}};

  LocalPrinterAshTestBase() = default;
  LocalPrinterAshTestBase(const LocalPrinterAshTestBase&) = delete;
  LocalPrinterAshTestBase& operator=(const LocalPrinterAshTestBase&) = delete;
  ~LocalPrinterAshTestBase() override = default;

  TestPrintBackend* sandboxed_print_backend() {
    return sandboxed_test_backend_.get();
  }
  TestPrintBackend* unsandboxed_print_backend() {
    return unsandboxed_test_backend_.get();
  }

  // Indicate if calls to print backend should be made using a service instead
  // of a local task runner.
  virtual bool UseService() = 0;

  // Indicate if fallback support for access-denied errors should be included
  // when using a service for print backend calls.
  virtual bool SupportFallback() = 0;

  void SetUp() override {
    // Choose between running with local test runner or via a service.
    feature_list_.InitWithFeatureState(features::kEnableOopPrintDrivers,
                                       UseService());

    sandboxed_test_backend_ = base::MakeRefCounted<TestPrintBackend>();
    ppd_provider_ = base::MakeRefCounted<FakePpdProvider>();
    chromeos::CupsPrintersManagerFactory::GetInstance()
        ->SetTestingFactoryAndUse(
            &profile_,
            base::BindLambdaForTesting([this](content::BrowserContext* context)
                                           -> std::unique_ptr<KeyedService> {
              auto printers_manager =
                  std::make_unique<chromeos::TestCupsPrintersManager>();
              printers_manager_ = printers_manager.get();
              return printers_manager;
            }));
    local_printer_ash_ =
        std::make_unique<TestLocalPrinterAshWithPrinterConfigurer>(
            &profile_, ppd_provider_, printers_manager_);

    if (UseService()) {
      sandboxed_print_backend_service_ =
          PrintBackendServiceTestImpl::LaunchForTesting(sandboxed_test_remote_,
                                                        sandboxed_test_backend_,
                                                        /*sandboxed=*/true);

      if (SupportFallback()) {
        unsandboxed_test_backend_ = base::MakeRefCounted<TestPrintBackend>();

        unsandboxed_print_backend_service_ =
            PrintBackendServiceTestImpl::LaunchForTesting(
                unsandboxed_test_remote_, unsandboxed_test_backend_,
                /*sandboxed=*/false);
      }
    } else {
      // Use of task runners will call `PrintBackend::CreateInstance()`, which
      // needs a test backend registered for it to use.
      PrintBackend::SetPrintBackendForTesting(sandboxed_test_backend_.get());
    }
  }

  void TearDown() override { PrintBackendServiceManager::ResetForTesting(); }

 protected:
  void AddPrinter(const std::string& id,
                  const std::string& display_name,
                  const std::string& description,
                  bool is_default,
                  bool requires_elevated_permissions) {
    auto caps = std::make_unique<PrinterSemanticCapsAndDefaults>();
    caps->papers = kPapers;
    auto basic_info = std::make_unique<PrinterBasicInfo>(
        id, display_name, description, /*printer_status=*/0, is_default,
        PrinterBasicInfoOptions{});

    if (SupportFallback()) {
      // Need to populate same values into a second print backend.
      // For fallback they will always be treated as valid.
      auto caps_unsandboxed =
          std::make_unique<PrinterSemanticCapsAndDefaults>(*caps);
      auto basic_info_unsandboxed =
          std::make_unique<PrinterBasicInfo>(*basic_info);
      unsandboxed_print_backend()->AddValidPrinter(
          id, std::move(caps_unsandboxed), std::move(basic_info_unsandboxed));
    }

    if (requires_elevated_permissions) {
      sandboxed_print_backend()->AddAccessDeniedPrinter(id);
    } else {
      sandboxed_print_backend()->AddValidPrinter(id, std::move(caps),
                                                 std::move(basic_info));
    }
  }

  void RunUntilIdle() { task_environment_.RunUntilIdle(); }

  chromeos::TestCupsPrintersManager& printers_manager() {
    DCHECK(printers_manager_);
    return *printers_manager_;
  }

  crosapi::LocalPrinterAsh* local_printer_ash() {
    return local_printer_ash_.get();
  }

 private:
  // Must outlive `profile_`.
  content::BrowserTaskEnvironment task_environment_;
  // Must outlive `printers_manager_`.
  TestingProfile profile_;
  scoped_refptr<TestPrintBackend> sandboxed_test_backend_;
  scoped_refptr<TestPrintBackend> unsandboxed_test_backend_;
  chromeos::TestCupsPrintersManager* printers_manager_;
  scoped_refptr<FakePpdProvider> ppd_provider_;
  std::unique_ptr<crosapi::LocalPrinterAsh> local_printer_ash_;

  // Support for testing via a service instead of with a local task runner.
  base::test::ScopedFeatureList feature_list_;
  mojo::Remote<mojom::PrintBackendService> sandboxed_test_remote_;
  mojo::Remote<mojom::PrintBackendService> unsandboxed_test_remote_;
  std::unique_ptr<PrintBackendServiceTestImpl> sandboxed_print_backend_service_;
  std::unique_ptr<PrintBackendServiceTestImpl>
      unsandboxed_print_backend_service_;
};

// Testing class to cover `LocalPrinterAsh` handling using a local
// task runner.
class LocalPrinterAshTest : public LocalPrinterAshTestBase {
 public:
  LocalPrinterAshTest() = default;
  LocalPrinterAshTest(const LocalPrinterAshTest&) = delete;
  LocalPrinterAshTest& operator=(const LocalPrinterAshTest&) = delete;
  ~LocalPrinterAshTest() override = default;

  bool UseService() override { return false; }
  bool SupportFallback() override { return false; }
};

// Testing class to cover `LocalPrinterAsh` handling using either a
// local task runner or a service.  Makes no attempt to cover fallback when
// using a service, which is handled separately by
// `LocalPrinterAshFallbackTest`
class LocalPrinterAshProcessScopeTest
    : public LocalPrinterAshTestBase,
      public testing::WithParamInterface<bool> {
 public:
  LocalPrinterAshProcessScopeTest() = default;
  LocalPrinterAshProcessScopeTest(const LocalPrinterAshProcessScopeTest&) =
      delete;
  LocalPrinterAshProcessScopeTest& operator=(
      const LocalPrinterAshProcessScopeTest&) = delete;
  ~LocalPrinterAshProcessScopeTest() override = default;

  bool UseService() override { return GetParam(); }
  bool SupportFallback() override { return false; }
};

// Testing class to cover `LocalPrinterAsh` handling using only a
// service and when fallback could yield different results.
class LocalPrinterAshFallbackTest : public LocalPrinterAshTestBase {
 public:
  LocalPrinterAshFallbackTest() = default;
  LocalPrinterAshFallbackTest(const LocalPrinterAshFallbackTest&) = delete;
  LocalPrinterAshFallbackTest& operator=(const LocalPrinterAshFallbackTest&) =
      delete;
  ~LocalPrinterAshFallbackTest() override = default;

  bool UseService() override { return true; }
  bool SupportFallback() override { return true; }
};

INSTANTIATE_TEST_SUITE_P(All, LocalPrinterAshProcessScopeTest, testing::Bool());

TEST_F(LocalPrinterAshTest, GetStatus) {
  chromeos::CupsPrinterStatus printer1("printer1");
  printer1.AddStatusReason(crosapi::mojom::StatusReason::Reason::kPaperJam,
                           crosapi::mojom::StatusReason::Severity::kError);
  printers_manager().SetPrinterStatus(printer1);
  crosapi::mojom::PrinterStatusPtr printer_status;
  local_printer_ash()->GetStatus(
      "printer1", base::BindOnce(base::BindLambdaForTesting(
                      [&](crosapi::mojom::PrinterStatusPtr status) {
                        printer_status = std::move(status);
                      })));
  auto expected_status = crosapi::mojom::PrinterStatus::New();
  expected_status->printer_id = "printer1";
  expected_status->timestamp = printer1.GetTimestamp();
  expected_status->status_reasons.push_back(crosapi::mojom::StatusReason::New(
      crosapi::mojom::StatusReason::Reason::kPaperJam,
      crosapi::mojom::StatusReason::Severity::kError));
  EXPECT_EQ(expected_status, printer_status);
}

TEST_F(LocalPrinterAshTest, GetPrinters) {
  Printer saved_printer =
      CreateTestPrinter("printer1", "saved", "description1");
  Printer enterprise_printer =
      CreateEnterprisePrinter("printer2", "enterprise", "description2");
  Printer automatic_printer =
      CreateTestPrinter("printer3", "automatic", "description3");

  printers_manager().AddPrinter(saved_printer, PrinterClass::kSaved);
  printers_manager().AddPrinter(enterprise_printer, PrinterClass::kEnterprise);
  printers_manager().AddPrinter(automatic_printer, PrinterClass::kAutomatic);

  std::vector<crosapi::mojom::LocalDestinationInfoPtr> printers;
  local_printer_ash()->GetPrinters(
      base::BindOnce(&RecordPrinterList, std::ref(printers)));

  std::vector<crosapi::mojom::LocalDestinationInfoPtr> expected_printers;
  expected_printers.push_back(crosapi::mojom::LocalDestinationInfo::New(
      "printer1", "saved", "description1", false));
  expected_printers.push_back(crosapi::mojom::LocalDestinationInfo::New(
      "printer2", "enterprise", "description2", true));
  expected_printers.push_back(crosapi::mojom::LocalDestinationInfo::New(
      "printer3", "automatic", "description3", false));
  EXPECT_EQ(expected_printers, printers);
}

// Tests that fetching capabilities for an existing installed printer is
// successful.
TEST_P(LocalPrinterAshProcessScopeTest, GetCapabilityValidPrinter) {
  Printer saved_printer =
      CreateTestPrinter("printer1", "saved", "description1");
  printers_manager().AddPrinter(saved_printer, PrinterClass::kSaved);
  printers_manager().InstallPrinter("printer1");

  // Add printer capabilities to `test_backend_`.
  AddPrinter("printer1", "saved", "description1", /*is_default=*/true,
             /*requires_elevated_permissions=*/false);

  crosapi::mojom::CapabilitiesResponsePtr fetched_caps;
  local_printer_ash()->GetCapability(
      "printer1", base::BindOnce(&RecordGetCapability, std::ref(fetched_caps)));

  RunUntilIdle();

  ASSERT_TRUE(fetched_caps);
  EXPECT_FALSE(fetched_caps->has_secure_protocol);
  EXPECT_EQ(crosapi::mojom::LocalDestinationInfo("printer1", "saved",
                                                 "description1", false),
            *fetched_caps->basic_info);
  ASSERT_TRUE(fetched_caps->capabilities);
  EXPECT_EQ(kPapers, fetched_caps->capabilities->papers);
}

// Test that printers which have not yet been installed are installed with
// `SetUpPrinter` before their capabilities are fetched.
TEST_P(LocalPrinterAshProcessScopeTest, GetCapabilityPrinterNotInstalled) {
  Printer discovered_printer =
      CreateTestPrinter("printer1", "discovered", "description1");
  // NOTE: The printer `discovered_printer` is not installed using
  // `InstallPrinter`.
  printers_manager().AddPrinter(discovered_printer, PrinterClass::kDiscovered);

  // Add printer capabilities to `test_backend_`.
  AddPrinter("printer1", "discovered", "description1", /*is_default=*/true,
             /*requires_elevated_permissions=*/false);

  crosapi::mojom::CapabilitiesResponsePtr fetched_caps;

  EXPECT_FALSE(printers_manager().IsPrinterInstalled(discovered_printer));

  // Install printer and fetch capabilities.
  local_printer_ash()->GetCapability(
      "printer1", base::BindOnce(&RecordGetCapability, std::ref(fetched_caps)));

  RunUntilIdle();

  EXPECT_TRUE(printers_manager().IsPrinterInstalled(discovered_printer));
  ASSERT_TRUE(fetched_caps);
  EXPECT_FALSE(fetched_caps->has_secure_protocol);
  EXPECT_EQ(crosapi::mojom::LocalDestinationInfo("printer1", "discovered",
                                                 "description1", false),
            *fetched_caps->basic_info);
  ASSERT_TRUE(fetched_caps->capabilities);
  EXPECT_EQ(kPapers, fetched_caps->capabilities->papers);
}

// In this test we expect the `GetCapability` to bail early because the
// provided printer can't be found in the `CupsPrintersManager`.
TEST_P(LocalPrinterAshProcessScopeTest, GetCapabilityInvalidPrinter) {
  crosapi::mojom::CapabilitiesResponsePtr fetched_caps;
  local_printer_ash()->GetCapability(
      "invalid printer",
      base::BindOnce(&RecordGetCapability, std::ref(fetched_caps)));

  RunUntilIdle();

  EXPECT_FALSE(fetched_caps);
}

// Test that installed printers to which the user does not have permission to
// access will receive a dictionary for the capabilities but will not have any
// settings in that.
TEST_P(LocalPrinterAshProcessScopeTest, GetCapabilityAccessDenied) {
  Printer saved_printer =
      CreateTestPrinter("printer1", "saved", "description1");
  printers_manager().AddPrinter(saved_printer, PrinterClass::kSaved);
  printers_manager().InstallPrinter("printer1");

  // Add printer capabilities to `test_backend_`.
  AddPrinter("printer1", "saved", "description1", /*is_default=*/true,
             /*requires_elevated_permissions=*/true);

  crosapi::mojom::CapabilitiesResponsePtr fetched_caps;
  local_printer_ash()->GetCapability(
      "printer1", base::BindOnce(&RecordGetCapability, std::ref(fetched_caps)));

  RunUntilIdle();

  ASSERT_TRUE(fetched_caps);
  EXPECT_FALSE(fetched_caps->has_secure_protocol);
  EXPECT_EQ(crosapi::mojom::LocalDestinationInfo("printer1", "saved",
                                                 "description1", false),
            *fetched_caps->basic_info);
  EXPECT_FALSE(fetched_caps->capabilities);
}

TEST_F(LocalPrinterAshFallbackTest, GetCapabilityElevatedPermissionsSucceeds) {
  Printer saved_printer =
      CreateTestPrinter("printer1", "saved", "description1");
  printers_manager().AddPrinter(saved_printer, PrinterClass::kSaved);
  printers_manager().InstallPrinter("printer1");

  // Add printer capabilities to `test_backend_`.
  AddPrinter("printer1", "saved", "description1", /*is_default=*/true,
             /*requires_elevated_permissions=*/true);

  // Note that printer does not initially show as requiring elevated privileges.
  EXPECT_FALSE(PrintBackendServiceManager::GetInstance()
                   .PrinterDriverRequiresElevatedPrivilege("printer1"));

  crosapi::mojom::CapabilitiesResponsePtr fetched_caps;
  local_printer_ash()->GetCapability(
      "printer1", base::BindOnce(&RecordGetCapability, std::ref(fetched_caps)));

  RunUntilIdle();

  // Verify that this printer now shows up as requiring elevated privileges.
  EXPECT_TRUE(PrintBackendServiceManager::GetInstance()
                  .PrinterDriverRequiresElevatedPrivilege("printer1"));

  // Getting capabilities should succeed when fallback is supported.
  ASSERT_TRUE(fetched_caps);
  EXPECT_FALSE(fetched_caps->has_secure_protocol);
  EXPECT_EQ(crosapi::mojom::LocalDestinationInfo("printer1", "saved",
                                                 "description1", false),
            *fetched_caps->basic_info);
  ASSERT_TRUE(fetched_caps->capabilities);
  EXPECT_EQ(kPapers, fetched_caps->capabilities->papers);
}

// Test that fetching a PPD license will return a license if the printer has one
// available.
TEST_F(LocalPrinterAshTest, FetchValidEulaUrl) {
  Printer::PpdReference ref;
  ref.effective_make_and_model = "expected_make_model";

  // Printers with a `PpdReference` will return a license
  Printer saved_printer = CreateTestPrinterWithPpdReference(
      "printer1", "saved", "description1", ref);
  printers_manager().AddPrinter(saved_printer, PrinterClass::kSaved);
  printers_manager().InstallPrinter("printer1");

  GURL fetched_eula_url;
  local_printer_ash()->GetEulaUrl(
      "printer1",
      base::BindOnce(&RecordGetEulaUrl, std::ref(fetched_eula_url)));

  RunUntilIdle();

  EXPECT_EQ(fetched_eula_url, GURL("chrome://os-credits/#expected_make_model"));
}

// Test that a printer with no PPD license will return an empty string.
TEST_F(LocalPrinterAshTest, FetchNotFoundEulaUrl) {
  // A printer without a `PpdReference` will simulate a PPD without a license.
  Printer saved_printer =
      CreateTestPrinter("printer1", "saved", "description1");
  printers_manager().AddPrinter(saved_printer, PrinterClass::kSaved);
  printers_manager().InstallPrinter("printer1");

  GURL fetched_eula_url;
  local_printer_ash()->GetEulaUrl(
      "printer1",
      base::BindOnce(&RecordGetEulaUrl, std::ref(fetched_eula_url)));

  RunUntilIdle();

  EXPECT_TRUE(fetched_eula_url.is_empty());
}

// Test that fetching a PPD license will exit early if the printer is not found
// in `CupsPrintersManager`.
TEST_F(LocalPrinterAshTest, FetchEulaUrlOnNonExistantPrinter) {
  Printer saved_printer =
      CreateTestPrinter("printer1", "saved", "description1");

  GURL fetched_eula_url;
  local_printer_ash()->GetEulaUrl(
      "printer1",
      base::BindOnce(&RecordGetEulaUrl, std::ref(fetched_eula_url)));

  RunUntilIdle();

  EXPECT_TRUE(fetched_eula_url.is_empty());
}

}  // namespace printing
