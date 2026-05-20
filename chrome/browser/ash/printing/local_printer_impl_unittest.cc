// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/printing/local_printer_impl.h"

#include "ash/constants/ash_features.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/ash/printing/cups_printers_manager_factory.h"
#include "chrome/browser/ash/printing/fake_cups_printers_manager.h"
#include "chrome/browser/ash/printing/oauth2/authorization_zones_manager_factory.h"
#include "chrome/browser/ash/printing/oauth2/mock_authorization_zones_manager.h"
#include "chrome/browser/ash/printing/oauth2/status_code.h"
#include "chrome/browser/global_features.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/ash/components/browser_context_helper/annotated_account_id.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "chromeos/printing/cups_printer_status.h"
#include "chromeos/printing/printer_configuration.h"
#include "chromeos/printing/uri.h"
#include "components/account_id/account_id.h"
#include "components/account_id/account_id_literal.h"
#include "components/application_locale_storage/application_locale_storage.h"
#include "components/prefs/testing_pref_service.h"
#include "components/session_manager/test/test_user_session_manager.h"
#include "components/user_manager/test_helper.h"
#include "content/public/test/browser_task_environment.h"
#include "printing/backend/print_backend.h"
#include "printing/backend/test_print_backend.h"
#include "printing/mojom/print.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(ENABLE_OOP_PRINTING)
#include "chrome/browser/printing/print_backend_service_manager.h"
#include "chrome/browser/printing/print_backend_service_test_impl.h"
#else
#include "base/notreached.h"
#endif

namespace ash {

namespace {

constexpr char kPrinterUri[] = "http://localhost:80";
constexpr char kEmail[] = "test@example.com";
constexpr auto kAccountId =
    AccountId::Literal::FromUserEmailGaiaId(kEmail,
                                            GaiaId::Literal("123456789"));

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
  void ResolvePpd(const chromeos::Printer::PpdReference& reference,
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

class TestLocalPrinterImpl : public LocalPrinterImpl {
 public:
  TestLocalPrinterImpl()
      : LocalPrinterImpl(TestingBrowserProcess::GetGlobal()
                             ->GetFeatures()
                             ->application_locale_storage()) {}
  ~TestLocalPrinterImpl() override = default;

  scoped_refptr<chromeos::PpdProvider> CreatePpdProvider(
      Profile* profile) override {
    return base::MakeRefCounted<FakePpdProvider>();
  }
};

chromeos::Printer CreateTestPrinter(const std::string& id,
                                    const std::string& name,
                                    const std::string& description) {
  chromeos::Printer printer;
  printer.SetUri(kPrinterUri);
  printer.set_id(id);
  printer.set_display_name(name);
  printer.set_description(description);
  return printer;
}

chromeos::Printer CreateEnterprisePrinter(const std::string& id,
                                          const std::string& name,
                                          const std::string& description) {
  chromeos::Printer printer = CreateTestPrinter(id, name, description);
  printer.set_source(chromeos::Printer::SRC_POLICY);
  return printer;
}

void RecordCapabilityToFuture(
    base::test::TestFuture<
        std::optional<chromeos::Printer>,
        std::optional<::printing::PrinterSemanticCapsAndDefaults>>* future,
    base::optional_ref<const chromeos::Printer> printer,
    const std::optional<::printing::PrinterSemanticCapsAndDefaults>& caps) {
  future->SetValue(
      printer.has_value() ? std::make_optional(*printer) : std::nullopt, caps);
}

void RecordOAuthTokenToFuture(
    base::test::TestFuture<std::optional<std::string>>* future,
    base::optional_ref<const std::string> token) {
  future->SetValue(token.has_value() ? std::make_optional(*token)
                                     : std::nullopt);
}

}  // namespace

class LocalPrinterImplTestBase : public testing::Test {
 public:
  const std::vector<::printing::PrinterSemanticCapsAndDefaults::Paper> kPapers =
      {{"bar", "vendor", gfx::Size(600, 600), gfx::Rect(0, 0, 600, 600)}};

  LocalPrinterImplTestBase(bool use_service,
                           bool support_fallback,
                           bool enable_oauth = false)
      : use_service_(use_service),
        support_fallback_(support_fallback),
        enable_oauth_(enable_oauth) {}
  LocalPrinterImplTestBase(const LocalPrinterImplTestBase&) = delete;
  LocalPrinterImplTestBase& operator=(const LocalPrinterImplTestBase&) = delete;
  ~LocalPrinterImplTestBase() override = default;

