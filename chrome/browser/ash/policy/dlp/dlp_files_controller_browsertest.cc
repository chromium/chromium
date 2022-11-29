// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/callback_forward.h"
#include "base/files/file_path.h"
#include "base/strings/string_piece_forward.h"
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

 protected:
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

}  // namespace policy
