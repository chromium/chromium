// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/host/glic_drive_picker_manager.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/types/expected.h"
#include "base/unguessable_token.h"
#include "build/build_config.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/public/glic_provider.h"
#include "chrome/browser/glic/public/glic_provider_latch.h"
#include "chrome/browser/glic/test_support/glic_test_util.h"
#include "chrome/browser/global_features.h"
#include "chrome/browser/signin/chrome_signin_client_factory.h"
#include "chrome/browser/signin/chrome_signin_client_test_util.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/ui/browser_window/test/fake_global_browser_collection.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/test_tab_strip_model_delegate.h"
#include "chrome/browser/ui/views/drive_picker_host/drive_picker_host_controller.h"
#include "chrome/browser/ui/views/drive_picker_host/drive_picker_result_handler.mojom.h"
#include "chrome/browser/ui/webui/drive_picker_host/drive_picker_host_request.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/account_capabilities_test_mutator.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_task_environment.h"
#include "google_apis/gaia/gaia_id.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/test/mock_base_window.h"
#include "ui/views/widget/widget.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/ash/test/glic_user_session_test_helper.h"
#include "chromeos/constants/chromeos_features.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

namespace glic {
namespace {

using ShowResult =
    base::expected<std::vector<GlicDriveFileSelection>, GlicDrivePickerError>;
using drive_picker_host::DrivePickerHostRequest;
using drive_picker_host::mojom::DriveFile;
using drive_picker_host::mojom::DriveFilePtr;
using drive_picker_host::mojom::DrivePickerError;
using drive_picker_host::mojom::DrivePickerResultHandler;

constexpr char kOutcomeHistogram[] = "Glic.DrivePicker.Outcome";

DriveFilePtr MakeValidDriveFile(
    const std::string& id,
    const std::string& name = "Report.pdf",
    const std::string& mime_type = "application/pdf",
    uint64_t size_bytes = 1024,
    std::optional<std::string> resource_key = std::nullopt) {
  auto file = DriveFile::New();
  file->id = id;
  file->name = name;
  file->mime_type = mime_type;
  file->type = "document";
  file->size_bytes = size_bytes;
  file->resource_key = std::move(resource_key);
  return file;
}

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
      std::unique_ptr<DrivePickerHostRequest> request) override {
    request_type_ = request->type();
    request_style_ = request->style();
    result_handler_.Bind(request->TakeResultHandler());
    if (on_show_) {
      on_show_.Run(this);
    }
  }

  DrivePickerHostRequest::RequestType request_type() const {
    return request_type_;
  }
  DrivePickerHostRequest::Style request_style() const { return request_style_; }
  mojo::Remote<DrivePickerResultHandler>& result_handler() {
    return result_handler_;
  }
  base::WeakPtr<FakeDrivePickerHostController> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

 private:
  base::RepeatingCallback<void(FakeDrivePickerHostController*)> on_show_;
  DrivePickerHostRequest::RequestType request_type_ =
      DrivePickerHostRequest::RequestType::kPickerUi;
  DrivePickerHostRequest::Style request_style_ =
      DrivePickerHostRequest::Style::kDefault;
  mojo::Remote<DrivePickerResultHandler> result_handler_;
  base::WeakPtrFactory<FakeDrivePickerHostController> weak_factory_{this};
};

class GlicDrivePickerManagerTest : public testing::Test {
 public:
  GlicDrivePickerManagerTest() {
    feature_list_.InitWithFeatures(
        /*enabled_features=*/
        {
            features::kGlic,
            features::kGlicDrivePicker,
            omnibox::kDrivePickerV2Scope,
#if BUILDFLAG(IS_CHROMEOS)
            chromeos::features::kFeatureManagementGlic,
#endif  // BUILDFLAG(IS_CHROMEOS)
        },
        /*disabled_features=*/
        {
            features::kGlicCountryFiltering,
            features::kGlicLocaleFiltering,
            features::kSmartRestart,
            features::kSmartRestartMetrics,
        });
    minor_feature_list_.InitAndEnableFeatureWithParameters(
        features::kGlicMinorCountryFiltering, {{"enabled_countries", "*"}});
  }

  void SetUp() override {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(switches::kTestType);
    GlicEnabling::SetSystemRequirementMetForTesting(true);

    testing_profile_manager_ =
        TestingBrowserProcess::GetGlobal()->SetUpGlobalFeaturesForTesting(
            /*profile_manager=*/true);

#if BUILDFLAG(IS_CHROMEOS)
    glic_user_session_test_helper_.PreProfileSetUp(
        testing_profile_manager_->profile_manager());
#endif  // BUILDFLAG(IS_CHROMEOS)

    profile_ = testing_profile_manager_->CreateTestingProfile(
        TestingProfile::kDefaultProfileUserName,
        IdentityTestEnvironmentProfileAdaptor::
            GetIdentityTestEnvironmentFactoriesWithAppendedFactories(
                {TestingProfile::TestingFactory{
                    ChromeSigninClientFactory::GetInstance(),
                    base::BindRepeating(&BuildChromeSigninClientWithURLLoader,
                                        &test_url_loader_factory_)}}));

    identity_test_env_adaptor_ =
        std::make_unique<IdentityTestEnvironmentProfileAdaptor>(profile_);
    identity_test_env()->SetTestURLLoaderFactory(&test_url_loader_factory_);

    primary_account_ = identity_test_env()->MakePrimaryAccountAvailable(
        "user@gmail.com", signin::ConsentLevel::kSignin);
    AccountCapabilitiesTestMutator mutator(&primary_account_);
    SetGlicCapability(mutator, true);
    identity_test_env()->UpdateAccountInfoForAccount(primary_account_);
    identity_test_env()->SetCookieAccounts(
        {{std::string(primary_account_.GetEmail()),
          primary_account_.GetGaiaId()}});

    SetFRECompletion(profile_, prefs::FreStatus::kCompleted);
    profile_->GetPrefs()->SetInteger(
        prefs::kGlicFileUploadAllowed,
        std::to_underlying(prefs::GlicFileUploadPolicyState::kEnabled));

    enabling_ = GlicEnabling::CreateForTesting(
        profile_, &testing_profile_manager_->profile_manager()
                       ->GetProfileAttributesStorage());

    CreateManager(enabling_.get());
  }

