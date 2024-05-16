// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/telemetry/api/routines/diagnostic_routine_manager.h"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/check_deref.h"
#include "base/containers/flat_map.h"
#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "base/test/test_future.h"
#include "base/types/expected.h"
#include "base/uuid.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/chromeos/extensions/telemetry/api/routines/diagnostic_routine.h"
#include "chrome/browser/chromeos/extensions/telemetry/api/routines/fake_diagnostic_routines_service.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chromeos/crosapi/mojom/telemetry_diagnostic_routine_service.mojom.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/ssl_status.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/unloaded_extension_reason.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/extension_id.h"
#include "net/base/net_errors.h"
#include "net/cert/cert_status_flags.h"
#include "net/cert/x509_certificate.h"
#include "net/test/cert_test_util.h"
#include "net/test/test_data_directory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/chromeos/extensions/telemetry/api/routines/fake_diagnostic_routines_service_factory.h"
#include "chromeos/ash/components/telemetry_extension/routines/telemetry_diagnostic_routine_service_ash.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/lacros/lacros_service.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

namespace chromeos {

namespace {

namespace crosapi = ::crosapi::mojom;

constexpr char kExtensionId1[] = "gogonhoemckpdpadfnjnpgbjpbjnodgc";
constexpr char kPwaPattern1[] = "*://googlechromelabs.github.io/*";
constexpr char kPwaUrl1[] = "https://googlechromelabs.github.io/";

constexpr char kExtensionId2[] = "alnedpmllcfpgldkagbfbjkloonjlfjb";
constexpr char kPwaPattern2[] = "https://hpcs-appschr.hpcloud.hp.com/*";
constexpr char kPwaUrl2[] = "https://hpcs-appschr.hpcloud.hp.com";

constexpr char kUnmappedUuid[] = "41976e88-b067-476f-9e1e-3ec47b6959af";

}  // namespace

class TelemetryExtensionDiagnosticRoutinesManagerTest
    : public BrowserWithTestWindowTest {
 public:
  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();

#if BUILDFLAG(IS_CHROMEOS_ASH)
    fake_routines_service_impl_ = new FakeDiagnosticRoutinesService();
    fake_routines_service_factory_.SetCreateInstanceResponse(
        std::unique_ptr<FakeDiagnosticRoutinesService>(
            fake_routines_service_impl_.get()));
    ash::TelemetryDiagnosticsRoutineServiceAsh::Factory::SetForTesting(
        &fake_routines_service_factory_);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
#if BUILDFLAG(IS_CHROMEOS_LACROS)
    fake_routines_service_impl_ =
        std::make_unique<FakeDiagnosticRoutinesService>();
    // Replace the production `TelemetryDiagnosticRoutinesService` with a fake
    // for testing.
    chromeos::LacrosService::Get()->InjectRemoteForTesting(
        fake_routines_service_impl_->receiver().BindNewPipeAndPassRemote());
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
  }

  void TearDown() override {
#if BUILDFLAG(IS_CHROMEOS_ASH)
    fake_routines_service_impl_ = nullptr;
#endif
    BrowserWithTestWindowTest::TearDown();
  }

 protected:
  void OpenAppUiUrlAndSetCertificateWithStatus(const GURL& url,
                                               net::CertStatus cert_status) {
    const base::FilePath certs_dir = net::GetTestCertsDirectory();
    scoped_refptr<net::X509Certificate> test_cert(
        net::ImportCertFromFile(certs_dir, "ok_cert.pem"));
    ASSERT_TRUE(test_cert);

    AddTab(browser(), url);

    // AddTab() adds a new tab at index 0.
    auto* web_contents = browser()->tab_strip_model()->GetWebContentsAt(0);
    auto* entry = web_contents->GetController().GetVisibleEntry();
    content::SSLStatus& ssl = entry->GetSSL();
    ssl.certificate = test_cert;
    ssl.cert_status = cert_status;
  }

  scoped_refptr<const extensions::Extension> CreateExtension(
      const std::string& extension_id,
      const std::vector<std::string> external_connectables) {
    base::Value::List matches;
    for (const auto& match : external_connectables) {
      matches.Append(match);
    }
    auto extension =
        extensions::ExtensionBuilder("Test ChromeOS System Extension")
            .SetManifestVersion(3)
            .SetManifestKey("chromeos_system_extension", base::Value::Dict())
            .SetManifestKey(
                "externally_connectable",
                base::Value::Dict().Set("matches", std::move(matches)))
            .SetID(extension_id)
            .SetLocation(extensions::mojom::ManifestLocation::kInternal)
            .Build();
    extensions::ExtensionRegistry::Get(profile())->AddEnabled(extension);

    return extension;
  }

