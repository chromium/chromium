// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/local_printer_ash.h"

#include <algorithm>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/ash/crosapi/test_local_printer_ash.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/ash/printing/cups_printers_manager_factory.h"
#include "chrome/browser/ash/printing/fake_cups_printers_manager.h"
#include "chrome/browser/ash/printing/ipp_client_info_calculator.h"
#include "chrome/browser/ash/printing/oauth2/authorization_zones_manager_factory.h"
#include "chrome/browser/ash/printing/oauth2/mock_authorization_zones_manager.h"
#include "chrome/browser/ash/printing/oauth2/status_code.h"
#include "chrome/browser/printing/local_printer_utils_chromeos.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/printing/printer_capabilities.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/crosapi/mojom/local_printer.mojom.h"
#include "chromeos/printing/cups_printer_status.h"
#include "chromeos/printing/ppd_provider.h"
#include "chromeos/printing/printer_configuration.h"
#include "components/account_id/account_id.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_service.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/user.h"
#include "content/public/browser/browser_context.h"
#include "content/public/test/browser_task_environment.h"
#include "printing/backend/print_backend.h"
#include "printing/backend/printing_restrictions.h"
#include "printing/backend/test_print_backend.h"
#include "printing/buildflags/buildflags.h"
#include "printing/mojom/print.mojom.h"
#include "printing/printing_features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "url/gurl.h"

#if BUILDFLAG(ENABLE_OOP_PRINTING)
#include "chrome/browser/printing/print_backend_service_manager.h"
#include "chrome/browser/printing/print_backend_service_test_impl.h"
#else
#include "base/notreached.h"
#endif

using ::chromeos::Printer;
using ::chromeos::PrinterClass;
using testing::ByMove;
using testing::NiceMock;
using testing::Return;