  void TearDown() override {
    active_controller_ = nullptr;
    manager_.reset();
    enabling_.reset();
    identity_test_env_adaptor_.reset();
    profile_ = nullptr;
    testing_profile_manager_ = nullptr;

    TestingBrowserProcess::GetGlobal()->TearDownGlobalFeaturesForTesting();
#if BUILDFLAG(IS_CHROMEOS)
    glic_user_session_test_helper_.PostProfileTearDown();
#endif  // BUILDFLAG(IS_CHROMEOS)
    GlicEnabling::SetSystemRequirementMetForTesting(std::nullopt);
  }

 protected:
  signin::IdentityTestEnvironment* identity_test_env() {
    return identity_test_env_adaptor_->identity_test_env();
  }

  signin::IdentityManager* identity_manager() {
    return IdentityManagerFactory::GetForProfile(profile_);
  }

  const GaiaId& primary_gaia() const { return primary_account_.GetGaiaId(); }

  // (Re)creates `manager_`, observing `enabling` if it is not null.
  void CreateManager(GlicEnabling* enabling) {
    manager_ = std::make_unique<GlicDrivePickerManager>(
        profile_, identity_manager(), enabling);
    manager_->SetControllerFactoryForTesting(
        base::BindRepeating(&GlicDrivePickerManagerTest::CreateFakeController,
                            base::Unretained(this)));
  }

  std::unique_ptr<DrivePickerHostController> CreateFakeController(
      Profile* profile,
      BrowserWindowInterface* browser,
      views::Widget* anchor_widget) {
    return std::make_unique<FakeDrivePickerHostController>(
        profile, browser, anchor_widget,
        base::BindRepeating(&GlicDrivePickerManagerTest::OnControllerShown,
                            base::Unretained(this)));
  }

  void OnControllerShown(FakeDrivePickerHostController* controller) {
    active_controller_ = controller->GetWeakPtr();
  }

  void OpenPicker(const base::UnguessableToken& owner,
                  base::test::TestFuture<ShowResult>& future) {
    manager_->ShowDrivePicker(owner, primary_gaia(),
                              /*is_panel_showing=*/true,
                              /*webui_contents=*/nullptr, future.GetCallback());
    ASSERT_TRUE(manager_->IsPickerActiveForTesting());
    ASSERT_TRUE(active_controller_);
    EXPECT_EQ(active_controller_->request_type(),
              DrivePickerHostRequest::RequestType::kPickerUi);
    EXPECT_EQ(active_controller_->request_style(),
              DrivePickerHostRequest::Style::kGlic);
  }

  // Expects that exactly one outcome was recorded, and that it is `outcome`.
  void ExpectOutcome(GlicDrivePickerOutcome outcome) {
    histogram_tester_.ExpectUniqueSample(kOutcomeHistogram, outcome, 1);
  }

  content::BrowserTaskEnvironment task_environment_;
  base::HistogramTester histogram_tester_;
  base::test::ScopedFeatureList feature_list_;
  base::test::ScopedFeatureList minor_feature_list_;
#if BUILDFLAG(IS_CHROMEOS)
  ash::GlicUserSessionTestHelper glic_user_session_test_helper_;
#endif  // BUILDFLAG(IS_CHROMEOS)
  raw_ptr<TestingProfileManager> testing_profile_manager_ = nullptr;
  network::TestURLLoaderFactory test_url_loader_factory_;
  raw_ptr<TestingProfile> profile_ = nullptr;
  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor>
      identity_test_env_adaptor_;
  AccountInfo primary_account_;
  std::unique_ptr<GlicEnabling> enabling_;
  std::unique_ptr<GlicDrivePickerManager> manager_;
  base::WeakPtr<FakeDrivePickerHostController> active_controller_;
};

TEST_F(GlicDrivePickerManagerTest, UnsupportedWhenGlicDrivePickerDisabled) {
  base::test::ScopedFeatureList disabled;
  disabled.InitAndDisableFeature(features::kGlicDrivePicker);

  base::test::TestFuture<ShowResult> future;
  manager_->ShowDrivePicker(base::UnguessableToken::Create(), primary_gaia(),
                            /*is_panel_showing=*/true,
                            /*webui_contents=*/nullptr, future.GetCallback());
  EXPECT_EQ(future.Get(), base::unexpected(GlicDrivePickerError::kUnsupported));
  EXPECT_FALSE(manager_->IsPickerActiveForTesting());
  ExpectOutcome(GlicDrivePickerOutcome::kUnsupported);
}