  DiagnosticRoutineManager& routine_manager() {
    return CHECK_DEREF(DiagnosticRoutineManager::Get(profile()));
  }

  FakeDiagnosticRoutinesService& fake_service() {
    return CHECK_DEREF(fake_routines_service_impl_.get());
  }

  crosapi::TelemetryDiagnosticRoutineArgumentPtr GetMemoryArgument() {
    auto memory_arg = crosapi::TelemetryDiagnosticMemoryRoutineArgument::New();
    memory_arg->max_testing_mem_kib = 42;
    return crosapi::TelemetryDiagnosticRoutineArgument::NewMemory(
        std::move(memory_arg));
  }

  base::flat_map<extensions::ExtensionId, std::unique_ptr<AppUiObserver>>&
  app_ui_observers() {
    return routine_manager().app_ui_observers_;
  }

  bool IsUuidRegisteredForExtension(extensions::ExtensionId extension_id,
                                    base::Uuid uuid) {
    const auto& routines_per_extension =
        routine_manager().routines_per_extension_;
    auto it = routines_per_extension.find(extension_id);
    if (it == routines_per_extension.end()) {
      return false;
    }

    auto found_uuid =
        std::find_if(it->second.begin(), it->second.end(),
                     [uuid](const std::unique_ptr<DiagnosticRoutine>& elem) {
                       return elem->info_.uuid == uuid;
                     });
    return found_uuid != it->second.end();
  }

 private:
#if BUILDFLAG(IS_CHROMEOS_ASH)
  raw_ptr<FakeDiagnosticRoutinesService> fake_routines_service_impl_;
  FakeDiagnosticRoutinesServiceFactory fake_routines_service_factory_;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  std::unique_ptr<FakeDiagnosticRoutinesService> fake_routines_service_impl_;
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
};

TEST_F(TelemetryExtensionDiagnosticRoutinesManagerTest,
       CreateRoutineNoExtension) {
  EXPECT_EQ(
      base::unexpected(DiagnosticRoutineManager::kExtensionUnloaded),
      routine_manager().CreateRoutine(kExtensionId1, GetMemoryArgument()));
}

TEST_F(TelemetryExtensionDiagnosticRoutinesManagerTest,
       CreateRoutineAppUiClosed) {
  CreateExtension(kExtensionId1, {kPwaPattern1});

  EXPECT_EQ(
      base::unexpected(DiagnosticRoutineManager::kAppUiClosed),
      routine_manager().CreateRoutine(kExtensionId1, GetMemoryArgument()));
}

TEST_F(TelemetryExtensionDiagnosticRoutinesManagerTest, CreateRoutineSuccess) {
  CreateExtension(kExtensionId1, {kPwaPattern1});

  OpenAppUiUrlAndSetCertificateWithStatus(GURL(kPwaUrl1),
                                          /*cert_status=*/net::OK);
  auto create_result =
      routine_manager().CreateRoutine(kExtensionId1, GetMemoryArgument());

  fake_service().FlushForTesting();

  EXPECT_TRUE(create_result.has_value());
  EXPECT_TRUE(app_ui_observers().contains(kExtensionId1));

  auto* control = fake_service().GetCreatedRoutineControlForRoutineType(
      crosapi::TelemetryDiagnosticRoutineArgument::Tag::kMemory);
  ASSERT_TRUE(control);
  EXPECT_TRUE(control->receiver().is_bound());

  base::test::TestFuture<void> future;
  control->receiver().set_disconnect_handler(future.GetCallback());

  // Closing the tab cuts the observation.
  browser()->tab_strip_model()->CloseWebContentsAt(0,
                                                   TabCloseTypes::CLOSE_NONE);

  fake_service().FlushForTesting();

  EXPECT_FALSE(app_ui_observers().contains(kExtensionId1));
  EXPECT_FALSE(
      IsUuidRegisteredForExtension(kExtensionId1, create_result.value()));

  EXPECT_TRUE(future.Wait());
}

