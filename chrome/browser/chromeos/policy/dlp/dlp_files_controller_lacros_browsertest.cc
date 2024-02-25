// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/dlp/dlp_files_controller_lacros.h"

#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager_factory.h"
#include "chrome/browser/chromeos/policy/dlp/test/mock_dlp_crosapi.h"
#include "chrome/browser/chromeos/policy/dlp/test/mock_dlp_rules_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chromeos/crosapi/mojom/dlp.mojom.h"
#include "chromeos/lacros/lacros_service.h"
#include "chromeos/startup/browser_init_params.h"
#include "content/public/test/browser_test.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

namespace {
constexpr char kFilePath[] = "test.txt";
}  // namespace

class DlpFilesControllerLacrosBlockUITest
    : public InProcessBrowserTest,
      public ::testing::WithParamInterface<
          std::pair<crosapi::mojom::FileAction, policy::dlp::FileAction>> {
 public:
  // InProcessBrowserTest:
  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    // If DLP interface is not available on this version of ash-chrome, this
    // test suite will no-op.
    if (!IsServiceAvailable()) {
      return;
    }

    // Replace the production DLP service with a mock for testing.
    mojo::Remote<crosapi::mojom::Dlp>& remote =
        chromeos::LacrosService::Get()->GetRemote<crosapi::mojom::Dlp>();
    remote.reset();
    receiver_.Bind(remote.BindNewPipeAndPassReceiver());

    ASSERT_TRUE(browser());
    policy::DlpRulesManagerFactory::GetInstance()->SetTestingFactory(
        browser()->profile(),
        base::BindRepeating(
            &DlpFilesControllerLacrosBlockUITest::SetDlpRulesManager,
            base::Unretained(this)));
    ASSERT_TRUE(policy::DlpRulesManagerFactory::GetForPrimaryProfile());
    ASSERT_TRUE(rules_manager_);

    files_controller_ =
        std::make_unique<DlpFilesControllerLacros>(*rules_manager_);
  }

  void TearDownOnMainThread() override {
    InProcessBrowserTest::TearDownOnMainThread();
  }

  std::unique_ptr<KeyedService> SetDlpRulesManager(
      content::BrowserContext* context) {
    auto dlp_rules_manager = std::make_unique<MockDlpRulesManager>(
        Profile::FromBrowserContext(context));
    rules_manager_ = dlp_rules_manager.get();
    return dlp_rules_manager;
  }

  // Returns whether the DLP interface is available. It may
  // not be available on earlier versions of ash-chrome.
  bool IsServiceAvailable() const {
    chromeos::LacrosService* lacros_service = chromeos::LacrosService::Get();
    return lacros_service && lacros_service->IsAvailable<crosapi::mojom::Dlp>();
  }

  void SetDlpInterfaceVersion(int version) {
    crosapi::mojom::BrowserInitParamsPtr init_params =
        chromeos::BrowserInitParams::GetForTests()->Clone();
    init_params->interface_versions.value()[crosapi::mojom::Dlp::Uuid_] =
        version;
    chromeos::BrowserInitParams::SetInitParamsForTests(std::move(init_params));
  }

  testing::StrictMock<MockDlpCrosapi>& service() { return service_; }

 protected:
  testing::StrictMock<MockDlpCrosapi> service_;
  mojo::Receiver<crosapi::mojom::Dlp> receiver_{&service_};

  raw_ptr<MockDlpRulesManager, DisableDanglingPtrDetection> rules_manager_ =
      nullptr;

  std::unique_ptr<DlpFilesControllerLacros> files_controller_;
};

INSTANTIATE_TEST_SUITE_P(
    DlpFilesControllerLacrosBlockUI,
    DlpFilesControllerLacrosBlockUITest,
    ::testing::Values(std::make_tuple(crosapi::mojom::FileAction::kUnknown,
                                      policy::dlp::FileAction::kUnknown),
                      std::make_tuple(crosapi::mojom::FileAction::kDownload,
                                      policy::dlp::FileAction::kDownload),
                      std::make_tuple(crosapi::mojom::FileAction::kTransfer,
                                      policy::dlp::FileAction::kTransfer),
                      std::make_tuple(crosapi::mojom::FileAction::kUpload,
                                      policy::dlp::FileAction::kUpload),
                      std::make_tuple(crosapi::mojom::FileAction::kCopy,
                                      policy::dlp::FileAction::kCopy),
                      std::make_tuple(crosapi::mojom::FileAction::kMove,
                                      policy::dlp::FileAction::kMove),
                      std::make_tuple(crosapi::mojom::FileAction::kOpen,
                                      policy::dlp::FileAction::kOpen),
                      std::make_tuple(crosapi::mojom::FileAction::kShare,
                                      policy::dlp::FileAction::kShare)));

IN_PROC_BROWSER_TEST_P(DlpFilesControllerLacrosBlockUITest,
                       ShowDlpBlockedFilesTest) {
  auto [mojo_action, dlp_action] = GetParam();

  // If DLP interface is not available on this version of ash-chrome, this test
  // suite will no-op.
  if (!IsServiceAvailable()) {
    return;
  }

  SetDlpInterfaceVersion(service().Version_);

  std::optional<uint64_t> task_id = std::nullopt;
  base::FilePath path(kFilePath);

  EXPECT_CALL(service(),
              ShowBlockedFiles(task_id, std::vector<base::FilePath>{path},
                               mojo_action));

  files_controller_->ShowDlpBlockedFiles(task_id, {path}, dlp_action);
}

}  // namespace policy