TEST_F(GlicDrivePickerManagerTest, UnsupportedWhenV2ScopeDisabled) {
  base::test::ScopedFeatureList disabled;
  disabled.InitAndDisableFeature(omnibox::kDrivePickerV2Scope);

  base::test::TestFuture<ShowResult> future;
  manager_->ShowDrivePicker(base::UnguessableToken::Create(), primary_gaia(),
                            /*is_panel_showing=*/true,
                            /*webui_contents=*/nullptr, future.GetCallback());
  EXPECT_EQ(future.Get(), base::unexpected(GlicDrivePickerError::kUnsupported));
  EXPECT_FALSE(manager_->IsPickerActiveForTesting());
  ExpectOutcome(GlicDrivePickerOutcome::kUnsupported);
}

// Enables GEiC before `SetUp()` creates the single profile so ChromeOS's
// `GlicUserSessionTestHelper` (which allows only one `TestingProfile` per test)
// latches the profile to Gemini Enterprise cleanly.
class GlicDrivePickerManagerGeicTest : public GlicDrivePickerManagerTest {
 public:
  GlicDrivePickerManagerGeicTest() {
    geic_feature_list_.InitWithFeaturesAndParameters(
        {{features::kGeic,
          {{"enabled", "true"},
           {features::kGeicGuestURL.name,
            "https://business.gemini.google/side-panel?configId=abc"}}}},
        {});
  }

 private:
  base::test::ScopedFeatureList geic_feature_list_;
};

TEST_F(GlicDrivePickerManagerGeicTest, UnsupportedForGeminiEnterprise) {
  ASSERT_EQ(GlicProviderLatch::GetProviderForTesting(*profile_),
            GlicProvider::kGeminiEnterprise);
  EXPECT_EQ(GlicKeyedService::Get(profile_)->drive_picker_manager(), nullptr);

  base::test::TestFuture<ShowResult> future;
  manager_->ShowDrivePicker(base::UnguessableToken::Create(), primary_gaia(),
                            /*is_panel_showing=*/true,
                            /*webui_contents=*/nullptr, future.GetCallback());
  EXPECT_EQ(future.Get(), base::unexpected(GlicDrivePickerError::kUnsupported));
  EXPECT_FALSE(manager_->IsPickerActiveForTesting());
  ExpectOutcome(GlicDrivePickerOutcome::kUnsupported);
}

TEST_F(GlicDrivePickerManagerTest, DisabledByPolicyWhenFileUploadDisallowed) {
  profile_->GetPrefs()->SetInteger(
      prefs::kGlicFileUploadAllowed,
      std::to_underlying(prefs::GlicFileUploadPolicyState::kDisabled));

  base::test::TestFuture<ShowResult> future;
  manager_->ShowDrivePicker(base::UnguessableToken::Create(), primary_gaia(),
                            /*is_panel_showing=*/true,
                            /*webui_contents=*/nullptr, future.GetCallback());
  EXPECT_EQ(future.Get(),
            base::unexpected(GlicDrivePickerError::kDisabledByPolicy));
  ExpectOutcome(GlicDrivePickerOutcome::kDisabledByPolicy);
}

TEST_F(GlicDrivePickerManagerTest, DisabledByPolicyBeforeFreConsent) {
  SetFRECompletion(profile_, prefs::FreStatus::kNotStarted);

  base::test::TestFuture<ShowResult> future;
  manager_->ShowDrivePicker(base::UnguessableToken::Create(), primary_gaia(),
                            /*is_panel_showing=*/true,
                            /*webui_contents=*/nullptr, future.GetCallback());
  EXPECT_EQ(future.Get(),
            base::unexpected(GlicDrivePickerError::kDisabledByPolicy));
  ExpectOutcome(GlicDrivePickerOutcome::kDisabledByPolicy);
}

TEST_F(GlicDrivePickerManagerTest, NoAnchorWindowWhenPanelNotShowing) {
  base::test::TestFuture<ShowResult> future;
  manager_->ShowDrivePicker(base::UnguessableToken::Create(), primary_gaia(),
                            /*is_panel_showing=*/false,
                            /*webui_contents=*/nullptr, future.GetCallback());
  EXPECT_EQ(future.Get(),
            base::unexpected(GlicDrivePickerError::kNoAnchorWindow));
  ExpectOutcome(GlicDrivePickerOutcome::kNoAnchorWindow);
}

TEST_F(GlicDrivePickerManagerTest, AccountMismatchRejectedBeforeLaunch) {
  base::test::TestFuture<ShowResult> future;
  manager_->ShowDrivePicker(base::UnguessableToken::Create(),
                            GaiaId("other-gaia"),
                            /*is_panel_showing=*/true,
                            /*webui_contents=*/nullptr, future.GetCallback());
  EXPECT_EQ(future.Get(),
            base::unexpected(GlicDrivePickerError::kAccountMismatch));
  ExpectOutcome(GlicDrivePickerOutcome::kAccountMismatch);
}

TEST_F(GlicDrivePickerManagerTest, WebSessionSignedOutRejectedBeforeLaunch) {
  identity_test_env()->SetCookieAccounts({});

  base::test::TestFuture<ShowResult> future;
  manager_->ShowDrivePicker(base::UnguessableToken::Create(), primary_gaia(),
                            /*is_panel_showing=*/true,
                            /*webui_contents=*/nullptr, future.GetCallback());
  EXPECT_EQ(future.Get(),
            base::unexpected(GlicDrivePickerError::kWebSessionSignedOut));
  EXPECT_FALSE(manager_->IsPickerActiveForTesting());
  ExpectOutcome(GlicDrivePickerOutcome::kWebSessionSignedOut);
}