TEST_F(TelemetryExtensionDiagnosticRoutinesManagerTest,
       CreateRoutineTwoExtension) {
  CreateExtension(kExtensionId1, {kPwaPattern1});
  CreateExtension(kExtensionId2, {kPwaPattern2});

  // Open app UI for extension 1.
  OpenAppUiUrlAndSetCertificateWithStatus(GURL(kPwaUrl1),
                                          /*cert_status=*/net::OK);
  auto create_result =
      routine_manager().CreateRoutine(kExtensionId1, GetMemoryArgument());

  fake_service().FlushForTesting();

  EXPECT_TRUE(create_result.has_value());
  EXPECT_TRUE(app_ui_observers().contains(kExtensionId1));

  EXPECT_EQ(
      base::unexpected(DiagnosticRoutineManager::kAppUiClosed),
      routine_manager().CreateRoutine(kExtensionId2, GetMemoryArgument()));
  EXPECT_TRUE(app_ui_observers().contains(kExtensionId1));
  EXPECT_FALSE(app_ui_observers().contains(kExtensionId2));

  // Open app UI for extension 2.
  OpenAppUiUrlAndSetCertificateWithStatus(GURL(kPwaUrl2),
                                          /*cert_status=*/net::OK);
  auto create_result_id2 =
      routine_manager().CreateRoutine(kExtensionId2, GetMemoryArgument());

  fake_service().FlushForTesting();

  EXPECT_TRUE(create_result_id2.has_value());
  EXPECT_TRUE(app_ui_observers().contains(kExtensionId1));
  EXPECT_TRUE(app_ui_observers().contains(kExtensionId2));

  // Close the app UI of extension 1.
  browser()->tab_strip_model()->CloseWebContentsAt(1,
                                                   TabCloseTypes::CLOSE_NONE);

  fake_service().FlushForTesting();

  EXPECT_FALSE(app_ui_observers().contains(kExtensionId1));
  EXPECT_TRUE(app_ui_observers().contains(kExtensionId2));
  EXPECT_FALSE(
      IsUuidRegisteredForExtension(kExtensionId1, create_result.value()));

  // Close the app UI of extension 2.
  browser()->tab_strip_model()->CloseWebContentsAt(0,
                                                   TabCloseTypes::CLOSE_NONE);

  fake_service().FlushForTesting();

  EXPECT_FALSE(app_ui_observers().contains(kExtensionId1));
  EXPECT_FALSE(app_ui_observers().contains(kExtensionId2));
  EXPECT_FALSE(
      IsUuidRegisteredForExtension(kExtensionId2, create_result_id2.value()));
}

TEST_F(TelemetryExtensionDiagnosticRoutinesManagerTest,
       CreateRoutineMultipleTabsOpen) {
  CreateExtension(kExtensionId1, {kPwaPattern1});

  OpenAppUiUrlAndSetCertificateWithStatus(GURL(kPwaUrl1),
                                          /*cert_status=*/net::OK);
  auto create_result =
      routine_manager().CreateRoutine(kExtensionId1, GetMemoryArgument());

  fake_service().FlushForTesting();

  EXPECT_TRUE(create_result.has_value());
  EXPECT_TRUE(app_ui_observers().contains(kExtensionId1));

  // Open second tab.
  OpenAppUiUrlAndSetCertificateWithStatus(GURL(kPwaUrl1),
                                          /*cert_status=*/net::OK);

  EXPECT_TRUE(app_ui_observers().contains(kExtensionId1));

  // Close the first tab (index 1). The observer shouldn't be cut.
  browser()->tab_strip_model()->CloseWebContentsAt(1,
                                                   TabCloseTypes::CLOSE_NONE);
  EXPECT_TRUE(app_ui_observers().contains(kExtensionId1));

  // Closing the second tab (the last one) cuts the observation.
  browser()->tab_strip_model()->CloseWebContentsAt(0,
                                                   TabCloseTypes::CLOSE_NONE);
  EXPECT_FALSE(app_ui_observers().contains(kExtensionId1));
  EXPECT_FALSE(
      IsUuidRegisteredForExtension(kExtensionId1, create_result.value()));
}

TEST_F(TelemetryExtensionDiagnosticRoutinesManagerTest,
       UnloadExtensionSuccess) {
  auto extension = CreateExtension(kExtensionId1, {kPwaPattern1});

  OpenAppUiUrlAndSetCertificateWithStatus(GURL(kPwaUrl1),
                                          /*cert_status=*/net::OK);
  auto create_result =
      routine_manager().CreateRoutine(kExtensionId1, GetMemoryArgument());

  fake_service().FlushForTesting();

  EXPECT_TRUE(create_result.has_value());
  EXPECT_TRUE(app_ui_observers().contains(kExtensionId1));
  auto* control = fake_service().GetCreatedRoutineControlForRoutineType(
      crosapi::TelemetryDiagnosticRoutineArgument::Tag::kMemory);
  EXPECT_TRUE(control);
  base::test::TestFuture<void> future;
  control->receiver().set_disconnect_handler(future.GetCallback());

  ASSERT_TRUE(extensions::ExtensionRegistry::Get(profile())->RemoveEnabled(
      kExtensionId1));
  extensions::ExtensionRegistry::Get(profile())->TriggerOnUnloaded(
      extension.get(), extensions::UnloadedExtensionReason::TERMINATE);

  fake_service().FlushForTesting();

  auto create_result_2 =
      routine_manager().CreateRoutine(kExtensionId1, GetMemoryArgument());

  EXPECT_EQ(
      create_result_2,
      base::unexpected(DiagnosticRoutineManager::Error::kExtensionUnloaded));
  EXPECT_FALSE(app_ui_observers().contains(kExtensionId1));
  EXPECT_TRUE(future.Wait());
}

