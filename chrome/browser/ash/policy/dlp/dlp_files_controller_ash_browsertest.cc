// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/dlp/dlp_files_controller_ash.h"

#include <vector>

#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/ash/file_manager/file_manager_test_util.h"
#include "chrome/browser/ash/file_system_provider/fake_extension_provider.h"
#include "chrome/browser/ash/file_system_provider/service.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager_factory.h"
#include "chrome/browser/chromeos/policy/dlp/test/mock_dlp_rules_manager.h"
#include "chrome/browser/enterprise/data_controls/dlp_reporting_manager.h"
#include "chrome/browser/enterprise/data_controls/dlp_reporting_manager_test_helper.h"
#include "chrome/browser/extensions/api/file_system/file_entry_picker.h"
#include "chrome/browser/file_select_helper.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/enterprise/data_controls/core/browser/dlp_policy_event.pb.h"
#include "content/public/browser/file_select_listener.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/test/browser_test.h"
#include "third_party/blink/public/mojom/choosers/file_chooser.mojom.h"
#include "ui/shell_dialogs/fake_select_file_dialog.h"
#include "url/gurl.h"

using testing::_;

namespace policy {

namespace {

constexpr char kExampleUrl[] = "https://example.com";

// A listener that compares the list of files chosen with files expected.
class TestFileSelectListener : public content::FileSelectListener {
 public:
  explicit TestFileSelectListener(
      std::vector<blink::mojom::FileChooserFileInfoPtr>* files,
      base::RepeatingClosure cb)
      : files_(files), cb_(cb) {}

 private:
  ~TestFileSelectListener() override = default;

  void FileSelected(std::vector<blink::mojom::FileChooserFileInfoPtr> files,
                    const base::FilePath& base_dir,
                    blink::mojom::FileChooserParams::Mode mode) override {
    *files_ = std::move(files);
    if (cb_) {
      cb_.Run();
    }
  }

  void FileSelectionCanceled() override {}

  raw_ptr<std::vector<blink::mojom::FileChooserFileInfoPtr>> files_;
  base::RepeatingClosure cb_;
};

}  // namespace

class DlpFilesControllerAshBrowserTest : public InProcessBrowserTest {
 public:
  DlpFilesControllerAshBrowserTest() = default;

  ~DlpFilesControllerAshBrowserTest() override = default;

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    ASSERT_TRUE(temp_dir_.IsValid());
    InProcessBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    DlpRulesManagerFactory::GetInstance()->SetTestingFactory(
        browser()->profile(),
        base::BindRepeating(
            &DlpFilesControllerAshBrowserTest::SetDlpRulesManager,
            base::Unretained(this)));
    ASSERT_TRUE(DlpRulesManagerFactory::GetForPrimaryProfile());
  }

  void TearDownOnMainThread() override {
    // Make sure the rules manager does not return a freed files controller.
    ON_CALL(*mock_rules_manager_, GetDlpFilesController)
        .WillByDefault(testing::Return(nullptr));

    // The files controller must be destroyed before the profile since it's
    // holding a pointer to it.
    files_controller_.reset();

    InProcessBrowserTest::TearDownOnMainThread();
  }

  std::unique_ptr<KeyedService> SetDlpRulesManager(
      content::BrowserContext* context) {
    auto dlp_rules_manager =
        std::make_unique<testing::StrictMock<MockDlpRulesManager>>(
            Profile::FromBrowserContext(context));
    mock_rules_manager_ = dlp_rules_manager.get();
    ON_CALL(*mock_rules_manager_, IsFilesPolicyEnabled)
        .WillByDefault(testing::Return(true));
    ON_CALL(*mock_rules_manager_, GetReportingManager)
        .WillByDefault(testing::Return(nullptr));

    files_controller_ = std::make_unique<DlpFilesControllerAsh>(
        *mock_rules_manager_, Profile::FromBrowserContext(context));
    ON_CALL(*mock_rules_manager_, GetDlpFilesController)
        .WillByDefault(testing::Return(files_controller_.get()));

    return dlp_rules_manager;
  }

 protected:
  // MockDlpRulesManager is owned by KeyedService and is guaranteed to outlive
  // this class.
  raw_ptr<MockDlpRulesManager, DanglingUntriaged> mock_rules_manager_ = nullptr;

  std::unique_ptr<DlpFilesControllerAsh> files_controller_ = nullptr;

  base::ScopedTempDir temp_dir_;
  std::vector<base::FilePath> file_paths_;
};