TEST_F(GlicDrivePickerManagerTest, SelectionSuccessReturnsSanitizedFiles) {
  base::test::TestFuture<ShowResult> future;
  OpenPicker(base::UnguessableToken::Create(), future);

  std::vector<DriveFilePtr> files;
  files.push_back(
      MakeValidDriveFile("file_1", "Spec.pdf", "application/pdf", 2048, "rk1"));
  files.push_back(MakeValidDriveFile("file_2", "Doc",
                                     "application/vnd.google-apps.document", 0,
                                     std::nullopt));
  active_controller_->result_handler()->OnSelection(std::move(files));

  const ShowResult& result = future.Get();
  ASSERT_TRUE(result.has_value());
  ASSERT_EQ(result->size(), 2u);
  EXPECT_EQ((*result)[0].id, "file_1");
  EXPECT_EQ((*result)[0].title, "Spec.pdf");
  EXPECT_EQ((*result)[0].mime_type, "application/pdf");
  EXPECT_EQ((*result)[0].size_bytes, 2048u);
  EXPECT_EQ((*result)[0].resource_key, "rk1");
  EXPECT_EQ((*result)[1].id, "file_2");
  EXPECT_EQ((*result)[1].size_bytes, 0u);
  EXPECT_EQ((*result)[1].resource_key, std::nullopt);
  EXPECT_FALSE(manager_->IsPickerActiveForTesting());
  ExpectOutcome(GlicDrivePickerOutcome::kFilesPicked);
}

TEST_F(GlicDrivePickerManagerTest, SelectionDropsInvalidFilesAndReturnsValid) {
  base::test::TestFuture<ShowResult> future;
  OpenPicker(base::UnguessableToken::Create(), future);

  std::vector<DriveFilePtr> files;
  files.push_back(MakeValidDriveFile("../bad-id"));
  files.push_back(MakeValidDriveFile("good_id", "Notes.txt", "text/plain", 42));
  active_controller_->result_handler()->OnSelection(std::move(files));

  const ShowResult& result = future.Get();
  ASSERT_TRUE(result.has_value());
  ASSERT_EQ(result->size(), 1u);
  EXPECT_EQ((*result)[0].id, "good_id");
  ExpectOutcome(GlicDrivePickerOutcome::kFilesPicked);
}

TEST_F(GlicDrivePickerManagerTest, SelectionAllInvalidFailsWithLoadFailed) {
  base::test::TestFuture<ShowResult> future;
  OpenPicker(base::UnguessableToken::Create(), future);

  std::vector<DriveFilePtr> files;
  files.push_back(MakeValidDriveFile("../bad-id"));
  active_controller_->result_handler()->OnSelection(std::move(files));

  EXPECT_EQ(future.Get(), base::unexpected(GlicDrivePickerError::kLoadFailed));
  EXPECT_FALSE(manager_->IsPickerActiveForTesting());
  ExpectOutcome(GlicDrivePickerOutcome::kSelectionRejectedByBrowser);
}

TEST_F(GlicDrivePickerManagerTest,
       SelectionExceedingMaxCapReportsBadMessageAndFails) {
  mojo::test::BadMessageObserver bad_message_observer;
  base::test::TestFuture<ShowResult> future;
  OpenPicker(base::UnguessableToken::Create(), future);

  std::vector<DriveFilePtr> files;
  for (size_t i = 0; i <= GlicDrivePickerManager::kMaxSelections; ++i) {
    files.push_back(MakeValidDriveFile("file_" + base::NumberToString(i)));
  }
  active_controller_->result_handler()->OnSelection(std::move(files));

  EXPECT_EQ(bad_message_observer.WaitForBadMessage(),
            "Too many Drive files selected.");
  EXPECT_EQ(future.Get(), base::unexpected(GlicDrivePickerError::kLoadFailed));
  EXPECT_FALSE(manager_->IsPickerActiveForTesting());
  ExpectOutcome(GlicDrivePickerOutcome::kSelectionRejectedByBrowser);
}

// `OnSelection()` checks policy and consent again before returning files, in
// case a change was not observed while the picker was open. A manager without
// a `GlicEnabling` does not observe consent changes, which exercises that
// check.
TEST_F(GlicDrivePickerManagerTest, SelectionRejectedWhenConsentRevoked) {
  CreateManager(/*enabling=*/nullptr);
  base::test::TestFuture<ShowResult> future;
  OpenPicker(base::UnguessableToken::Create(), future);

  SetFRECompletion(profile_, prefs::FreStatus::kNotStarted);
  ASSERT_TRUE(manager_->IsPickerActiveForTesting());

  std::vector<DriveFilePtr> files;
  files.push_back(MakeValidDriveFile("file_1"));
  active_controller_->result_handler()->OnSelection(std::move(files));

  EXPECT_EQ(future.Get(),
            base::unexpected(GlicDrivePickerError::kDisabledByPolicy));
  EXPECT_FALSE(manager_->IsPickerActiveForTesting());
  ExpectOutcome(GlicDrivePickerOutcome::kDisabledByPolicy);
}

TEST_F(GlicDrivePickerManagerTest, OnCancelResolvesEmptySelection) {
  base::test::TestFuture<ShowResult> future;
  OpenPicker(base::UnguessableToken::Create(), future);

  active_controller_->result_handler()->OnCancel();

  const ShowResult& result = future.Get();
  ASSERT_TRUE(result.has_value());
  EXPECT_TRUE(result->empty());
  EXPECT_FALSE(manager_->IsPickerActiveForTesting());
  ExpectOutcome(GlicDrivePickerOutcome::kCancelledByUser);
}

