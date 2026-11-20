// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "chrome/browser/glic/host/glic_drive_picker_manager.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/browser/glic/public/glic_api_metrics.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/test_support/glic_api_test.h"
#include "chrome/browser/glic/test_support/glic_histogram_tester.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "chrome/browser/ui/views/drive_picker_host/drive_picker_host_controller.h"
#include "chrome/browser/ui/views/drive_picker_host/drive_picker_result_handler.mojom.h"
#include "chrome/browser/ui/webui/drive_picker_host/drive_picker_host_request.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "content/public/test/browser_test.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace glic {
namespace {

class FakeDrivePickerHostController : public DrivePickerHostController {
 public:
  FakeDrivePickerHostController(
      Profile* profile,
      BrowserWindowInterface* browser,
      views::Widget* anchor_widget,
      base::RepeatingCallback<void(FakeDrivePickerHostController*)> on_show)
      : DrivePickerHostController(profile, browser, anchor_widget),
        on_show_(std::move(on_show)) {}
  ~FakeDrivePickerHostController() override = default;

  void ShowDrivePickerHost(
      std::unique_ptr<drive_picker_host::DrivePickerHostRequest> request)
      override {
    result_handler_.Bind(request->TakeResultHandler());
    if (on_show_) {
      on_show_.Run(this);
    }
  }

  mojo::Remote<drive_picker_host::mojom::DrivePickerResultHandler>&
  result_handler() {
    return result_handler_;
  }
  base::WeakPtr<FakeDrivePickerHostController> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

 private:
  base::RepeatingCallback<void(FakeDrivePickerHostController*)> on_show_;
  mojo::Remote<drive_picker_host::mojom::DrivePickerResultHandler>
      result_handler_;
  base::WeakPtrFactory<FakeDrivePickerHostController> weak_factory_{this};
};

class GlicApiDrivePickerBrowserTest : public GlicApiBrowserTest {
 public:
  GlicApiDrivePickerBrowserTest()
      : GlicApiBrowserTest(
            GlicTestJsPath("./glic_api_drive_picker_browsertest.js")) {
    feature_list_.InitAndEnableFeature(features::kGlicDrivePicker);
  }

  void TearDownOnMainThread() override {
    if (service()) {
      if (auto* manager = service()->drive_picker_manager()) {
        manager->SetControllerFactoryForTesting(
            GlicDrivePickerManager::ControllerFactory());
      }
    }
    GlicApiBrowserTest::TearDownOnMainThread();
  }

 protected:
  std::string PrimaryAccountGaiaId() {
    return IdentityManagerFactory::GetForProfile(GetProfile())
        ->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin)
        .gaia.ToString();
  }

  void InstallFakeDrivePickerController(
      base::RepeatingCallback<void(FakeDrivePickerHostController*)> on_show) {
    service()->drive_picker_manager()->SetControllerFactoryForTesting(
        base::BindRepeating(
            [](base::RepeatingCallback<void(FakeDrivePickerHostController*)>
                   callback,
               Profile* profile, BrowserWindowInterface* browser,
               views::Widget* anchor_widget)
                -> std::unique_ptr<DrivePickerHostController> {
              return std::make_unique<FakeDrivePickerHostController>(
                  profile, browser, anchor_widget, std::move(callback));
            },
            std::move(on_show)));
  }

  base::WeakPtr<FakeDrivePickerHostController> active_drive_picker_controller_;

 private:
  base::test::ScopedFeatureList feature_list_;
};

