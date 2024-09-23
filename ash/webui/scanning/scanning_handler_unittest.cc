// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/scanning/scanning_handler.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "ash/webui/scanning/scanning_app_delegate.h"
#include "base/check.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/raw_ptr.h"
#include "base/values.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_web_ui.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/shell_dialogs/select_file_dialog.h"
#include "ui/shell_dialogs/select_file_dialog_factory.h"
#include "ui/shell_dialogs/select_file_policy.h"
#include "ui/shell_dialogs/selected_file_info.h"
#include "url/gurl.h"

namespace ash {

namespace {

constexpr char kHandlerFunctionName[] = "handlerFunctionName";
constexpr char kTestFilePath[] = "/test/file/path";

}  // namespace

// A test ui::SelectFileDialog.
class TestSelectFileDialog : public ui::SelectFileDialog {
 public:
  TestSelectFileDialog(Listener* listener,
                       std::unique_ptr<ui::SelectFilePolicy> policy,
                       base::FilePath selected_path)
      : ui::SelectFileDialog(listener, std::move(policy)),
        selected_path_(selected_path) {}

  TestSelectFileDialog(const TestSelectFileDialog&) = delete;
  TestSelectFileDialog& operator=(const TestSelectFileDialog&) = delete;

 protected:
  void SelectFileImpl(Type type,
                      const std::u16string& title,
                      const base::FilePath& default_path,
                      const FileTypeInfo* file_types,
                      int file_type_index,
                      const base::FilePath::StringType& default_extension,
                      gfx::NativeWindow owning_window,
                      const GURL* caller) override {
    if (selected_path_.empty()) {
      listener_->FileSelectionCanceled();
      return;
    }

    listener_->FileSelected(ui::SelectedFileInfo(selected_path_), /*index=*/0);
  }

  bool IsRunning(gfx::NativeWindow owning_window) const override {
    return true;
  }
  void ListenerDestroyed() override { listener_ = nullptr; }
  bool HasMultipleFileTypeChoicesImpl() override { return false; }

 private:
  ~TestSelectFileDialog() override = default;

  // The simulated file path selected by the user.
  base::FilePath selected_path_;
};

// A factory associated with the artificial file picker.
class TestSelectFileDialogFactory : public ui::SelectFileDialogFactory {
 public:
  explicit TestSelectFileDialogFactory(base::FilePath selected_path)
      : selected_path_(selected_path) {}

  ui::SelectFileDialog* Create(
      ui::SelectFileDialog::Listener* listener,
      std::unique_ptr<ui::SelectFilePolicy> policy) override {
    return new TestSelectFileDialog(listener, std::move(policy),
                                    selected_path_);
  }

  TestSelectFileDialogFactory(const TestSelectFileDialogFactory&) = delete;
  TestSelectFileDialogFactory& operator=(const TestSelectFileDialogFactory&) =
      delete;

 private:
  // The simulated file path selected by the user.
  base::FilePath selected_path_;
};

// A fake impl of ScanningAppDelegate.
class FakeScanningAppDelegate : public ScanningAppDelegate {
 public:
  FakeScanningAppDelegate() = default;

  FakeScanningAppDelegate(const FakeScanningAppDelegate&) = delete;
  FakeScanningAppDelegate& operator=(const FakeScanningAppDelegate&) = delete;

  std::unique_ptr<ui::SelectFilePolicy> CreateChromeSelectFilePolicy()
      override {
    return nullptr;
  }

  std::string GetBaseNameFromPath(const base::FilePath& path) override {
    return path.BaseName().value();
  }

  base::FilePath GetMyFilesPath() override {
    return base::FilePath(kTestFilePath);
  }

  bool IsFilePathSupported(const base::FilePath& path_to_file) override {
    return !path_to_file.ReferencesParent() &&
           my_files_path_.IsParent(path_to_file);
  }