TEST_F(GlicDrivePickerManagerTest, OnPickerClosedResolvesEmptySelection) {
  base::test::TestFuture<ShowResult> future;
  OpenPicker(base::UnguessableToken::Create(), future);

  active_controller_->ResetControllerStateForTesting();

  const ShowResult& result = future.Get();
  ASSERT_TRUE(result.has_value());
  EXPECT_TRUE(result->empty());
  EXPECT_FALSE(manager_->IsPickerActiveForTesting());
  ExpectOutcome(GlicDrivePickerOutcome::kDialogClosed);
}

TEST_F(GlicDrivePickerManagerTest, OnPickerDisconnectedFailsWithLoadFailed) {
  base::test::TestFuture<ShowResult> future;
  OpenPicker(base::UnguessableToken::Create(), future);

  active_controller_->result_handler().reset();

  EXPECT_EQ(future.Get(), base::unexpected(GlicDrivePickerError::kLoadFailed));
  EXPECT_FALSE(manager_->IsPickerActiveForTesting());
  ExpectOutcome(GlicDrivePickerOutcome::kPickerDisconnected);
}

TEST_F(GlicDrivePickerManagerTest, OnErrorMapsDrivePickerErrors) {
  struct Case {
    DrivePickerError input_error;
    GlicDrivePickerError expected_error;
    GlicDrivePickerOutcome expected_outcome;
  };
  const Case kCases[] = {
      {DrivePickerError::kAlreadyActive, GlicDrivePickerError::kAlreadyActive,
       GlicDrivePickerOutcome::kAlreadyActive},
      {DrivePickerError::kWindowNotFound, GlicDrivePickerError::kNoAnchorWindow,
       GlicDrivePickerOutcome::kNoAnchorWindow},
      {DrivePickerError::kTokenFetchFailure, GlicDrivePickerError::kLoadFailed,
       GlicDrivePickerOutcome::kTokenFetchFailed},
      {DrivePickerError::kApiLoadFailure, GlicDrivePickerError::kLoadFailed,
       GlicDrivePickerOutcome::kApiLoadFailed},
      {DrivePickerError::kInvalidResponse, GlicDrivePickerError::kLoadFailed,
       GlicDrivePickerOutcome::kInvalidResponse},
      {DrivePickerError::kInvalidFileId, GlicDrivePickerError::kLoadFailed,
       GlicDrivePickerOutcome::kInvalidFileId},
      {DrivePickerError::kInvalidFileMetadata,
       GlicDrivePickerError::kLoadFailed,
       GlicDrivePickerOutcome::kInvalidFileMetadata},
      {DrivePickerError::kUnsupportedFileType,
       GlicDrivePickerError::kLoadFailed,
       GlicDrivePickerOutcome::kUnsupportedFileType},
      {DrivePickerError::kInvalidFileSize, GlicDrivePickerError::kLoadFailed,
       GlicDrivePickerOutcome::kInvalidFileSize},
      {DrivePickerError::kMojoDisconnected, GlicDrivePickerError::kLoadFailed,
       GlicDrivePickerOutcome::kPickerDisconnected},
      {DrivePickerError::kWebUINotFound, GlicDrivePickerError::kLoadFailed,
       GlicDrivePickerOutcome::kDialogCreationFailed},
      {DrivePickerError::kViewNotFound, GlicDrivePickerError::kLoadFailed,
       GlicDrivePickerOutcome::kDialogCreationFailed},
      {DrivePickerError::kUnknown, GlicDrivePickerError::kUnknown,
       GlicDrivePickerOutcome::kUnknownError},
  };

  for (const Case& test_case : kCases) {
    base::HistogramTester histogram_tester;
    base::test::TestFuture<ShowResult> future;
    OpenPicker(base::UnguessableToken::Create(), future);
    active_controller_->result_handler()->OnError(test_case.input_error);
    EXPECT_EQ(future.Get(), base::unexpected(test_case.expected_error));
    histogram_tester.ExpectUniqueSample(kOutcomeHistogram,
                                        test_case.expected_outcome, 1);
  }
}

TEST_F(GlicDrivePickerManagerTest, AlreadyActiveRejectedAcrossDifferentOwners) {
  const base::UnguessableToken owner_a = base::UnguessableToken::Create();
  const base::UnguessableToken owner_b = base::UnguessableToken::Create();

  base::test::TestFuture<ShowResult> future_a;
  OpenPicker(owner_a, future_a);

  base::test::TestFuture<ShowResult> future_b;
  manager_->ShowDrivePicker(owner_b, primary_gaia(),
                            /*is_panel_showing=*/true,
                            /*webui_contents=*/nullptr, future_b.GetCallback());
  EXPECT_EQ(future_b.Get(),
            base::unexpected(GlicDrivePickerError::kAlreadyActive));
  EXPECT_TRUE(manager_->IsPickerActiveForTesting());
  EXPECT_FALSE(future_a.IsReady());

  active_controller_->result_handler()->OnCancel();
  ASSERT_TRUE(future_a.Get().has_value());
  histogram_tester_.ExpectBucketCount(
      kOutcomeHistogram, GlicDrivePickerOutcome::kAlreadyActive, 1);
  histogram_tester_.ExpectBucketCount(
      kOutcomeHistogram, GlicDrivePickerOutcome::kCancelledByUser, 1);
  histogram_tester_.ExpectTotalCount(kOutcomeHistogram, 2);
}