  void SetUp() override {
    test_user_session_manager_ =
        std::make_unique<ash::test::TestUserSessionManager>(
            TestingBrowserProcess::GetGlobal()->GetTestingLocalState());
    profile_manager_ = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal());
    ASSERT_TRUE(profile_manager_->SetUp());

    ASSERT_TRUE(test_user_session_manager_->AddRegularUser(kAccountId));
    test_user_session_manager_->LogIn(kAccountId);

    profile_ = profile_manager_->CreateTestingProfile(kEmail);
    ash::AnnotatedAccountId::Set(profile_, kAccountId);
    user_manager::UserManager::Get()->OnUserProfileCreated(
        kAccountId, profile_->GetPrefs());

    std::vector<base::test::FeatureRefAndParams> features_to_enable = {};
    std::vector<base::test::FeatureRef> features_to_disable = {};
#if BUILDFLAG(ENABLE_OOP_PRINTING)
    if (use_service_) {
      features_to_enable.emplace_back(base::test::FeatureRefAndParams(
          ::printing::features::kEnableOopPrintDrivers,
          {{::printing::features::kEnableOopPrintDriversSandbox.name,
            "true"}}));
    } else {
      features_to_disable.push_back(
          ::printing::features::kEnableOopPrintDrivers);
    }
#endif
    if (enable_oauth_) {
      features_to_enable.emplace_back(
          base::test::FeatureRefAndParams(ash::features::kEnableOAuthIpp, {}));
    }
    feature_list_.InitWithFeaturesAndParameters(features_to_enable,
                                                features_to_disable);

    sandboxed_test_backend_ =
        base::MakeRefCounted<::printing::TestPrintBackend>();
    ash::CupsPrintersManagerFactory::GetInstance()->SetTestingFactoryAndUse(
        profile_,
        base::BindLambdaForTesting([this](content::BrowserContext* context)
                                       -> std::unique_ptr<KeyedService> {
          auto printers_manager =
              std::make_unique<ash::FakeCupsPrintersManager>();
          printers_manager_ = printers_manager.get();
          return printers_manager;
        }));

    if (enable_oauth_) {
      ash::printing::oauth2::AuthorizationZonesManagerFactory::GetInstance()
          ->SetTestingFactoryAndUse(
              profile_,
              base::BindRepeating([](content::BrowserContext* context)
                                      -> std::unique_ptr<KeyedService> {
                return std::make_unique<testing::StrictMock<
                    ash::printing::oauth2::MockAuthorizationZoneManager>>();
              }));
    }

    local_printer_ = std::make_unique<TestLocalPrinterImpl>();

    if (use_service_) {
#if BUILDFLAG(ENABLE_OOP_PRINTING)
      sandboxed_print_backend_service_ =
          ::printing::PrintBackendServiceTestImpl::LaunchForTesting(
              sandboxed_test_remote_, sandboxed_test_backend_,
              /*sandboxed=*/true);

      if (support_fallback_) {
        unsandboxed_test_backend_ =
            base::MakeRefCounted<::printing::TestPrintBackend>();

        unsandboxed_print_backend_service_ =
            ::printing::PrintBackendServiceTestImpl::LaunchForTesting(
                unsandboxed_test_remote_, unsandboxed_test_backend_,
                /*sandboxed=*/false);
      }

      service_manager_client_id_ =
          ::printing::PrintBackendServiceManager::GetInstance()
              .RegisterQueryClient();
#else
      NOTREACHED();
#endif
    } else {
      ::printing::PrintBackend::SetPrintBackendForTesting(
          sandboxed_test_backend_.get());
    }
  }

  void TearDown() override {
#if BUILDFLAG(ENABLE_OOP_PRINTING)
    if (use_service_) {
      ::printing::PrintBackendServiceManager::GetInstance().UnregisterClient(
          service_manager_client_id_);
    }

    ::printing::PrintBackendServiceManager::ResetForTesting();
#endif

    local_printer_ = nullptr;
    printers_manager_ = nullptr;
    user_manager::UserManager::Get()->OnUserProfileWillBeDestroyed(kAccountId);
    profile_ = nullptr;
    profile_manager_->DeleteTestingProfile(kEmail);
    profile_manager_.reset();
  }