  void OpenFilesInMediaApp(
      const std::vector<base::FilePath>& file_paths) override {
    DCHECK(!file_paths.empty());
    file_paths_ = file_paths;
  }

  void ShowFileInFilesApp(
      const base::FilePath& path_to_file,
      base::OnceCallback<void(const bool)> callback) override {
    std::move(callback).Run(kTestFilePath == path_to_file.value());
  }

  void SaveScanSettingsToPrefs(const std::string& scan_settings) override {
    scan_settings_ = scan_settings;
  }

  std::string GetScanSettingsFromPrefs() override { return scan_settings_; }

  BindScanServiceCallback GetBindScanServiceCallback(
      content::WebUI* web_ui) override {
    return base::DoNothing();
  }

  // Returns the file paths saved in OpenFilesInMediaApp().
  const std::vector<base::FilePath>& file_paths() const { return file_paths_; }

  void SetMyFilesPath(base::FilePath my_files_path) {
    my_files_path_ = my_files_path;
  }

 private:
  std::vector<base::FilePath> file_paths_;
  std::string scan_settings_;
  base::FilePath my_files_path_;
};

class ScanningHandlerTest : public testing::Test {
 public:
  ScanningHandlerTest()
      : task_environment_(content::BrowserTaskEnvironment::REAL_IO_THREAD),
        web_ui_(),
        scanning_handler_() {}
  ~ScanningHandlerTest() override = default;

  void SetUp() override {
    auto delegate = std::make_unique<FakeScanningAppDelegate>();
    fake_scanning_app_delegate_ = delegate.get();
    scanning_handler_ = std::make_unique<ScanningHandler>(std::move(delegate));
    scanning_handler_->SetWebUIForTest(&web_ui_);
    scanning_handler_->RegisterMessages();

    base::Value::List args;
    web_ui_.HandleReceivedMessage("initialize", args);

    EXPECT_TRUE(temp_dir_.CreateUniqueTempDir());
    my_files_path_ = temp_dir_.GetPath().Append("MyFiles");
    EXPECT_TRUE(base::CreateDirectory(my_files_path_));
    fake_scanning_app_delegate_->SetMyFilesPath(my_files_path_);
  }

  void TearDown() override { ui::SelectFileDialog::SetFactory(nullptr); }