TEST_F(GlicDrivePickerManagerTest, CancelForOwnerOnlyCancelsMatchingOwner) {
  const base::UnguessableToken owner_a = base::UnguessableToken::Create();
  const base::UnguessableToken owner_b = base::UnguessableToken::Create();

  base::test::TestFuture<ShowResult> future_a;
  OpenPicker(owner_a, future_a);

  // Closing an unrelated Glic instance must not cancel owner_a's picker.
  manager_->CancelForOwner(owner_b);
  EXPECT_TRUE(manager_->IsPickerActiveForTesting());
  EXPECT_FALSE(future_a.IsReady());

  // Closing owner_a's panel cancels the picker with an empty list.
  manager_->CancelForOwner(owner_a);
  const ShowResult& result = future_a.Get();
  ASSERT_TRUE(result.has_value());
  EXPECT_TRUE(result->empty());
  EXPECT_FALSE(manager_->IsPickerActiveForTesting());
  ExpectOutcome(GlicDrivePickerOutcome::kCancelledByOwner);
}

TEST_F(GlicDrivePickerManagerTest, LiveFileUploadPolicyDisableAbortsPicker) {
  base::test::TestFuture<ShowResult> future;
  OpenPicker(base::UnguessableToken::Create(), future);

  profile_->GetPrefs()->SetInteger(
      prefs::kGlicFileUploadAllowed,
      std::to_underlying(prefs::GlicFileUploadPolicyState::kDisabled));

  EXPECT_EQ(future.Get(),
            base::unexpected(GlicDrivePickerError::kDisabledByPolicy));
  EXPECT_FALSE(manager_->IsPickerActiveForTesting());
  ExpectOutcome(GlicDrivePickerOutcome::kDisabledByPolicy);
}

TEST_F(GlicDrivePickerManagerTest, LiveConsentRevocationAbortsPicker) {
  base::test::TestFuture<ShowResult> future;
  OpenPicker(base::UnguessableToken::Create(), future);

  SetFRECompletion(profile_, prefs::FreStatus::kNotStarted);

  EXPECT_EQ(future.Get(),
            base::unexpected(GlicDrivePickerError::kDisabledByPolicy));
  EXPECT_FALSE(manager_->IsPickerActiveForTesting());
  ExpectOutcome(GlicDrivePickerOutcome::kDisabledByPolicy);
}

TEST_F(GlicDrivePickerManagerTest, LiveWebSignOutAbortsPicker) {
  base::test::TestFuture<ShowResult> future;
  OpenPicker(base::UnguessableToken::Create(), future);

  identity_test_env()->SetCookieAccounts({});

  EXPECT_EQ(future.Get(),
            base::unexpected(GlicDrivePickerError::kWebSessionSignedOut));
  EXPECT_FALSE(manager_->IsPickerActiveForTesting());
  ExpectOutcome(GlicDrivePickerOutcome::kWebSessionSignedOut);
}

TEST_F(GlicDrivePickerManagerTest, LiveRefreshTokenRemovedAbortsPicker) {
  base::test::TestFuture<ShowResult> future;
  OpenPicker(base::UnguessableToken::Create(), future);

  identity_test_env()->RemoveRefreshTokenForPrimaryAccount();

  EXPECT_EQ(future.Get(),
            base::unexpected(GlicDrivePickerError::kChromeSignInRequired));
  EXPECT_FALSE(manager_->IsPickerActiveForTesting());
  ExpectOutcome(GlicDrivePickerOutcome::kChromeSignInRequired);
}

TEST_F(GlicDrivePickerManagerTest,
       LiveRefreshTokenPersistentErrorAbortsPicker) {
  base::test::TestFuture<ShowResult> future;
  OpenPicker(base::UnguessableToken::Create(), future);

  identity_test_env()->UpdatePersistentErrorOfRefreshTokenForAccount(
      primary_account_.GetAccountId(),
      GoogleServiceAuthError::FromInvalidGaiaCredentialsReason(
          GoogleServiceAuthError::InvalidGaiaCredentialsReason::
              CREDENTIALS_REJECTED_BY_SERVER));

  EXPECT_EQ(future.Get(),
            base::unexpected(GlicDrivePickerError::kChromeSignInRequired));
  EXPECT_FALSE(manager_->IsPickerActiveForTesting());
  ExpectOutcome(GlicDrivePickerOutcome::kChromeSignInRequired);
}

#if !BUILDFLAG(IS_CHROMEOS)
TEST_F(GlicDrivePickerManagerTest,
       ChromeSignInRequiredWhenPrimaryAccountClearedBeforeLaunch) {
  identity_test_env()->ClearPrimaryAccount();

  base::test::TestFuture<ShowResult> future;
  manager_->ShowDrivePicker(base::UnguessableToken::Create(), primary_gaia(),
                            /*is_panel_showing=*/true,
                            /*webui_contents=*/nullptr, future.GetCallback());
  EXPECT_EQ(future.Get(),
            base::unexpected(GlicDrivePickerError::kChromeSignInRequired));
  EXPECT_FALSE(manager_->IsPickerActiveForTesting());
  ExpectOutcome(GlicDrivePickerOutcome::kChromeSignInRequired);
}

TEST_F(GlicDrivePickerManagerTest,
       LivePrimaryAccountClearedAbortsWithSignInRequired) {
  base::test::TestFuture<ShowResult> future;
  OpenPicker(base::UnguessableToken::Create(), future);

  identity_test_env()->ClearPrimaryAccount();

  EXPECT_EQ(future.Get(),
            base::unexpected(GlicDrivePickerError::kChromeSignInRequired));
  EXPECT_FALSE(manager_->IsPickerActiveForTesting());
  ExpectOutcome(GlicDrivePickerOutcome::kChromeSignInRequired);
}

