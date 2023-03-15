// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#include "base/strings/string_piece_forward.h"
#include "chrome/browser/ash/policy/dlp/dlp_files_controller.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager_factory.h"
#include "chrome/browser/chromeos/policy/dlp/mock_dlp_rules_manager.h"
#include "chrome/browser/extensions/api/file_system/file_entry_picker.h"
#include "chrome/browser/file_select_helper.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/file_select_listener.h"
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
    if (cb_)
      cb_.Run();
  }

  void FileSelectionCanceled() override {}

  raw_ptr<std::vector<blink::mojom::FileChooserFileInfoPtr>> files_;
  base::RepeatingClosure cb_;
};

}  // namespace

class DlpFilesControllerBrowserTest : public InProcessBrowserTest {
 public:
  DlpFilesControllerBrowserTest() = default;

  ~DlpFilesControllerBrowserTest() override = default;

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    ASSERT_TRUE(temp_dir_.IsValid());
    InProcessBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    DlpRulesManagerFactory::GetInstance()->SetTestingFactory(
        browser()->profile(),
        base::BindRepeating(&DlpFilesControllerBrowserTest::SetDlpRulesManager,
                            base::Unretained(this)));
    ASSERT_TRUE(DlpRulesManagerFactory::GetForPrimaryProfile());
  }

  std::unique_ptr<KeyedService> SetDlpRulesManager(
      content::BrowserContext* context) {
    auto dlp_rules_manager =
        std::make_unique<testing::StrictMock<MockDlpRulesManager>>();
    mock_rules_manager_ = dlp_rules_manager.get();
    ON_CALL(*mock_rules_manager_, IsFilesPolicyEnabled)
        .WillByDefault(testing::Return(true));
    ON_CALL(*mock_rules_manager_, GetReportingManager)
        .WillByDefault(testing::Return(nullptr));

    files_controller_ =
        std::make_unique<DlpFilesController>(*mock_rules_manager_);
    ON_CALL(*mock_rules_manager_, GetDlpFilesController)
        .WillByDefault(testing::Return(files_controller_.get()));

    return dlp_rules_manager;
  }

 protected:
  // MockDlpRulesManager is owned by KeyedService and is guaranteed to outlive
  // this class.
  MockDlpRulesManager* mock_rules_manager_ = nullptr;

  std::unique_ptr<DlpFilesController> files_controller_ = nullptr;

  base::ScopedTempDir temp_dir_;
  std::vector<base::FilePath> file_paths_;
};

IN_PROC_BROWSER_TEST_F(DlpFilesControllerBrowserTest, FilesUploadCallerPassed) {
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

IN_PROC_BROWSER_TEST_F(DlpFilesControllerBrowserTest,
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

// (b/273269211): This is a test for the crash that happens upon showing a
// warning dialog when a file is moved to Google Drive.
IN_PROC_BROWSER_TEST_F(DlpFilesControllerBrowserTest, WarningDialog) {
  EXPECT_CALL(*mock_rules_manager_, GetReportingManager);
  EXPECT_CALL(*mock_rules_manager_,
              IsRestrictedComponent(
                  GURL(kExampleUrl), DlpRulesManager::Component::kDrive,
                  DlpRulesManager::Restriction::kFiles, testing::_, testing::_))
      .WillOnce(testing::Return(DlpRulesManager::Level::kWarn));

  std::vector<DlpFilesController::FileDaemonInfo> transferred_files;
  transferred_files.emplace_back(1234, base::FilePath("file1.txt"),
                                 kExampleUrl);
  EXPECT_EQ(files_controller_->GetWarnDialogForTesting(), nullptr);
  files_controller_->IsFilesTransferRestricted(
      transferred_files,
      DlpFilesController::DlpFileDestination(
          DlpRulesManager::Component::kDrive),
      DlpFilesController::FileAction::kMove, base::DoNothing());
  EXPECT_NE(files_controller_->GetWarnDialogForTesting(), nullptr);
}

}  // namespace policy