 protected:
  void AddPrinter(const std::string& id,
                  const std::string& display_name,
                  const std::string& description,
                  bool requires_elevated_permissions) {
    auto caps = std::make_unique<::printing::PrinterSemanticCapsAndDefaults>();
    caps->papers = kPapers;
    auto basic_info = std::make_unique<::printing::PrinterBasicInfo>(
        id, display_name, description, ::printing::PrinterBasicInfoOptions{});

#if BUILDFLAG(ENABLE_OOP_PRINTING)
    if (support_fallback_) {
      auto caps_unsandboxed =
          std::make_unique<::printing::PrinterSemanticCapsAndDefaults>(*caps);
      auto basic_info_unsandboxed =
          std::make_unique<::printing::PrinterBasicInfo>(*basic_info);
      unsandboxed_test_backend_->AddValidPrinter(
          id, std::move(caps_unsandboxed), std::move(basic_info_unsandboxed));
    }
#endif

    if (requires_elevated_permissions) {
      sandboxed_test_backend_->AddAccessDeniedPrinter(id);
    } else {
      sandboxed_test_backend_->AddValidPrinter(id, std::move(caps),
                                               std::move(basic_info));
    }
    sandboxed_test_backend_->SetDefaultPrinterName(id);
  }

  ash::FakeCupsPrintersManager& printers_manager() {
    CHECK(printers_manager_);
    return *printers_manager_;
  }

  ash::LocalPrinter* local_printer() { return local_printer_.get(); }

  base::test::ScopedFeatureList& feature_list() { return feature_list_; }

#if BUILDFLAG(ENABLE_OOP_PRINTING)
  void SetTerminateServiceOnNextInteraction() {
    if (support_fallback_) {
      unsandboxed_print_backend_service_
          ->SetTerminateReceiverOnNextInteraction();
    }

    sandboxed_print_backend_service_->SetTerminateReceiverOnNextInteraction();
  }
#endif

  ash::printing::oauth2::MockAuthorizationZoneManager& auth_manager() {
    return *static_cast<testing::StrictMock<
        ash::printing::oauth2::MockAuthorizationZoneManager>*>(
        ash::printing::oauth2::AuthorizationZonesManagerFactory::
            GetForBrowserContext(profile_));
  }

  std::optional<std::string> GetOAuthAccessToken(
      const std::string& printer_id) {
    base::test::TestFuture<std::optional<std::string>> future;
    local_printer_->GetOAuthAccessToken(
        kAccountId, printer_id,
        base::BindOnce(&RecordOAuthTokenToFuture, base::Unretained(&future)));
    return future.Take();
  }