TEST_F(GlicDrivePickerManagerTest,
       LiveAccountSwitchToIneligibleAccountAbortsWithAccountMismatch) {
  base::test::TestFuture<ShowResult> future;
  OpenPicker(base::UnguessableToken::Create(), future);

  // Switching to an account without `can_use_gemini_in_chrome` makes
  // `GlicEnabling` report ineligible before `DrivePickerAccountWatcher` runs;
  // verify `OnPolicyOrEnablingChanged()` still reports `kAccountMismatch`.
  identity_test_env()->MakePrimaryAccountAvailable(
      "other@gmail.com", signin::ConsentLevel::kSignin);

  EXPECT_EQ(future.Get(),
            base::unexpected(GlicDrivePickerError::kAccountMismatch));
  EXPECT_FALSE(manager_->IsPickerActiveForTesting());
  ExpectOutcome(GlicDrivePickerOutcome::kAccountMismatch);
}
#endif  // !BUILDFLAG(IS_CHROMEOS)

TEST_F(GlicDrivePickerManagerTest,
       LiveCapabilityLossAbortsWithDisabledByPolicy) {
  base::test::TestFuture<ShowResult> future;
  OpenPicker(base::UnguessableToken::Create(), future);

  AccountCapabilitiesTestMutator mutator(&primary_account_);
  SetGlicCapability(mutator, false);
  identity_test_env()->UpdateAccountInfoForAccount(primary_account_);

  EXPECT_EQ(future.Get(),
            base::unexpected(GlicDrivePickerError::kDisabledByPolicy));
  EXPECT_FALSE(manager_->IsPickerActiveForTesting());
  ExpectOutcome(GlicDrivePickerOutcome::kDisabledByPolicy);
}

TEST_F(GlicDrivePickerManagerTest, DestructionCancelsActivePicker) {
  base::test::TestFuture<ShowResult> future;
  OpenPicker(base::UnguessableToken::Create(), future);

  manager_.reset();

  const ShowResult& result = future.Get();
  ASSERT_TRUE(result.has_value());
  EXPECT_TRUE(result->empty());
  ExpectOutcome(GlicDrivePickerOutcome::kCancelledByOwner);
}

class FakeBrowserCollectionGlobalFeatures : public GlobalFeatures {
 public:
  FakeBrowserCollectionGlobalFeatures() = default;
  ~FakeBrowserCollectionGlobalFeatures() override = default;

 protected:
  std::unique_ptr<GlobalBrowserCollection> CreateGlobalBrowserCollection()
      override {
    return std::make_unique<FakeGlobalBrowserCollection>();
  }
};

class FakeBrowserEntry {
 public:
  FakeBrowserEntry(
      FakeGlobalBrowserCollection* collection,
      Profile* profile,
      gfx::NativeWindow native_window,
      BrowserWindowInterface::Type type = BrowserWindowInterface::TYPE_NORMAL)
      : collection_(collection),
        tab_strip_model_(&tab_strip_delegate_, profile) {
    ON_CALL(base_window_, GetNativeWindow())
        .WillByDefault(testing::Return(native_window));
    ON_CALL(base_window_, IsVisible()).WillByDefault([this]() {
      return is_visible_;
    });
    ON_CALL(base_window_, IsMinimized()).WillByDefault([this]() {
      return is_minimized_;
    });

    ON_CALL(bwi_, GetProfile()).WillByDefault(testing::Return(profile));
    ON_CALL(testing::Const(bwi_), GetProfile())
        .WillByDefault(testing::Return(profile));
    ON_CALL(bwi_, GetType()).WillByDefault(testing::Return(type));
    ON_CALL(bwi_, IsDeleteScheduled()).WillByDefault([this]() {
      return delete_scheduled_;
    });
    ON_CALL(bwi_, GetWindow()).WillByDefault(testing::Return(&base_window_));
    ON_CALL(testing::Const(bwi_), GetWindow())
        .WillByDefault(testing::Return(&base_window_));
    ON_CALL(bwi_, GetTabStripModel())
        .WillByDefault(testing::Return(&tab_strip_model_));
    ON_CALL(testing::Const(bwi_), GetTabStripModel())
        .WillByDefault(testing::Return(&tab_strip_model_));

    collection_->SimulateBrowserCreated(&bwi_);
  }

  ~FakeBrowserEntry() { collection_->SimulateBrowserClosed(&bwi_); }

  void Activate() { collection_->SimulateBrowserActivated(&bwi_); }
  void set_is_visible(bool visible) { is_visible_ = visible; }
  void set_is_minimized(bool minimized) { is_minimized_ = minimized; }
  void set_delete_scheduled(bool scheduled) { delete_scheduled_ = scheduled; }
  BrowserWindowInterface* browser() { return &bwi_; }

 private:
  raw_ptr<FakeGlobalBrowserCollection> collection_;
  testing::NiceMock<ui::MockBaseWindow> base_window_;
  testing::NiceMock<MockBrowserWindowInterface> bwi_;
  TestTabStripModelDelegate tab_strip_delegate_;
  TabStripModel tab_strip_model_;
  bool is_visible_ = true;
  bool is_minimized_ = false;
  bool delete_scheduled_ = false;
};