class GlicApiDrivePickerDisabledBrowserTest : public GlicApiBrowserTest {
 public:
  GlicApiDrivePickerDisabledBrowserTest()
      : GlicApiBrowserTest(
            GlicTestJsPath("./glic_api_drive_picker_browsertest.js")) {
    feature_list_.InitAndDisableFeature(features::kGlicDrivePicker);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(GlicApiDrivePickerBrowserTest, testShowDrivePicker) {
  InstallFakeDrivePickerController(
      base::BindRepeating([](FakeDrivePickerHostController* controller) {
        std::vector<drive_picker_host::mojom::DriveFilePtr> files;
        auto pdf = drive_picker_host::mojom::DriveFile::New();
        pdf->id = "file-1";
        pdf->name = "Q3 Report.pdf";
        pdf->mime_type = "application/pdf";
        pdf->type = "document";
        pdf->size_bytes = 4096;
        pdf->resource_key = "rk-1";
        files.push_back(std::move(pdf));

        auto doc = drive_picker_host::mojom::DriveFile::New();
        doc->id = "doc-2";
        doc->name = "Notes";
        doc->mime_type = "application/vnd.google-apps.document";
        doc->type = "document";
        doc->size_bytes = 0;
        files.push_back(std::move(doc));

        controller->result_handler()->OnSelection(std::move(files));
      }));
  ASSERT_OK(OpenGlicForActiveTab());
  ExecuteJsTest({.params = base::Value(PrimaryAccountGaiaId())});
}

IN_PROC_BROWSER_TEST_F(GlicApiDrivePickerBrowserTest,
                       testShowDrivePickerCancel) {
  InstallFakeDrivePickerController(
      base::BindRepeating([](FakeDrivePickerHostController* controller) {
        controller->result_handler()->OnCancel();
      }));
  ASSERT_OK(OpenGlicForActiveTab());
  ExecuteJsTest({.params = base::Value(PrimaryAccountGaiaId())});
}

IN_PROC_BROWSER_TEST_F(GlicApiDrivePickerBrowserTest,
                       testShowDrivePickerCancelledOnPanelClose) {
  InstallFakeDrivePickerController(base::BindRepeating(
      [](base::WeakPtr<FakeDrivePickerHostController>* out_controller,
         FakeDrivePickerHostController* controller) {
        *out_controller = controller->GetWeakPtr();
      },
      base::Unretained(&active_drive_picker_controller_)));
  ASSERT_OK(OpenGlicForActiveTab());
  PreventDeletionOnClose();
  ExecuteJsTest({.params = base::Value(PrimaryAccountGaiaId())});

  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return active_drive_picker_controller_ != nullptr; }));
  ASSERT_OK(CloseGlicForTabAndWait(GetTabListInterface()->GetActiveTab()));
  EXPECT_FALSE(active_drive_picker_controller_);
  ContinueJsTest();
}

IN_PROC_BROWSER_TEST_F(GlicApiDrivePickerBrowserTest,
                       testShowDrivePickerAlreadyActive) {
  InstallFakeDrivePickerController(base::BindRepeating(
      [](base::WeakPtr<FakeDrivePickerHostController>* out_controller,
         FakeDrivePickerHostController* controller) {
        *out_controller = controller->GetWeakPtr();
      },
      base::Unretained(&active_drive_picker_controller_)));
  ASSERT_OK(OpenGlicForActiveTab());
  ExecuteJsTest({.params = base::Value(PrimaryAccountGaiaId())});

  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return active_drive_picker_controller_ != nullptr; }));
  ContinueJsTest();

  ASSERT_TRUE(active_drive_picker_controller_);
  std::vector<drive_picker_host::mojom::DriveFilePtr> files;
  auto pdf = drive_picker_host::mojom::DriveFile::New();
  pdf->id = "file-1";
  pdf->name = "Q3 Report.pdf";
  pdf->mime_type = "application/pdf";
  pdf->type = "document";
  pdf->size_bytes = 4096;
  files.push_back(std::move(pdf));
  active_drive_picker_controller_->result_handler()->OnSelection(
      std::move(files));
  ContinueJsTest();
}

IN_PROC_BROWSER_TEST_F(GlicApiDrivePickerBrowserTest,
                       testShowDrivePickerAccountMismatch) {
  glic::GlicHistogramTester histogram_tester;
  ASSERT_OK(OpenGlicForActiveTab());
  ExecuteJsTest();
  histogram_tester.ExpectBucketCount("Glic.Api.RequestCounts.ShowDrivePicker",
                                     mojom::GlicRequestEvent::kRequestReceived,
                                     1);
  histogram_tester.ExpectBucketCount(
      "Glic.Api.StatusCounts.Received",
      glic::GlicHostApiRequestId::kShowDrivePicker, 1);
}

IN_PROC_BROWSER_TEST_F(GlicApiDrivePickerBrowserTest,
                       testShowDrivePickerDisabledByPolicy) {
  GetProfile()->GetPrefs()->SetInteger(
      glic::prefs::kGlicFileUploadAllowed,
      std::to_underlying(glic::prefs::GlicFileUploadPolicyState::kDisabled));
  ASSERT_OK(OpenGlicForActiveTab());
  ExecuteJsTest();
}

IN_PROC_BROWSER_TEST_F(GlicApiDrivePickerDisabledBrowserTest,
                       testShowDrivePickerUnavailable) {
  ASSERT_OK(OpenGlicForActiveTab());
  ExecuteJsTest();
}

}  // namespace
}  // namespace glic