  // Gets the call data after a ScanningHandler WebUI call and asserts the
  // expected response.
  const content::TestWebUI::CallData& GetCallData(int size_before_call) {
    const std::vector<std::unique_ptr<content::TestWebUI::CallData>>&
        call_data_list = web_ui_.call_data();
    EXPECT_EQ(size_before_call + 1u, call_data_list.size());

    const content::TestWebUI::CallData& call_data = *call_data_list.back();
    EXPECT_EQ("cr.webUIResponse", call_data.function_name());
    EXPECT_EQ(kHandlerFunctionName, call_data.arg1()->GetString());
    // True if ResolveJavascriptCallback and false if RejectJavascriptCallback
    // is called by the handler.
    EXPECT_TRUE(call_data.arg2()->GetBool());
    return call_data;
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  content::TestWebUI web_ui_;
  std::unique_ptr<ScanningHandler> scanning_handler_;
  raw_ptr<FakeScanningAppDelegate> fake_scanning_app_delegate_;
  base::ScopedTempDir temp_dir_;
  base::FilePath my_files_path_;
};

// Validates that invoking the requestScanToLocation Web UI event opens the
// select dialog, and if a directory is chosen, returns the selected file path
// and base name.
TEST_F(ScanningHandlerTest, SelectDirectory) {
  const base::FilePath base_file_path("/this/is/a/test/directory/Base Name");
  ui::SelectFileDialog::SetFactory(
      std::make_unique<TestSelectFileDialogFactory>(base_file_path));

  const size_t call_data_count_before_call = web_ui_.call_data().size();
  base::Value::List args;
  args.Append(kHandlerFunctionName);
  web_ui_.HandleReceivedMessage("requestScanToLocation", args);

  const content::TestWebUI::CallData& call_data =
      GetCallData(call_data_count_before_call);
  ASSERT_TRUE(call_data.arg3()->is_dict());
  const base::Value::Dict& selected_path_dict = call_data.arg3()->GetDict();
  EXPECT_EQ(base_file_path.value(), *selected_path_dict.FindString("filePath"));
  EXPECT_EQ("Base Name", *selected_path_dict.FindString("baseName"));
}

// Validates that invoking the requestScanToLocation Web UI event opens the
// select dialog, and if the dialog is canceled, returns an empty file path and
// base name.
TEST_F(ScanningHandlerTest, CancelDialog) {
  ui::SelectFileDialog::SetFactory(
      std::make_unique<TestSelectFileDialogFactory>(base::FilePath()));

  const size_t call_data_count_before_call = web_ui_.call_data().size();
  base::Value::List args;
  args.Append(kHandlerFunctionName);
  web_ui_.HandleReceivedMessage("requestScanToLocation", args);

  const content::TestWebUI::CallData& call_data =
      GetCallData(call_data_count_before_call);
  ASSERT_TRUE(call_data.arg3()->is_dict());
  const base::Value::Dict& selected_path_dict = call_data.arg3()->GetDict();
  EXPECT_EQ("", *selected_path_dict.FindString("filePath"));
  EXPECT_EQ("", *selected_path_dict.FindString("baseName"));
}

// Validates that invoking the showFileInLocation Web UI event calls the
// OpenFilesAppFunction function and returns the callback with the boolean.
TEST_F(ScanningHandlerTest, ShowFileInLocation) {
  const size_t call_data_count_before_call = web_ui_.call_data().size();
  base::Value::List args;
  args.Append(kHandlerFunctionName);
  args.Append(kTestFilePath);
  web_ui_.HandleReceivedMessage("showFileInLocation", args);

  const content::TestWebUI::CallData& call_data =
      GetCallData(call_data_count_before_call);
  // Expect true from call to ShowFileInFilesApp().
  EXPECT_TRUE(call_data.arg3()->GetBool());
}

// Validates that invoking the getMyFilesPath Web UI event returns the correct
// path.
TEST_F(ScanningHandlerTest, GetMyFilesPath) {
  const size_t call_data_count_before_call = web_ui_.call_data().size();
  base::Value::List args;
  args.Append(kHandlerFunctionName);
  web_ui_.HandleReceivedMessage("getMyFilesPath", args);

  const content::TestWebUI::CallData& call_data =
      GetCallData(call_data_count_before_call);
  EXPECT_EQ(base::FilePath(kTestFilePath).value(),
            call_data.arg3()->GetString());
}

// Validates that invoking the openFilesInMediaApp Web UI event calls
// ChromeScanningAppDelegate.OpenFilesInMediaApp().
TEST_F(ScanningHandlerTest, OpenFilesInMediaApp) {
  const std::string file1 = "path/to/file/file1.jpg";
  const std::string file2 = "path/to/file/file2.jpg";
  base::Value::List file_paths_value;
  file_paths_value.Append(file1);
  file_paths_value.Append(file2);

  base::Value::List args;
  args.Append(std::move(file_paths_value));
  web_ui_.HandleReceivedMessage("openFilesInMediaApp", args);

  const std::vector<base::FilePath> expected_file_paths(
      {base::FilePath(file1), base::FilePath(file2)});
  EXPECT_EQ(expected_file_paths, fake_scanning_app_delegate_->file_paths());
}

// Validates that calling the saveScanSettings then the getScanSettings Web UI
// event invokes ChromeScanningAppDelegate.SaveScanSettingsToPrefs() and
// ChromeScanningAppDelegate.GetScanSettingsFromPrefs().
TEST_F(ScanningHandlerTest, ScanSettingsPrefs) {
  const std::string expected_sticky_settings = R"({
    "lastUsedScannerName": "Brother MFC-J497DW",
    "scanToPath": "path/to/file",
    "scanners": [
      {
        "name": "Brother MFC-J497DW",
        "lastScanDate": "2021-04-16T02:45:26.768Z",
        "sourceName": "ADF",
        "fileType": 2,
        "colorMode": 1,
        "pageSize": 2,
        "resolutionDpi": 100
      }
    ]
  })";