IN_PROC_BROWSER_TEST_F(DlpFilesControllerAshBrowserTest,
                       CheckIfDownloadAllowed_OneDrive) {
  // Mount OneDrive file system.
  auto fake_provider = ash::file_system_provider::FakeExtensionProvider::Create(
      extension_misc::kODFSExtensionId);
  const auto providerId = fake_provider->GetId();
  auto* service = ash::file_system_provider::Service::Get(browser()->profile());
  service->RegisterProvider(std::move(fake_provider));

  const auto mount_options = ash::file_system_provider::MountOptions(
      "test-filesystem", "OneDrive Test FileSystem");
  service->MountFileSystem(providerId, mount_options);

  const auto file_system_list =
      service->GetProvidedFileSystemInfoList(providerId);
  EXPECT_EQ(file_system_list.size(), 1UL);

  // Setup the reporting manager.
  std::vector<DlpPolicyEvent> events;
  auto reporting_manager =
      std::make_unique<data_controls::DlpReportingManager>();
  SetReportQueueForReportingManager(
      reporting_manager.get(), events,
      base::SequencedTaskRunner::GetCurrentDefault());
  ON_CALL(*mock_rules_manager_, GetReportingManager)
      .WillByDefault(::testing::Return(reporting_manager.get()));

  const std::string rule_name = "Rule name";
  const std::string rule_id = "Rule ID";

  const DlpRulesManager::RuleMetadata ruleMetadata(rule_name, rule_id);

  EXPECT_CALL(
      *mock_rules_manager_,
      IsRestrictedComponent(_, data_controls::Component::kOneDrive, _, _, _))
      .WillOnce(
          testing::DoAll(testing::SetArgPointee<3>(kExampleUrl),
                         testing::SetArgPointee<4>(ruleMetadata),
                         testing::Return(DlpRulesManager::Level::kBlock)));

  EXPECT_CALL(*mock_rules_manager_, GetReportingManager())
      .Times(::testing::AnyNumber());

  testing::StrictMock<
      base::MockCallback<DlpFilesControllerAsh::CheckIfDlpAllowedCallback>>
      cb;
  EXPECT_CALL(cb, Run(/*is_allowed=*/false)).Times(1);

  const std::string file_path = "Downloads/file.txt";

  files_controller_->CheckIfDownloadAllowed(
      DlpFileDestination(GURL(kExampleUrl)),
      base::FilePath(file_system_list[0].mount_path().Append(file_path)),
      cb.Get());

  ASSERT_EQ(events.size(), 1u);

  auto event_builder = data_controls::DlpPolicyEventBuilder::Event(
      GURL(kExampleUrl).spec(), rule_name, rule_id,
      DlpRulesManager::Restriction::kFiles, DlpRulesManager::Level::kBlock);

  event_builder->SetDestinationComponent(data_controls::Component::kOneDrive);
  event_builder->SetContentName(base::FilePath(file_path).BaseName().value());

  EXPECT_THAT(events[0],
              data_controls::IsDlpPolicyEvent(event_builder->Create()));
}

IN_PROC_BROWSER_TEST_F(DlpFilesControllerAshBrowserTest,
                       FilesUploadCallerPassed) {
  ui::FakeSelectFileDialog::Factory* select_file_dialog_factory =
      ui::FakeSelectFileDialog::RegisterFactory();

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL(kExampleUrl)));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::RenderFrameHost* render_frame_host =
      web_contents->GetPrimaryMainFrame();
  Profile* profile = Profile::FromBrowserContext(
      render_frame_host->GetProcess()->GetBrowserContext());
  scoped_refptr<FileSelectHelper> file_select_helper(
      new FileSelectHelper(profile));

  blink::mojom::FileChooserParams params(
      /*mode=*/blink::mojom::FileChooserParams_Mode::kSave,
      /*title=*/std::u16string(),
      /*default_file_name=*/base::FilePath(),
      /*selected_files=*/{},
      /*accept_types=*/{u".txt"},
      /*need_local_path=*/true,
      /*use_media_capture=*/false,
      /*requestor=*/GURL());
  std::vector<blink::mojom::FileChooserFileInfoPtr> files;
  base::RunLoop run_loop_listener;
  auto listener = base::MakeRefCounted<TestFileSelectListener>(
      &files, run_loop_listener.QuitClosure());
  {
    base::RunLoop run_loop;
    select_file_dialog_factory->SetOpenCallback(run_loop.QuitClosure());
    file_select_helper->RunFileChooser(render_frame_host, std::move(listener),
                                       params.Clone());
    run_loop.Run();
  }

  const GURL* caller = select_file_dialog_factory->GetLastDialog()->caller();
  ASSERT_TRUE(caller);
  EXPECT_EQ(*caller, GURL(kExampleUrl));
}

IN_PROC_BROWSER_TEST_F(DlpFilesControllerAshBrowserTest,
                       FileEntryPicker_CallerPassed) {
  ui::FakeSelectFileDialog::Factory* select_file_dialog_factory =
      ui::FakeSelectFileDialog::RegisterFactory();

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL(kExampleUrl)));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  {
    base::RunLoop run_loop;
    select_file_dialog_factory->SetOpenCallback(run_loop.QuitClosure());
    new extensions::FileEntryPicker(
        /*web_contents=*/web_contents, /*suggested_name=*/base::FilePath(),
        /*file_type_info*/ ui::SelectFileDialog::FileTypeInfo(),
        /*picker_type=*/ui::SelectFileDialog::Type::SELECT_SAVEAS_FILE,
        /*files_selected_callback=*/base::DoNothing(),
        /*file_selection_canceled_callback=*/base::DoNothing());
    run_loop.Run();
  }

  const GURL* caller = select_file_dialog_factory->GetLastDialog()->caller();
  ASSERT_TRUE(caller);
  EXPECT_EQ(*caller, GURL(kExampleUrl));
}

}  // namespace policy
