// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/strings/string_piece_forward.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/ash/policy/dlp/dlp_files_controller.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager_factory.h"
#include "chrome/browser/chromeos/policy/dlp/mock_dlp_rules_manager.h"
#include "chrome/browser/file_select_helper.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chromeos/dbus/dlp/dlp_client.h"
#include "content/public/browser/file_select_listener.h"
#include "content/public/test/browser_test.h"
#include "third_party/blink/public/mojom/choosers/file_chooser.mojom.h"
#include "ui/shell_dialogs/fake_select_file_dialog.h"

using testing::_;

namespace policy {

namespace {

constexpr char kExampleUrl[] = "https://example.com";
constexpr char kExampleUrl1[] = "https://example1.com";
constexpr char kExampleUrl2[] = "https://example2.com";
constexpr char kExampleUrl3[] = "https://example3.com";

constexpr char kFileName1[] = "test1.txt";
constexpr char kFileName2[] = "test2.txt";
constexpr char kFileName3[] = "test3.txt";

void CreateDummyFile(const base::FilePath& path) {
  base::ScopedAllowBlockingForTesting allow_io;
  EXPECT_TRUE(WriteFile(path, "42", sizeof("42")) == sizeof("42"));
}

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

  void SetUpRulesManager(Profile* profile) {
    policy::DlpRulesManagerFactory::GetInstance()->SetTestingFactory(
        profile,
        base::BindRepeating(&DlpFilesControllerBrowserTest::SetDlpRulesManager,
                            base::Unretained(this)));
  }

  std::unique_ptr<KeyedService> SetDlpRulesManager(
      content::BrowserContext* context) {
    auto dlp_rules_manager = std::make_unique<MockDlpRulesManager>();
    MockDlpRulesManager* rules_manager = dlp_rules_manager.get();

    files_controller_ = std::make_unique<DlpFilesController>(*rules_manager);

    ON_CALL(*rules_manager, GetDlpFilesController)
        .WillByDefault(::testing::Return(files_controller_.get()));

    return dlp_rules_manager;
  }

  void AddFileToDlpClient(base::StringPiece filename,
                          const std::string& source_url) {
    ASSERT_TRUE(chromeos::DlpClient::Get()->IsAlive());
    base::MockCallback<chromeos::DlpClient::AddFileCallback> add_file_cb;

    EXPECT_CALL(add_file_cb, Run(testing::_)).Times(1);

    const base::FilePath dir_path = temp_dir_.GetPath();

    const base::FilePath file_path = dir_path.AppendASCII(filename);
    CreateDummyFile(file_path);
    dlp::AddFileRequest add_file_req;
    add_file_req.set_file_path(file_path.value());
    add_file_req.set_source_url(source_url);
    chromeos::DlpClient::Get()->AddFile(add_file_req, add_file_cb.Get());

    testing::Mock::VerifyAndClearExpectations(&add_file_cb);

    file_paths_.push_back(file_path);
  }

 protected:
  std::unique_ptr<DlpFilesController> files_controller_;
  base::ScopedTempDir temp_dir_;
  std::vector<base::FilePath> file_paths_;
};

IN_PROC_BROWSER_TEST_F(DlpFilesControllerBrowserTest,
                       FilesUploadRestrictedFile) {
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

  SetUpRulesManager(profile);

  AddFileToDlpClient(kFileName1, kExampleUrl1);
  AddFileToDlpClient(kFileName2, kExampleUrl2);
  AddFileToDlpClient(kFileName3, kExampleUrl3);

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
  dlp::CheckFilesTransferResponse response;
  response.add_files_paths(file_paths_[1].value());
  chromeos::DlpClient::Get()->GetTestInterface()->SetCheckFilesTransferResponse(
      response);
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

  select_file_dialog_factory->GetLastDialog()->CallMultiFilesSelected(
      file_paths_);
  run_loop_listener.Run();

  std::vector<base::FilePath> expected_allowed_files = {file_paths_[0],
                                                        file_paths_[2]};
  EXPECT_EQ(files.size(), expected_allowed_files.size());
  for (size_t i = 0; i < files.size(); ++i) {
    EXPECT_EQ(files[i]->get_native_file()->file_path,
              expected_allowed_files[i]);
  }
}

}  // namespace policy