  // First, save the expected scan settings to the Pref service.
  base::Value::List save_args;
  save_args.Append(expected_sticky_settings);
  web_ui_.HandleReceivedMessage("saveScanSettings", save_args);

  // Then retrieve the expected scan settings from the Pref service.
  const size_t call_data_count_before_call = web_ui_.call_data().size();
  base::Value::List get_args;
  get_args.Append(kHandlerFunctionName);
  web_ui_.HandleReceivedMessage("getScanSettings", get_args);
  const content::TestWebUI::CallData& call_data =
      GetCallData(call_data_count_before_call);
  EXPECT_EQ(expected_sticky_settings, call_data.arg3()->GetString());
}

// Validates that invoking the ensureValidFilePath Web UI event with a valid
// file path returns the expected result.
TEST_F(ScanningHandlerTest, ValidFilePathExists) {
  const base::FilePath myScanPath = my_files_path_.Append("myScanPath");
  base::File(myScanPath, base::File::FLAG_CREATE | base::File::FLAG_READ);

  const size_t call_data_count_before_call = web_ui_.call_data().size();
  base::Value::List args;
  args.Append(kHandlerFunctionName);
  args.Append(myScanPath.value());
  web_ui_.HandleReceivedMessage("ensureValidFilePath", args);
  task_environment_.RunUntilIdle();

  const content::TestWebUI::CallData& call_data =
      GetCallData(call_data_count_before_call);
  ASSERT_TRUE(call_data.arg3()->is_dict());
  const base::Value::Dict& selected_path_dict = call_data.arg3()->GetDict();
  EXPECT_EQ(myScanPath.value(), *selected_path_dict.FindString("filePath"));
  EXPECT_EQ("myScanPath", *selected_path_dict.FindString("baseName"));
}

// Validates that invoking the ensureValidFilePath Web UI event with an invalid
// file path returns an object with an empty file path.
TEST_F(ScanningHandlerTest, InvalidFilePath) {
  const std::string invalidFilePath = "invalid/file/path";

  const size_t call_data_count_before_call = web_ui_.call_data().size();
  base::Value::List args;
  args.Append(kHandlerFunctionName);
  args.Append(invalidFilePath);
  web_ui_.HandleReceivedMessage("ensureValidFilePath", args);
  task_environment_.RunUntilIdle();

  const content::TestWebUI::CallData& call_data =
      GetCallData(call_data_count_before_call);
  ASSERT_TRUE(call_data.arg3()->is_dict());
  const base::Value::Dict& selected_path_dict = call_data.arg3()->GetDict();
  EXPECT_EQ(std::string(), *selected_path_dict.FindString("filePath"));
  EXPECT_EQ(std::string(), *selected_path_dict.FindString("baseName"));
}

// Validates a request for a plural string with a key missing in the plural
// string map does return a value.
TEST_F(ScanningHandlerTest, GetPluralStringBadKey) {
  base::Value::List args;
  args.Append(kHandlerFunctionName);
  args.Append(/*name=*/"incorrectKey");
  args.Append(/*count=*/2);
  web_ui_.HandleReceivedMessage("getPluralString", args);
  task_environment_.RunUntilIdle();

  const std::vector<std::unique_ptr<content::TestWebUI::CallData>>&
      call_data_list = web_ui_.call_data();
  EXPECT_EQ(0u, call_data_list.size());
}

}  // namespace ash