namespace printing {

namespace {

using LocalPrintersCallback = base::OnceCallback<void(
    std::vector<crosapi::mojom::LocalDestinationInfoPtr>)>;
using printing::mojom::IppClientInfoPtr;

constexpr char kPrinterUri[] = "http://localhost:80";
const AccountId kAffiliatedUserAccountId =
    AccountId::FromUserEmail("user@example.com");
const AccountId kUnaffiliatedUserAccountId =
    AccountId::FromUserEmail("user@gmail.com");

Printer CreateTestPrinter(const std::string& id,
                          const std::string& name,
                          const std::string& description) {
  Printer printer;
  printer.SetUri(kPrinterUri);
  printer.set_id(id);
  printer.set_display_name(name);
  printer.set_description(description);
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

  void ResolvePpdLicense(std::string_view effective_make_and_model,
                         ResolvePpdLicenseCallback cb) override {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
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

class FakeLocalPrintersObserver : public crosapi::mojom::LocalPrintersObserver {
 public:
  FakeLocalPrintersObserver() = default;
  ~FakeLocalPrintersObserver() override = default;

  mojo::PendingRemote<crosapi::mojom::LocalPrintersObserver> GenerateRemote() {
    mojo::PendingRemote<crosapi::mojom::LocalPrintersObserver> remote;
    receiver_.Bind(remote.InitWithNewPipeAndPassReceiver());
    return remote;
  }

  // crosapi::mojom::LocalPrintersObserver:
  void OnLocalPrintersUpdated(
      std::vector<crosapi::mojom::LocalDestinationInfoPtr> printers) override {
    for (const auto& printer : printers) {
      crosapi::mojom::LocalDestinationInfoPtr local_printer =
          crosapi::mojom::LocalDestinationInfo::New();
      local_printer->id = printer->id;
      local_printers_.push_back(std::move(local_printer));
    }

    if (local_printers_callback_) {
      std::move(local_printers_callback_).Run(mojo::Clone(local_printers_));
    }
  }

  void GetLocalPrinters(LocalPrintersCallback callback) {
    local_printers_callback_ = std::move(callback);
  }

 private:
  LocalPrintersCallback local_printers_callback_;
  std::vector<crosapi::mojom::LocalDestinationInfoPtr> local_printers_;
  mojo::Receiver<crosapi::mojom::LocalPrintersObserver> receiver_{this};
};

class TestLocalPrinterAshWithPrinterConfigurer : public TestLocalPrinterAsh {
 public:
  TestLocalPrinterAshWithPrinterConfigurer(
      Profile* profile,
      scoped_refptr<chromeos::PpdProvider> ppd_provider,
      ash::FakeCupsPrintersManager* manager)
      : TestLocalPrinterAsh(profile, ppd_provider), manager_(manager) {}
  TestLocalPrinterAshWithPrinterConfigurer(
      const TestLocalPrinterAshWithPrinterConfigurer&) = delete;
  TestLocalPrinterAshWithPrinterConfigurer& operator=(
      const TestLocalPrinterAshWithPrinterConfigurer&) = delete;
  ~TestLocalPrinterAshWithPrinterConfigurer() override = default;

 private:
  const raw_ptr<ash::FakeCupsPrintersManager> manager_;
};

// Base testing class for `LocalPrinterAsh`.  Contains the base
// logic to allow for using either a local task runner or a service to make
// print backend calls, and to possibly enable fallback when using a service.
// Tests to trigger those different paths can be done by overloading
// `UseService()` and `SupportFallback()`.
class LocalPrinterAshTestBase : public testing::Test {
 public:
  const std::vector<PrinterSemanticCapsAndDefaults::Paper> kPapers = {
      {"bar", "vendor", gfx::Size(600, 600), gfx::Rect(0, 0, 600, 600)}};

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

  void SetUsername(const std::string& username) {
    fake_user_manager_->SaveUserDisplayEmail(
        AccountId::FromUserEmail(TestingProfile::kDefaultProfileUserName),
        username);
  }

  // Indicate if calls to print backend should be made using a service instead
  // of a local task runner.
  virtual bool UseService() = 0;

  // Indicate if fallback support for access-denied errors should be included
  // when using a service for print backend calls.
  virtual bool SupportFallback() = 0;

  virtual std::vector<base::test::FeatureRefAndParams> FeaturesToEnable() {
    return {};
  }

  sync_preferences::TestingPrefServiceSyncable* GetPrefs() {
    return profile_.GetTestingPrefService();
  }

  void SetUp() override {
    fake_user_manager_->AddUser(
        AccountId::FromUserEmail(TestingProfile::kDefaultProfileUserName));

    std::vector<base::test::FeatureRefAndParams> features_to_enable =
        FeaturesToEnable();
    std::vector<base::test::FeatureRef> features_to_disable;
#if BUILDFLAG(ENABLE_OOP_PRINTING)
    // Choose between running with local test runner or via a service.
    if (UseService()) {
      features_to_enable.emplace_back(base::test::FeatureRefAndParams(
          features::kEnableOopPrintDrivers,
          {{ features::kEnableOopPrintDriversSandbox.name,
             "true" }}));
    } else {
      features_to_disable.push_back(features::kEnableOopPrintDrivers);
    }
#endif
    feature_list_.InitWithFeaturesAndParameters(features_to_enable,
                                                features_to_disable);

    sandboxed_test_backend_ = base::MakeRefCounted<TestPrintBackend>();
    ppd_provider_ = base::MakeRefCounted<FakePpdProvider>();
    ash::CupsPrintersManagerFactory::GetInstance()->SetTestingFactoryAndUse(
        &profile_,
        base::BindLambdaForTesting([this](content::BrowserContext* context)
                                       -> std::unique_ptr<KeyedService> {
          auto printers_manager =
              std::make_unique<ash::FakeCupsPrintersManager>();
          printers_manager_ = printers_manager.get();
          return printers_manager;
        }));
    local_printer_ash_ =
        std::make_unique<TestLocalPrinterAshWithPrinterConfigurer>(
            &profile_, ppd_provider_, printers_manager_);

    if (UseService()) {
#if BUILDFLAG(ENABLE_OOP_PRINTING)
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

      // Client registration is normally covered by `PrintPreviewUI`, so mimic
      // that here.
      service_manager_client_id_ =
          PrintBackendServiceManager::GetInstance().RegisterQueryClient();
#else
      NOTREACHED();
#endif  // BUILDFLAG(ENABLE_OOP_PRINTING)
    } else {
      // Use of task runners will call `PrintBackend::CreateInstance()`, which
      // needs a test backend registered for it to use.
      PrintBackend::SetPrintBackendForTesting(sandboxed_test_backend_.get());
    }
  }

  void TearDown() override {
#if BUILDFLAG(ENABLE_OOP_PRINTING)
    if (UseService()) {
      PrintBackendServiceManager::GetInstance().UnregisterClient(
          service_manager_client_id_);
    }

    PrintBackendServiceManager::ResetForTesting();
#endif
  }

 protected:
  void AddPrinter(const std::string& id,
                  const std::string& display_name,
                  const std::string& description,
                  bool requires_elevated_permissions) {
    auto caps = std::make_unique<PrinterSemanticCapsAndDefaults>();
    caps->papers = kPapers;
    auto basic_info = std::make_unique<PrinterBasicInfo>(
        id, display_name, description, PrinterBasicInfoOptions{});

#if BUILDFLAG(ENABLE_OOP_PRINTING)
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
#endif  // BUILDFLAG(ENABLE_OOP_PRINTING)

    if (requires_elevated_permissions) {
      sandboxed_print_backend()->AddAccessDeniedPrinter(id);
    } else {
      sandboxed_print_backend()->AddValidPrinter(id, std::move(caps),
                                                 std::move(basic_info));
    }
    sandboxed_print_backend()->SetDefaultPrinterName(id);
  }

#if BUILDFLAG(ENABLE_OOP_PRINTING)
  void SetTerminateServiceOnNextInteraction() {
    if (SupportFallback()) {
      unsandboxed_print_backend_service_
          ->SetTerminateReceiverOnNextInteraction();
    }

    sandboxed_print_backend_service_->SetTerminateReceiverOnNextInteraction();
  }
#endif  // BUILDFLAG(ENABLE_OOP_PRINTING)

  void RunUntilIdle() { task_environment_.RunUntilIdle(); }

  ash::FakeCupsPrintersManager& printers_manager() {
    DCHECK(printers_manager_);
    return *printers_manager_;
  }

  crosapi::LocalPrinterAsh* local_printer_ash() {
    return local_printer_ash_.get();
  }

  base::test::ScopedFeatureList& feature_list() { return feature_list_; }

  ash::FakeChromeUserManager* fake_user_manager() {
    return fake_user_manager_.Get();
  }

 protected:
  FakeLocalPrintersObserver fake_local_printers_observer_;

 private:
  user_manager::TypedScopedUserManager<ash::FakeChromeUserManager>
      fake_user_manager_{std::make_unique<ash::FakeChromeUserManager>()};

  // Must outlive `profile_`.
  content::BrowserTaskEnvironment task_environment_;

  // Must outlive `printers_manager_`.
  TestingProfile profile_;
  scoped_refptr<TestPrintBackend> sandboxed_test_backend_;
  scoped_refptr<TestPrintBackend> unsandboxed_test_backend_;
  raw_ptr<ash::FakeCupsPrintersManager> printers_manager_ = nullptr;
  scoped_refptr<FakePpdProvider> ppd_provider_;
  std::unique_ptr<crosapi::LocalPrinterAsh> local_printer_ash_;
  base::test::ScopedFeatureList feature_list_;

#if BUILDFLAG(ENABLE_OOP_PRINTING)
  // Support for testing via a service instead of with a local task runner.
  mojo::Remote<mojom::PrintBackendService> sandboxed_test_remote_;
  mojo::Remote<mojom::PrintBackendService> unsandboxed_test_remote_;
  std::unique_ptr<PrintBackendServiceTestImpl> sandboxed_print_backend_service_;
  std::unique_ptr<PrintBackendServiceTestImpl>
      unsandboxed_print_backend_service_;
  PrintBackendServiceManager::ClientId service_manager_client_id_;
#endif  // BUILDFLAG(ENABLE_OOP_PRINTING)
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

TEST_F(LocalPrinterAshTest, GetPolicies_Unset) {
  crosapi::mojom::PoliciesPtr policies;
  local_printer_ash()->GetPolicies(base::BindLambdaForTesting(
      [&](crosapi::mojom::PoliciesPtr data) { policies = std::move(data); }));
  EXPECT_EQ(crosapi::mojom::Policies::New(), policies);
}

TEST_F(LocalPrinterAshTest, GetPolicies_PaperSize) {
  auto* prefs = GetPrefs();
  base::DictValue paper_size;
  paper_size.Set(kPaperSizeName, "iso_a4_210x297mm");
  prefs->Set("printing.paper_size_default", base::Value(std::move(paper_size)));

  crosapi::mojom::PoliciesPtr policies;
  local_printer_ash()->GetPolicies(base::BindLambdaForTesting(
      [&](crosapi::mojom::PoliciesPtr data) { policies = std::move(data); }));

  ASSERT_TRUE(policies);
  EXPECT_EQ(gfx::Size(210000, 297000), policies->paper_size_default);
}

TEST_F(LocalPrinterAshTest, GetPolicies_BackgroundGraphics) {
  auto* prefs = GetPrefs();
  // crosapi::mojom::Policies::BackgroundGraphicsModeRestriction::kDisabled
  prefs->SetInteger(prefs::kPrintingAllowedBackgroundGraphicsModes, 2);
  // crosapi::mojom::Policies::BackgroundGraphicsModeRestriction::kEnabled
  prefs->SetInteger(prefs::kPrintingBackgroundGraphicsDefault, 1);

  crosapi::mojom::PoliciesPtr policies;
  local_printer_ash()->GetPolicies(base::BindLambdaForTesting(
      [&](crosapi::mojom::PoliciesPtr data) { policies = std::move(data); }));

  ASSERT_TRUE(policies);
  EXPECT_EQ(
      crosapi::mojom::Policies::BackgroundGraphicsModeRestriction::kDisabled,
      policies->allowed_background_graphics_modes);
  EXPECT_EQ(
      crosapi::mojom::Policies::BackgroundGraphicsModeRestriction::kEnabled,
      policies->background_graphics_default);
}

TEST_F(LocalPrinterAshTest, GetPolicies_MaxSheetsAllowed) {
  auto* prefs = GetPrefs();
  prefs->SetInteger(ash::prefs::kPrintingMaxSheetsAllowed, 5);

  crosapi::mojom::PoliciesPtr policies;
  local_printer_ash()->GetPolicies(base::BindLambdaForTesting(
      [&](crosapi::mojom::PoliciesPtr data) { policies = std::move(data); }));

  EXPECT_TRUE(policies->max_sheets_allowed_has_value);
  EXPECT_EQ(5u, policies->max_sheets_allowed);
}

// Zero sheets allowed is a valid policy.
TEST_F(LocalPrinterAshTest, GetPolicies_ZeroSheetsAllowed) {
  auto* prefs = GetPrefs();
  prefs->SetInteger(ash::prefs::kPrintingMaxSheetsAllowed, 0);

  crosapi::mojom::PoliciesPtr policies;
  local_printer_ash()->GetPolicies(base::BindLambdaForTesting(
      [&](crosapi::mojom::PoliciesPtr data) { policies = std::move(data); }));

  ASSERT_TRUE(policies);
  EXPECT_TRUE(policies->max_sheets_allowed_has_value);
  EXPECT_EQ(0u, policies->max_sheets_allowed);
}

// Negative sheets allowed is not a valid policy.
TEST_F(LocalPrinterAshTest, GetPolicies_NegativeMaxSheets) {
  auto* prefs = GetPrefs();
  prefs->SetInteger(ash::prefs::kPrintingMaxSheetsAllowed, -1);

  crosapi::mojom::PoliciesPtr policies;
  local_printer_ash()->GetPolicies(base::BindLambdaForTesting(
      [&](crosapi::mojom::PoliciesPtr data) { policies = std::move(data); }));

  ASSERT_TRUE(policies);
  EXPECT_FALSE(policies->max_sheets_allowed_has_value);
}

TEST_F(LocalPrinterAshTest, GetPolicies_PrintHeaderFooter_UnmanagedDisabled) {
  auto* prefs = GetPrefs();
  prefs->SetBoolean(prefs::kPrintHeaderFooter, false);
  crosapi::mojom::PoliciesPtr policies;
  local_printer_ash()->GetPolicies(base::BindLambdaForTesting(
      [&](crosapi::mojom::PoliciesPtr data) { policies = std::move(data); }));
  ASSERT_TRUE(policies);
  EXPECT_EQ(crosapi::mojom::Policies::OptionalBool::kUnset,
            policies->print_header_footer_allowed);
  EXPECT_EQ(crosapi::mojom::Policies::OptionalBool::kFalse,
            policies->print_header_footer_default);
}

TEST_F(LocalPrinterAshTest, GetPolicies_PrintHeaderFooter_UnmanagedEnabled) {
  auto* prefs = GetPrefs();
  prefs->SetBoolean(prefs::kPrintHeaderFooter, true);
  crosapi::mojom::PoliciesPtr policies;
  local_printer_ash()->GetPolicies(base::BindLambdaForTesting(
      [&](crosapi::mojom::PoliciesPtr data) { policies = std::move(data); }));
  ASSERT_TRUE(policies);
  EXPECT_EQ(crosapi::mojom::Policies::OptionalBool::kUnset,
            policies->print_header_footer_allowed);
  EXPECT_EQ(crosapi::mojom::Policies::OptionalBool::kTrue,
            policies->print_header_footer_default);
}

TEST_F(LocalPrinterAshTest, GetPolicies_PrintHeaderFooter_ManagedDisabled) {
  auto* prefs = GetPrefs();
  prefs->SetManagedPref(prefs::kPrintHeaderFooter,
                        std::make_unique<base::Value>(false));
  crosapi::mojom::PoliciesPtr policies;
  local_printer_ash()->GetPolicies(base::BindLambdaForTesting(
      [&](crosapi::mojom::PoliciesPtr data) { policies = std::move(data); }));
  ASSERT_TRUE(policies);
  EXPECT_EQ(crosapi::mojom::Policies::OptionalBool::kFalse,
            policies->print_header_footer_allowed);
  EXPECT_EQ(crosapi::mojom::Policies::OptionalBool::kUnset,
            policies->print_header_footer_default);
}

TEST_F(LocalPrinterAshTest, GetPolicies_PrintHeaderFooter_ManagedEnabled) {
  auto* prefs = GetPrefs();
  prefs->SetManagedPref(prefs::kPrintHeaderFooter,
                        std::make_unique<base::Value>(true));
  crosapi::mojom::PoliciesPtr policies;
  local_printer_ash()->GetPolicies(base::BindLambdaForTesting(
      [&](crosapi::mojom::PoliciesPtr data) { policies = std::move(data); }));
  ASSERT_TRUE(policies);
  EXPECT_EQ(crosapi::mojom::Policies::OptionalBool::kTrue,
            policies->print_header_footer_allowed);
  EXPECT_EQ(crosapi::mojom::Policies::OptionalBool::kUnset,
            policies->print_header_footer_default);
}

TEST_F(LocalPrinterAshTest, GetPolicies_Color) {
  const uint32_t expected_allowed_color_modes = static_cast<uint32_t>(
      static_cast<int32_t>(printing::mojom::ColorModeRestriction::kMonochrome) |
      static_cast<int32_t>(printing::mojom::ColorModeRestriction::kColor));
  auto* prefs = GetPrefs();
  prefs->SetInteger(ash::prefs::kPrintingAllowedColorModes, 3);
  prefs->SetInteger(ash::prefs::kPrintingColorDefault, 2);

  crosapi::mojom::PoliciesPtr policies;
  local_printer_ash()->GetPolicies(base::BindLambdaForTesting(
      [&](crosapi::mojom::PoliciesPtr data) { policies = std::move(data); }));

  EXPECT_EQ(expected_allowed_color_modes, policies->allowed_color_modes);
  EXPECT_EQ(printing::mojom::ColorModeRestriction::kColor,
            policies->default_color_mode);
}

TEST_F(LocalPrinterAshTest, GetPolicies_Duplex) {
  const uint32_t expected_allowed_duplex_modes = static_cast<uint32_t>(
      static_cast<int32_t>(printing::mojom::DuplexModeRestriction::kSimplex) |
      static_cast<int32_t>(printing::mojom::DuplexModeRestriction::kDuplex));
  auto* prefs = GetPrefs();
  prefs->SetInteger(ash::prefs::kPrintingAllowedDuplexModes, 7);
  prefs->SetInteger(ash::prefs::kPrintingDuplexDefault, 1);

  crosapi::mojom::PoliciesPtr policies;
  local_printer_ash()->GetPolicies(base::BindLambdaForTesting(
      [&](crosapi::mojom::PoliciesPtr data) { policies = std::move(data); }));

  EXPECT_EQ(expected_allowed_duplex_modes, policies->allowed_duplex_modes);
  EXPECT_EQ(printing::mojom::DuplexModeRestriction::kSimplex,
            policies->default_duplex_mode);
}

TEST_F(LocalPrinterAshTest, GetPolicies_Pin) {
  auto* prefs = GetPrefs();
  prefs->SetInteger(ash::prefs::kPrintingAllowedPinModes, 1);
  prefs->SetInteger(ash::prefs::kPrintingPinDefault, 2);

  crosapi::mojom::PoliciesPtr policies;
  local_printer_ash()->GetPolicies(base::BindLambdaForTesting(
      [&](crosapi::mojom::PoliciesPtr data) { policies = std::move(data); }));

  EXPECT_EQ(printing::mojom::PinModeRestriction::kPin,
            policies->allowed_pin_modes);
  EXPECT_EQ(printing::mojom::PinModeRestriction::kNoPin,
            policies->default_pin_mode);
}

// Verify the LocalPrintersObserver receives the full set of local printers
// when added or triggered.
TEST_F(LocalPrinterAshTest, LocalPrintersObserver) {
  Printer saved_printer =
      CreateTestPrinter("printer1", "saved", "description1");
  Printer enterprise_printer =
      CreateEnterprisePrinter("printer2", "enterprise", "description2");
  Printer automatic_printer =
      CreateTestPrinter("printer3", "automatic", "description3");

  printers_manager().AddPrinter(saved_printer, PrinterClass::kSaved);
  printers_manager().AddPrinter(enterprise_printer, PrinterClass::kEnterprise);
  printers_manager().AddPrinter(automatic_printer, PrinterClass::kAutomatic);

  // Starting observation should return the 3 added printers.
  local_printer_ash()->AddLocalPrintersObserver(
      fake_local_printers_observer_.GenerateRemote(),
      base::BindLambdaForTesting(
          [&](std::vector<crosapi::mojom::LocalDestinationInfoPtr> printers) {
            EXPECT_EQ(3u, printers.size());
          }));

  base::test::TestFuture<std::vector<crosapi::mojom::LocalDestinationInfoPtr>>
      printers_future;
  fake_local_printers_observer_.GetLocalPrinters(printers_future.GetCallback());

  local_printer_ash()->OnLocalPrintersUpdated();
  EXPECT_EQ(3u, printers_future.Get().size());
}

TEST(LocalPrinterAsh, ConfigToMojom) {
  ash::PrintServersConfig config;
  config.fetching_mode = crosapi::mojom::PrintServersConfig::
      ServerPrintersFetchingMode::kSingleServerOnly;
  config.print_servers.emplace_back("id", GURL("http://localhost"), "name");
  crosapi::mojom::PrintServersConfigPtr mojom =
      crosapi::LocalPrinterAsh::ConfigToMojom(config);
  ASSERT_TRUE(mojom);
  EXPECT_EQ(crosapi::mojom::PrintServersConfig::ServerPrintersFetchingMode::
                kSingleServerOnly,
            mojom->fetching_mode);
  ASSERT_EQ(1u, mojom->print_servers.size());
  EXPECT_EQ("id", mojom->print_servers[0]->id);
  EXPECT_EQ(GURL("http://localhost"), mojom->print_servers[0]->url);
  EXPECT_EQ("name", mojom->print_servers[0]->name);
}

TEST(LocalPrinterAsh, PrinterToMojom) {
  // Status
  chromeos::CupsPrinterStatus status("id");
  status.AddStatusReason(crosapi::mojom::StatusReason::Reason::kOutOfInk,
                         crosapi::mojom::StatusReason::Severity::kWarning);
  // Managed print options
  chromeos::Printer::ManagedPrintOptions print_options;
  chromeos::Printer::PrintOption<bool> color_option;
  color_option.default_value = true;
  color_option.allowed_values = std::vector<bool>{false, true};

  Printer printer("id");
  printer.set_display_name("name");
  printer.set_description("description");
  printer.set_printer_status(status);
  printer.set_print_job_options(print_options);
  crosapi::mojom::LocalDestinationInfoPtr mojom =
      printing::PrinterToMojom(printer);
  ASSERT_TRUE(mojom);
  EXPECT_EQ("id", mojom->id);
  EXPECT_EQ("name", mojom->name);
  EXPECT_EQ("description", mojom->description);
  EXPECT_FALSE(mojom->configured_via_policy);
  EXPECT_EQ(printing::StatusToMojom(status), mojom->printer_status);
  EXPECT_EQ(printing::ManagedPrintOptionsToMojom(print_options),
            mojom->managed_print_options);
}

TEST(LocalPrinterAsh, PrinterToMojom_ConfiguredViaPolicy) {
  Printer printer("id");
  printer.set_source(Printer::SRC_POLICY);
  crosapi::mojom::LocalDestinationInfoPtr mojom =
      printing::PrinterToMojom(printer);
  ASSERT_TRUE(mojom);
  EXPECT_EQ("id", mojom->id);
  EXPECT_TRUE(mojom->configured_via_policy);
}

TEST(LocalPrinterAsh, StatusToMojom) {
  chromeos::CupsPrinterStatus status("id");
  status.AddStatusReason(crosapi::mojom::StatusReason::Reason::kOutOfInk,
                         crosapi::mojom::StatusReason::Severity::kWarning);
  crosapi::mojom::PrinterStatusPtr mojom = printing::StatusToMojom(status);
  ASSERT_TRUE(mojom);
  EXPECT_EQ("id", mojom->printer_id);
  EXPECT_EQ(status.GetTimestamp(), mojom->timestamp);
  ASSERT_EQ(1u, mojom->status_reasons.size());
  EXPECT_EQ(crosapi::mojom::StatusReason::Reason::kOutOfInk,
            mojom->status_reasons[0]->reason);
  EXPECT_EQ(crosapi::mojom::StatusReason::Severity::kWarning,
            mojom->status_reasons[0]->severity);
}

}  // namespace printing