TEST_F(TelemetryExtensionDiagnosticRoutinesManagerTest,
       StartRoutineNoExtension) {
  EXPECT_FALSE(routine_manager().StartRoutineForExtension(
      kExtensionId1, base::Uuid::ParseLowercase(kUnmappedUuid)));
}

TEST_F(TelemetryExtensionDiagnosticRoutinesManagerTest, StartRoutineNoRoutine) {
  CreateExtension(kExtensionId1, {kPwaPattern1});

  EXPECT_FALSE(routine_manager().StartRoutineForExtension(
      kExtensionId1, base::Uuid::ParseLowercase(kUnmappedUuid)));
}

TEST_F(TelemetryExtensionDiagnosticRoutinesManagerTest, StartRoutineSuccess) {
  CreateExtension(kExtensionId1, {kPwaPattern1});
  OpenAppUiUrlAndSetCertificateWithStatus(GURL(kPwaUrl1),
                                          /*cert_status=*/net::OK);

  auto create_result =
      routine_manager().CreateRoutine(kExtensionId1, GetMemoryArgument());
  EXPECT_TRUE(create_result.has_value());

  EXPECT_TRUE(routine_manager().StartRoutineForExtension(
      kExtensionId1, create_result.value()));
}

TEST_F(TelemetryExtensionDiagnosticRoutinesManagerTest, CancelRoutineSuccess) {
  CreateExtension(kExtensionId1, {kPwaPattern1});
  OpenAppUiUrlAndSetCertificateWithStatus(GURL(kPwaUrl1),
                                          /*cert_status=*/net::OK);

  auto create_result =
      routine_manager().CreateRoutine(kExtensionId1, GetMemoryArgument());
  EXPECT_TRUE(create_result.has_value());

  EXPECT_TRUE(
      IsUuidRegisteredForExtension(kExtensionId1, create_result.value()));

  routine_manager().CancelRoutineForExtension(kExtensionId1,
                                              create_result.value());
  EXPECT_FALSE(
      IsUuidRegisteredForExtension(kExtensionId1, create_result.value()));
}

TEST_F(TelemetryExtensionDiagnosticRoutinesManagerTest,
       ReplyToRoutineInquiryNoExtension) {
  EXPECT_FALSE(routine_manager().ReplyToRoutineInquiryForExtension(
      kExtensionId1, base::Uuid::ParseLowercase(kUnmappedUuid),
      crosapi::TelemetryDiagnosticRoutineInquiryReply::NewUnrecognizedReply(
          true)));
}

TEST_F(TelemetryExtensionDiagnosticRoutinesManagerTest,
       ReplyToRoutineInquiryNoRoutine) {
  CreateExtension(kExtensionId1, {kPwaPattern1});

  EXPECT_FALSE(routine_manager().ReplyToRoutineInquiryForExtension(
      kExtensionId1, base::Uuid::ParseLowercase(kUnmappedUuid),
      crosapi::TelemetryDiagnosticRoutineInquiryReply::NewUnrecognizedReply(
          true)));
}

TEST_F(TelemetryExtensionDiagnosticRoutinesManagerTest,
       ReplyToRoutineInquirySuccess) {
  CreateExtension(kExtensionId1, {kPwaPattern1});
  OpenAppUiUrlAndSetCertificateWithStatus(GURL(kPwaUrl1),
                                          /*cert_status=*/net::OK);

  auto create_result =
      routine_manager().CreateRoutine(kExtensionId1, GetMemoryArgument());
  EXPECT_TRUE(create_result.has_value());

  EXPECT_TRUE(routine_manager().ReplyToRoutineInquiryForExtension(
      kExtensionId1, create_result.value(),
      crosapi::TelemetryDiagnosticRoutineInquiryReply::NewUnrecognizedReply(
          true)));
}

}  // namespace chromeos