class GlicDrivePickerAnchorBrowserTest : public ChromeViewsTestBase {
 public:
  void SetUp() override {
    GlobalFeatures::ReplaceGlobalFeaturesForTesting(base::BindRepeating(
        &GlicDrivePickerAnchorBrowserTest::CreateGlobalFeatures,
        base::Unretained(this)));
    testing_profile_manager_ =
        TestingBrowserProcess::GetGlobal()->SetUpGlobalFeaturesForTesting(
            /*profile_manager=*/true);
    ChromeViewsTestBase::SetUp();

    fake_collection_ = static_cast<FakeGlobalBrowserCollection*>(
        TestingBrowserProcess::GetGlobal()
            ->GetFeatures()
            ->global_browser_collection());
    ASSERT_TRUE(fake_collection_);

    profile_ = testing_profile_manager_->CreateTestingProfile("profile_a");
    other_profile_ =
        testing_profile_manager_->CreateTestingProfile("profile_b");
  }

  void TearDown() override {
    other_profile_ = nullptr;
    profile_ = nullptr;
    fake_collection_ = nullptr;
    testing_profile_manager_ = nullptr;
    TestingBrowserProcess::GetGlobal()->TearDownGlobalFeaturesForTesting();
    ChromeViewsTestBase::TearDown();
    GlobalFeatures::ReplaceGlobalFeaturesForTesting(base::NullCallback());
  }

 protected:
  std::unique_ptr<GlobalFeatures> CreateGlobalFeatures() {
    return std::make_unique<FakeBrowserCollectionGlobalFeatures>();
  }

  std::unique_ptr<views::Widget> CreateAnchorWidget() {
    auto widget =
        CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
    widget->Show();
    return widget;
  }

  BrowserWindowInterface* FindAnchorBrowser(views::Widget* anchor_widget) {
    return GlicDrivePickerManager::FindAnchorBrowserForTesting(
        profile_, anchor_widget, [](BrowserWindowInterface* b) {
          return b && b->GetWindow() && b->GetWindow()->IsVisible() &&
                 !b->GetWindow()->IsMinimized();
        });
  }

  GlicEnabling::ScopedBypassEnablementChecksForTesting bypass_enabling_;
  raw_ptr<TestingProfileManager> testing_profile_manager_ = nullptr;
  raw_ptr<FakeGlobalBrowserCollection> fake_collection_ = nullptr;
  raw_ptr<TestingProfile> profile_ = nullptr;
  raw_ptr<TestingProfile> other_profile_ = nullptr;
};

TEST_F(GlicDrivePickerAnchorBrowserTest,
       PrefersSidePanelBrowserMatchingAnchorWidget) {
  auto widget_a = CreateAnchorWidget();
  auto widget_b = CreateAnchorWidget();

  FakeBrowserEntry browser_a(fake_collection_, profile_,
                             widget_a->GetNativeWindow());
  FakeBrowserEntry browser_b(fake_collection_, profile_,
                             widget_b->GetNativeWindow());
  browser_a.Activate();
  browser_b.Activate();

  // Even though `browser_b` is more recently active, matching `widget_a` wins.
  EXPECT_EQ(FindAnchorBrowser(widget_a.get()), browser_a.browser());
}

TEST_F(GlicDrivePickerAnchorBrowserTest,
       FallsBackToMostRecentlyActiveAttachableBrowser) {
  auto widget_a = CreateAnchorWidget();
  auto widget_b = CreateAnchorWidget();
  auto floaty_widget = CreateAnchorWidget();

  FakeBrowserEntry browser_a(fake_collection_, profile_,
                             widget_a->GetNativeWindow());
  FakeBrowserEntry browser_b(fake_collection_, profile_,
                             widget_b->GetNativeWindow());
  browser_a.Activate();
  browser_b.Activate();

  EXPECT_EQ(FindAnchorBrowser(nullptr), browser_b.browser());
  EXPECT_EQ(FindAnchorBrowser(floaty_widget.get()), browser_b.browser());
}

TEST_F(GlicDrivePickerAnchorBrowserTest,
       SkipsOtherProfileHiddenAndDeleteScheduledBrowsers) {
  auto widget_eligible = CreateAnchorWidget();
  auto widget_other_profile = CreateAnchorWidget();
  auto widget_deleting = CreateAnchorWidget();
  auto widget_hidden = CreateAnchorWidget();
  auto widget_minimized = CreateAnchorWidget();
  auto widget_popup = CreateAnchorWidget();

  FakeBrowserEntry eligible(fake_collection_, profile_,
                            widget_eligible->GetNativeWindow());
  FakeBrowserEntry other_profile_browser(
      fake_collection_, other_profile_,
      widget_other_profile->GetNativeWindow());
  FakeBrowserEntry deleting(fake_collection_, profile_,
                            widget_deleting->GetNativeWindow());
  deleting.set_delete_scheduled(true);
  FakeBrowserEntry hidden(fake_collection_, profile_,
                          widget_hidden->GetNativeWindow());
  hidden.set_is_visible(false);
  FakeBrowserEntry minimized(fake_collection_, profile_,
                             widget_minimized->GetNativeWindow());
  minimized.set_is_minimized(true);
  FakeBrowserEntry popup(fake_collection_, profile_,
                         widget_popup->GetNativeWindow(),
                         BrowserWindowInterface::TYPE_POPUP);

  eligible.Activate();
  other_profile_browser.Activate();
  deleting.Activate();
  hidden.Activate();
  minimized.Activate();
  popup.Activate();

  EXPECT_EQ(FindAnchorBrowser(nullptr), eligible.browser());

  // When the only eligible browser becomes hidden, return nullptr so the
  // controller falls back to the detached anchor widget.
  eligible.set_is_visible(false);
  EXPECT_EQ(FindAnchorBrowser(nullptr), nullptr);
}

}  // namespace
}  // namespace glic