 private:
  const bool use_service_;
  const bool support_fallback_;
  const bool enable_oauth_;
  base::test::ScopedFeatureList feature_list_;
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<ash::test::TestUserSessionManager> test_user_session_manager_;
  std::unique_ptr<TestingProfileManager> profile_manager_;
  scoped_refptr<::printing::TestPrintBackend> sandboxed_test_backend_;
  scoped_refptr<::printing::TestPrintBackend> unsandboxed_test_backend_;
  std::unique_ptr<ash::LocalPrinter> local_printer_;
  raw_ptr<TestingProfile> profile_ = nullptr;
  raw_ptr<ash::FakeCupsPrintersManager> printers_manager_ = nullptr;
#if BUILDFLAG(ENABLE_OOP_PRINTING)
  mojo::Remote<::printing::mojom::PrintBackendService> sandboxed_test_remote_;
  mojo::Remote<::printing::mojom::PrintBackendService> unsandboxed_test_remote_;
  std::unique_ptr<::printing::PrintBackendServiceTestImpl>
      sandboxed_print_backend_service_;
  std::unique_ptr<::printing::PrintBackendServiceTestImpl>
      unsandboxed_print_backend_service_;
  ::printing::PrintBackendServiceManager::ClientId service_manager_client_id_;
#endif
};

class LocalPrinterImplTest : public LocalPrinterImplTestBase {
 public:
  LocalPrinterImplTest()
      : LocalPrinterImplTestBase(/*use_service=*/false,
                                 /*support_fallback=*/false) {}
  ~LocalPrinterImplTest() override = default;
};

class LocalPrinterImplProcessScopeTest
    : public LocalPrinterImplTestBase,
      public testing::WithParamInterface<bool> {
 public:
  LocalPrinterImplProcessScopeTest()
      : LocalPrinterImplTestBase(
            /*use_service=*/GetParam(),
            /*support_fallback=*/false) {}
  ~LocalPrinterImplProcessScopeTest() override = default;
};

#if BUILDFLAG(ENABLE_OOP_PRINTING)
class LocalPrinterImplServiceTest : public LocalPrinterImplTestBase {
 public:
  LocalPrinterImplServiceTest()
      : LocalPrinterImplTestBase(
            /*use_service=*/true,
            /*support_fallback=*/true) {}
  ~LocalPrinterImplServiceTest() override = default;
};

INSTANTIATE_TEST_SUITE_P(All,
                         LocalPrinterImplProcessScopeTest,
                         testing::Bool());
#else
INSTANTIATE_TEST_SUITE_P(/*no prefix */,
                         LocalPrinterImplProcessScopeTest,
                         testing::Values(false));
#endif

TEST_F(LocalPrinterImplTest, GetPrinters) {
  chromeos::Printer saved_printer =
      CreateTestPrinter("printer1", "saved", "description1");
  chromeos::Printer enterprise_printer =
      CreateEnterprisePrinter("printer2", "enterprise", "description2");
  chromeos::Printer automatic_printer =
      CreateTestPrinter("printer3", "automatic", "description3");

  printers_manager().AddPrinter(saved_printer, chromeos::PrinterClass::kSaved);
  printers_manager().AddPrinter(enterprise_printer,
                                chromeos::PrinterClass::kEnterprise);
  printers_manager().AddPrinter(automatic_printer,
                                chromeos::PrinterClass::kAutomatic);

  base::test::TestFuture<std::vector<chromeos::Printer>> printers_future;
  local_printer()->GetPrinters(kAccountId, printers_future.GetCallback());

  const std::vector<chromeos::Printer>& printers = printers_future.Get();
  ASSERT_EQ(3u, printers.size());

  EXPECT_EQ("printer1", printers[0].id());
  EXPECT_EQ("saved", printers[0].display_name());
  EXPECT_EQ("description1", printers[0].description());
  EXPECT_EQ(chromeos::Printer::SRC_USER_PREFS, printers[0].source());
  ASSERT_TRUE(printers[0].HasUri());
  EXPECT_EQ(kPrinterUri, printers[0].uri().GetNormalized());

  EXPECT_EQ("printer2", printers[1].id());
  EXPECT_EQ("enterprise", printers[1].display_name());
  EXPECT_EQ("description2", printers[1].description());
  EXPECT_EQ(chromeos::Printer::SRC_POLICY, printers[1].source());
  ASSERT_TRUE(printers[1].HasUri());
  EXPECT_EQ(kPrinterUri, printers[1].uri().GetNormalized());

  EXPECT_EQ("printer3", printers[2].id());
  EXPECT_EQ("automatic", printers[2].display_name());
  EXPECT_EQ("description3", printers[2].description());
  EXPECT_EQ(chromeos::Printer::SRC_USER_PREFS, printers[2].source());
  ASSERT_TRUE(printers[2].HasUri());
  EXPECT_EQ(kPrinterUri, printers[2].uri().GetNormalized());
}

TEST_F(LocalPrinterImplTest, GetPrinterTest) {
  chromeos::Printer saved_printer =
      CreateTestPrinter("printer1", "saved", "description1");
  printers_manager().AddPrinter(saved_printer, chromeos::PrinterClass::kSaved);

  std::optional<chromeos::Printer> printer =
      local_printer()->GetPrinter(kAccountId, "printer1");
  ASSERT_TRUE(printer);
  EXPECT_EQ("printer1", printer->id());
  EXPECT_EQ("saved", printer->display_name());
  EXPECT_EQ("description1", printer->description());
  EXPECT_EQ(chromeos::Printer::SRC_USER_PREFS, printer->source());
  ASSERT_TRUE(printer->HasUri());
  EXPECT_EQ(kPrinterUri, printer->uri().GetNormalized());
}

TEST_F(LocalPrinterImplTest, GetStatus) {
  chromeos::CupsPrinterStatus printer1("printer1");
  printer1.AddStatusReason(crosapi::mojom::StatusReason::Reason::kPaperJam,
                           crosapi::mojom::StatusReason::Severity::kError);
  printers_manager().SetPrinterStatus(printer1);

  std::optional<chromeos::CupsPrinterStatus> printer_status;
  local_printer()->GetStatus(
      kAccountId, "printer1",
      base::BindLambdaForTesting(
          [&](const chromeos::CupsPrinterStatus& status) {
            printer_status = status;
          }));
  ASSERT_TRUE(printer_status);
  EXPECT_EQ(printer1.GetPrinterId(), printer_status->GetPrinterId());
  EXPECT_EQ(printer1.GetTimestamp(), printer_status->GetTimestamp());
  EXPECT_EQ(printer1.GetStatusReasons(), printer_status->GetStatusReasons());
}

TEST_F(LocalPrinterImplTest, GetEulaUrlPrinterNotFound) {
  base::test::TestFuture<const GURL&> future;
  local_printer()->GetEulaUrl(kAccountId, "invalid_printer",
                              future.GetCallback());

  EXPECT_TRUE(future.Get().is_empty());
}

TEST_F(LocalPrinterImplTest, GetEulaUrlPrinterWithoutPpd) {
  chromeos::Printer saved_printer =
      CreateTestPrinter("printer1", "saved", "description1");
  printers_manager().AddPrinter(saved_printer, chromeos::PrinterClass::kSaved);

  base::test::TestFuture<const GURL&> future;
  local_printer()->GetEulaUrl(kAccountId, "printer1", future.GetCallback());

  // No effective_make_and_model -> NOT_FOUND -> empty url
  EXPECT_TRUE(future.Get().is_empty());
}

TEST_F(LocalPrinterImplTest, GetEulaUrlSuccess) {
  chromeos::Printer saved_printer =
      CreateTestPrinter("printer1", "saved", "description1");
  saved_printer.mutable_ppd_reference()->effective_make_and_model =
      "license_info";
  printers_manager().AddPrinter(saved_printer, chromeos::PrinterClass::kSaved);

  base::test::TestFuture<const GURL&> future;
  local_printer()->GetEulaUrl(kAccountId, "printer1", future.GetCallback());

  ASSERT_FALSE(future.Get().is_empty());
  EXPECT_EQ(future.Get().spec(), "chrome://os-credits/#license_info");
}

// Tests that fetching capabilities for non-installed printers is successful
// depending on its autoconf compatibility.
TEST_P(LocalPrinterImplProcessScopeTest, GetCapabilityForNonInstalledPrinters) {
  const std::string autoconf_printer_id = "printer1";
  chromeos::Printer autoconf_printer =
      CreateTestPrinter(autoconf_printer_id, "discovered", "description1");
  const std::string non_autoconf_printer_id = "printer2";
  chromeos::Printer non_autoconf_printer =
      CreateTestPrinter(non_autoconf_printer_id, "discovered", "description2");

  printers_manager().AddPrinter(autoconf_printer,
                                chromeos::PrinterClass::kDiscovered);
  printers_manager().AddPrinter(non_autoconf_printer,
                                chromeos::PrinterClass::kDiscovered);
  printers_manager().MarkPrinterAsNotAutoconfigurable(non_autoconf_printer_id);

  // Add printer capabilities to `test_backend_`.
  AddPrinter(autoconf_printer_id, "discovered", "description1",
             /*requires_elevated_permissions=*/false);
  AddPrinter(non_autoconf_printer_id, "discovered", "description2",
             /*requires_elevated_permissions=*/false);

  // Try to fetch capabilities for both printers but only the autoconf printer
  // should succeed.
  base::test::TestFuture<
      std::optional<chromeos::Printer>,
      std::optional<::printing::PrinterSemanticCapsAndDefaults>>
      autoconf_future;
  base::test::TestFuture<
      std::optional<chromeos::Printer>,
      std::optional<::printing::PrinterSemanticCapsAndDefaults>>
      non_autoconf_future;

  local_printer()->GetCapability(
      kAccountId, autoconf_printer_id,
      base::BindOnce(&RecordCapabilityToFuture,
                     base::Unretained(&autoconf_future)));

  local_printer()->GetCapability(
      kAccountId, non_autoconf_printer_id,
      base::BindOnce(&RecordCapabilityToFuture,
                     base::Unretained(&non_autoconf_future)));

  auto [autoconf_fetched_printer, autoconf_fetched_caps] =
      autoconf_future.Take();
  auto [non_autoconf_fetched_printer, non_autoconf_fetched_caps] =
      non_autoconf_future.Take();

  EXPECT_TRUE(autoconf_fetched_printer.has_value());
  EXPECT_TRUE(autoconf_fetched_caps.has_value());
  EXPECT_FALSE(non_autoconf_fetched_printer.has_value());
  EXPECT_FALSE(non_autoconf_fetched_caps.has_value());
}

// Tests that fetching capabilities for an existing installed printer is
// successful.
TEST_P(LocalPrinterImplProcessScopeTest, GetCapabilityValidPrinter) {
  chromeos::Printer saved_printer =
      CreateTestPrinter("printer1", "saved", "description1");
  printers_manager().AddPrinter(saved_printer, chromeos::PrinterClass::kSaved);
  printers_manager().MarkInstalled(saved_printer.id());

  // Add printer capabilities to `test_backend_`.
  AddPrinter("printer1", "saved", "description1",
             /*requires_elevated_permissions=*/false);

  base::test::TestFuture<
      std::optional<chromeos::Printer>,
      std::optional<::printing::PrinterSemanticCapsAndDefaults>>
      future;
  local_printer()->GetCapability(
      kAccountId, "printer1",
      base::BindOnce(&RecordCapabilityToFuture, base::Unretained(&future)));

  auto [fetched_printer, fetched_caps] = future.Take();

  ASSERT_TRUE(fetched_printer.has_value());
  EXPECT_FALSE(fetched_printer->HasSecureProtocol());
  EXPECT_EQ("printer1", fetched_printer->id());
  EXPECT_EQ("saved", fetched_printer->display_name());
  EXPECT_EQ("description1", fetched_printer->description());
  EXPECT_EQ(chromeos::Printer::SRC_USER_PREFS, fetched_printer->source());
  ASSERT_TRUE(fetched_printer->HasUri());
  EXPECT_EQ(kPrinterUri, fetched_printer->uri().GetNormalized());

  ASSERT_TRUE(fetched_caps.has_value());
  EXPECT_EQ(kPapers, fetched_caps->papers);
}

// Test that printers which have not yet been installed are installed with
// `SetUpPrinter` before their capabilities are fetched.
TEST_P(LocalPrinterImplProcessScopeTest, GetCapabilityPrinterNotInstalled) {
  chromeos::Printer discovered_printer =
      CreateTestPrinter("printer1", "discovered", "description1");
  // NOTE: The printer `discovered_printer` is not installed.
  printers_manager().AddPrinter(discovered_printer,
                                chromeos::PrinterClass::kDiscovered);

  // Add printer capabilities to `test_backend_`.
  AddPrinter("printer1", "discovered", "description1",
             /*requires_elevated_permissions=*/false);

  EXPECT_FALSE(printers_manager().IsPrinterInstalled(discovered_printer));

  base::test::TestFuture<
      std::optional<chromeos::Printer>,
      std::optional<::printing::PrinterSemanticCapsAndDefaults>>
      future;
  // Install printer and fetch capabilities.
  local_printer()->GetCapability(
      kAccountId, "printer1",
      base::BindOnce(&RecordCapabilityToFuture, base::Unretained(&future)));

  auto [fetched_printer, fetched_caps] = future.Take();

  EXPECT_TRUE(printers_manager().IsPrinterInstalled(discovered_printer));
  ASSERT_TRUE(fetched_printer.has_value());
  EXPECT_EQ("printer1", fetched_printer->id());
  ASSERT_TRUE(fetched_caps.has_value());
  EXPECT_EQ(kPapers, fetched_caps->papers);
}

// In this test we expect the `GetCapability` to bail early because the
// provided printer can't be found in the `CupsPrintersManager`.
TEST_P(LocalPrinterImplProcessScopeTest, GetCapabilityInvalidPrinter) {
  base::test::TestFuture<
      std::optional<chromeos::Printer>,
      std::optional<::printing::PrinterSemanticCapsAndDefaults>>
      future;
  local_printer()->GetCapability(
      kAccountId, "invalid printer",
      base::BindOnce(&RecordCapabilityToFuture, base::Unretained(&future)));

  auto [fetched_printer, fetched_caps] = future.Take();

  EXPECT_FALSE(fetched_printer.has_value());
  EXPECT_FALSE(fetched_caps.has_value());
}

// Tests that no capabilities are returned if a printer is unreachable from
// CUPS. We simulate this behavior by not calling AddPrinter(), which
// registers a printer with the test backend.
TEST_P(LocalPrinterImplProcessScopeTest, GetCapabilityUnreachablePrinter) {
  chromeos::Printer saved_printer =
      CreateTestPrinter("printer1", "saved", "description1");
  printers_manager().AddPrinter(saved_printer, chromeos::PrinterClass::kSaved);
  printers_manager().MarkInstalled(saved_printer.id());

  base::test::TestFuture<
      std::optional<chromeos::Printer>,
      std::optional<::printing::PrinterSemanticCapsAndDefaults>>
      future;
  local_printer()->GetCapability(
      kAccountId, "printer1",
      base::BindOnce(&RecordCapabilityToFuture, base::Unretained(&future)));

  auto [fetched_printer, fetched_caps] = future.Take();

  ASSERT_TRUE(fetched_printer.has_value());
  EXPECT_EQ("printer1", fetched_printer->id());
  EXPECT_FALSE(fetched_caps.has_value());
}

#if BUILDFLAG(ENABLE_OOP_PRINTING)
// Tests that fetching capabilities fails if the print backend service
// terminates early, such as it would from a crash.
TEST_F(LocalPrinterImplServiceTest, GetCapabilityTerminatedService) {
  chromeos::Printer saved_printer =
      CreateTestPrinter("printer1", "saved", "description1");
  printers_manager().AddPrinter(saved_printer, chromeos::PrinterClass::kSaved);
  printers_manager().MarkInstalled(saved_printer.id());

  // Add printer capabilities to `test_backend_`.
  AddPrinter("printer1", "saved", "description1",
             /*requires_elevated_permissions=*/false);

  // Set up for service to terminate on next use.
  SetTerminateServiceOnNextInteraction();

  base::test::TestFuture<
      std::optional<chromeos::Printer>,
      std::optional<::printing::PrinterSemanticCapsAndDefaults>>
      future;
  local_printer()->GetCapability(
      kAccountId, "printer1",
      base::BindOnce(&RecordCapabilityToFuture, base::Unretained(&future)));

  auto [fetched_printer, fetched_caps] = future.Take();

  ASSERT_TRUE(fetched_printer.has_value());
  EXPECT_EQ("printer1", fetched_printer->id());
  EXPECT_FALSE(fetched_caps.has_value());
}
#endif  // BUILDFLAG(ENABLE_OOP_PRINTING)

// Test that installed printers to which the user does not have permission to
// access will receive a dictionary for the capabilities but will not have any
// settings in that.
TEST_P(LocalPrinterImplProcessScopeTest, GetCapabilityAccessDenied) {
  chromeos::Printer saved_printer =
      CreateTestPrinter("printer1", "saved", "description1");
  printers_manager().AddPrinter(saved_printer, chromeos::PrinterClass::kSaved);
  printers_manager().MarkInstalled(saved_printer.id());

  // Add printer capabilities to `test_backend_`.
  AddPrinter("printer1", "saved", "description1",
             /*requires_elevated_permissions=*/true);

  base::test::TestFuture<
      std::optional<chromeos::Printer>,
      std::optional<::printing::PrinterSemanticCapsAndDefaults>>
      future;
  local_printer()->GetCapability(
      kAccountId, "printer1",
      base::BindOnce(&RecordCapabilityToFuture, base::Unretained(&future)));

  auto [fetched_printer, fetched_caps] = future.Take();

  ASSERT_TRUE(fetched_printer.has_value());
  EXPECT_EQ("printer1", fetched_printer->id());
  EXPECT_FALSE(fetched_caps.has_value());
}

#if BUILDFLAG(ENABLE_OOP_PRINTING)
TEST_F(LocalPrinterImplServiceTest, GetCapabilityElevatedPermissionsSucceeds) {
  chromeos::Printer saved_printer =
      CreateTestPrinter("printer1", "saved", "description1");
  printers_manager().AddPrinter(saved_printer, chromeos::PrinterClass::kSaved);
  printers_manager().MarkInstalled(saved_printer.id());

  // Add printer capabilities to `test_backend_`.
  AddPrinter("printer1", "saved", "description1",
             /*requires_elevated_permissions=*/true);

  // Note that printer does not initially show as requiring elevated privileges.
  EXPECT_FALSE(::printing::PrintBackendServiceManager::GetInstance()
                   .PrinterDriverFoundToRequireElevatedPrivilege("printer1"));

  base::test::TestFuture<
      std::optional<chromeos::Printer>,
      std::optional<::printing::PrinterSemanticCapsAndDefaults>>
      future;
  local_printer()->GetCapability(
      kAccountId, "printer1",
      base::BindOnce(&RecordCapabilityToFuture, base::Unretained(&future)));

  auto [fetched_printer, fetched_caps] = future.Take();

  // Verify that this printer now shows up as requiring elevated privileges.
  EXPECT_TRUE(::printing::PrintBackendServiceManager::GetInstance()
                  .PrinterDriverFoundToRequireElevatedPrivilege("printer1"));

  // Getting capabilities should succeed when fallback is supported.
  ASSERT_TRUE(fetched_printer.has_value());
  EXPECT_EQ("printer1", fetched_printer->id());
  ASSERT_TRUE(fetched_caps.has_value());
  EXPECT_EQ(kPapers, fetched_caps->papers);
}
#endif  // BUILDFLAG(ENABLE_OOP_PRINTING)

class LocalPrinterImplWithOAuth2Test : public LocalPrinterImplTestBase {
 public:
  LocalPrinterImplWithOAuth2Test()
      : LocalPrinterImplTestBase(
            /*use_service=*/false,
            /*support_fallback=*/false,
            /*enable_oauth=*/true) {}
  ~LocalPrinterImplWithOAuth2Test() override = default;
};

TEST_F(LocalPrinterImplWithOAuth2Test, GetOAuthAccessTokenUnknownPrinter) {
  const std::optional<std::string> result = GetOAuthAccessToken("printer_id");
  EXPECT_FALSE(result);
}

TEST_F(LocalPrinterImplWithOAuth2Test, GetOAuthAccessTokenNonOAuthPrinter) {
  chromeos::Printer saved_printer =
      CreateTestPrinter("printer_id", "saved", "description1");
  printers_manager().AddPrinter(saved_printer, chromeos::PrinterClass::kSaved);

  chromeos::CupsPrinterStatus printer_status("printer_id");
  printers_manager().SetPrinterStatus(printer_status);

  const std::optional<std::string> result = GetOAuthAccessToken("printer_id");
  EXPECT_FALSE(result);
}

TEST_F(LocalPrinterImplWithOAuth2Test,
       GetOAuthAccessTokenOAuthConnectionError) {
  chromeos::Printer saved_printer =
      CreateTestPrinter("printer_id", "saved", "description1");
  printers_manager().AddPrinter(saved_printer, chromeos::PrinterClass::kSaved);

  chromeos::CupsPrinterStatus printer_status("printer_id");
  printer_status.SetAuthenticationInfo({"https://server/url", "scope"});
  printers_manager().SetPrinterStatus(printer_status);

  EXPECT_CALL(auth_manager(), GetEndpointAccessToken(testing::_, testing::_,
                                                     "scope", testing::_))
      .WillOnce([](const GURL& auth_server, const chromeos::Uri& ipp_endpoint,
                   const std::string& scope,
                   ash::printing::oauth2::StatusCallback callback) {
        EXPECT_EQ(auth_server.spec(), "https://server/url");
        std::move(callback).Run(
            ash::printing::oauth2::StatusCode::kConnectionError,
            "error_message");
      });

  const std::optional<std::string> result = GetOAuthAccessToken("printer_id");
  EXPECT_FALSE(result);
}

TEST_F(LocalPrinterImplWithOAuth2Test, GetOAuthAccessTokenSuccess) {
  chromeos::Printer saved_printer =
      CreateTestPrinter("printer_id", "saved", "description1");
  printers_manager().AddPrinter(saved_printer, chromeos::PrinterClass::kSaved);

  chromeos::CupsPrinterStatus printer_status("printer_id");
  printer_status.SetAuthenticationInfo({"https://server/url", "scope"});
  printers_manager().SetPrinterStatus(printer_status);

  EXPECT_CALL(auth_manager(), GetEndpointAccessToken(testing::_, testing::_,
                                                     "scope", testing::_))
      .WillOnce([](const GURL& auth_server, const chromeos::Uri& ipp_endpoint,
                   const std::string& scope,
                   ash::printing::oauth2::StatusCallback callback) {
        EXPECT_EQ(auth_server.spec(), "https://server/url");
        std::move(callback).Run(ash::printing::oauth2::StatusCode::kOK,
                                "access_token");
      });

  const std::optional<std::string> result = GetOAuthAccessToken("printer_id");
  ASSERT_TRUE(result);
  EXPECT_EQ(result.value(), "access_token");
}

}  // namespace ash
