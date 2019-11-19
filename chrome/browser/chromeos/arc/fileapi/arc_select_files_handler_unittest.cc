// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/fileapi/arc_select_files_handler.h"

#include <string>

#include "base/json/json_reader.h"
#include "base/strings/stringprintf.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/chromeos/arc/fileapi/arc_select_files_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/chrome_select_file_policy.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/arc/mojom/file_system.mojom.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/shell_dialogs/select_file_dialog.h"

using JavaScriptResultCallback =
    content::RenderFrameHost::JavaScriptResultCallback;
using SelectFilesCallback = arc::mojom::FileSystemHost::SelectFilesCallback;
using arc::mojom::SelectFilesActionType;
using arc::mojom::SelectFilesRequest;
using arc::mojom::SelectFilesRequestPtr;
using testing::_;
using testing::Return;
using ui::SelectFileDialog;

namespace arc {

namespace {

constexpr char kTestingProfileName[] = "test-user";

MATCHER_P(FileTypeInfoMatcher, expected, "") {
  EXPECT_EQ(expected.extensions, arg.extensions);
  return true;
}

MATCHER_P(FilePathMatcher, expected, "") {
  EXPECT_EQ(expected.value(), arg.value());
  return true;
}

MATCHER_P(SelectFilesResultMatcher, expected, "") {
  EXPECT_EQ(expected->urls.size(), arg->urls.size());
  for (size_t i = 0; i < expected->urls.size(); ++i) {
    EXPECT_EQ(expected->urls[i], arg->urls[i]);
  }
  EXPECT_EQ(expected->picker_activity, arg->picker_activity);
  return true;
}

MATCHER_P(FileSelectorElementsMatcher, expected, "") {
  EXPECT_EQ(expected->directory_elements.size(),
            arg->directory_elements.size());
  for (size_t i = 0; i < expected->directory_elements.size(); ++i) {
    EXPECT_EQ(expected->directory_elements[i]->name,
              arg->directory_elements[i]->name);
  }
  EXPECT_EQ(expected->file_elements.size(), arg->file_elements.size());
  for (size_t i = 0; i < expected->file_elements.size(); ++i) {
    EXPECT_EQ(expected->file_elements[i]->name, arg->file_elements[i]->name);
  }
  return true;
}

mojom::FileSelectorElementPtr CreateElement(const std::string& name) {
  mojom::FileSelectorElementPtr element = mojom::FileSelectorElement::New();
  element->name = name;
  return element;
}

class MockSelectFileDialogHolder : public SelectFileDialogHolder {
 public:
  explicit MockSelectFileDialogHolder(ui::SelectFileDialog::Listener* listener)
      : SelectFileDialogHolder(listener) {}
  ~MockSelectFileDialogHolder() override = default;
  MOCK_METHOD5(SelectFile,
               bool(ui::SelectFileDialog::Type type,
                    const base::FilePath& default_path,
                    const ui::SelectFileDialog::FileTypeInfo* file_types,
                    int task_id,
                    bool show_android_picker_apps));
  MOCK_METHOD2(ExecuteJavaScript,
               void(const std::string&, JavaScriptResultCallback));
};

}  // namespace

class ArcSelectFilesHandlerTest : public testing::Test {
 public:
  ArcSelectFilesHandlerTest() = default;
  ~ArcSelectFilesHandlerTest() override = default;

  void SetUp() override {
    profile_manager_ = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal());
    ASSERT_TRUE(profile_manager_->SetUp());
    Profile* profile =
        profile_manager_->CreateTestingProfile(kTestingProfileName);

    arc_select_files_handler_ =
        std::make_unique<ArcSelectFilesHandler>(profile);

    std::unique_ptr<MockSelectFileDialogHolder> mock_dialog_holder =
        std::make_unique<MockSelectFileDialogHolder>(nullptr);
    mock_dialog_holder_ = mock_dialog_holder.get();
    arc_select_files_handler_->SetDialogHolderForTesting(
        std::move(mock_dialog_holder));
    ON_CALL(*mock_dialog_holder_, SelectFile(_, _, _, _, _))
        .WillByDefault(Return(true));
  }

  void TearDown() override {
    arc_select_files_handler_.reset();
    profile_manager_.reset();
  }

 protected:
  void CallSelectFilesAndCheckDialogType(
      SelectFilesActionType request_action_type,
      bool request_allow_multiple,
      SelectFileDialog::Type expected_dialog_type,
      bool expected_show_android_picker_apps) {
    SelectFilesRequestPtr request = SelectFilesRequest::New();
    request->action_type = request_action_type;
    request->allow_multiple = request_allow_multiple;

    EXPECT_CALL(*mock_dialog_holder_,
                SelectFile(expected_dialog_type, _, _, _,
                           expected_show_android_picker_apps))
        .Times(1);

    SelectFilesCallback callback;
    arc_select_files_handler_->SelectFiles(request, std::move(callback));
  }

  void CallOnFileSelectorEventAndCheckScript(
      mojom::FileSelectorEventType event_type,
      const std::string& target_name,
      const std::string& expected_script) {
    mojom::FileSelectorEventPtr event = mojom::FileSelectorEvent::New();
    event->type = event_type;
    event->click_target = mojom::FileSelectorElement::New();
    event->click_target->name = target_name;

    EXPECT_CALL(*mock_dialog_holder_, ExecuteJavaScript(expected_script, _))
        .Times(1);

    base::MockCallback<mojom::FileSystemHost::OnFileSelectorEventCallback>
        callback;
    EXPECT_CALL(std::move(callback), Run()).Times(1);

    arc_select_files_handler_->OnFileSelectorEvent(std::move(event),
                                                   callback.Get());
  }

  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfileManager> profile_manager_;
  std::unique_ptr<ArcSelectFilesHandler> arc_select_files_handler_;
  MockSelectFileDialogHolder* mock_dialog_holder_;
};

TEST_F(ArcSelectFilesHandlerTest, SelectFiles_DialogType) {
  CallSelectFilesAndCheckDialogType(SelectFilesActionType::GET_CONTENT, false,
                                    SelectFileDialog::SELECT_OPEN_FILE, true);
  CallSelectFilesAndCheckDialogType(SelectFilesActionType::GET_CONTENT, true,
                                    SelectFileDialog::SELECT_OPEN_MULTI_FILE,
                                    true);
  CallSelectFilesAndCheckDialogType(SelectFilesActionType::OPEN_DOCUMENT, false,
                                    SelectFileDialog::SELECT_OPEN_FILE, false);
  CallSelectFilesAndCheckDialogType(SelectFilesActionType::OPEN_DOCUMENT, true,
                                    SelectFileDialog::SELECT_OPEN_MULTI_FILE,
                                    false);
  CallSelectFilesAndCheckDialogType(
      SelectFilesActionType::OPEN_DOCUMENT_TREE, false,
      SelectFileDialog::SELECT_EXISTING_FOLDER, false);
  CallSelectFilesAndCheckDialogType(SelectFilesActionType::CREATE_DOCUMENT,
                                    true, SelectFileDialog::SELECT_SAVEAS_FILE,
                                    false);
}

TEST_F(ArcSelectFilesHandlerTest, SelectFiles_FileTypeInfo) {
  SelectFilesRequestPtr request = SelectFilesRequest::New();
  request->action_type = SelectFilesActionType::OPEN_DOCUMENT;
  request->mime_types.push_back("text/plain");
  request->task_id = 1234;

  SelectFileDialog::FileTypeInfo expected_file_type_info;
  expected_file_type_info.allowed_paths =
      SelectFileDialog::FileTypeInfo::ANY_PATH;
  std::vector<base::FilePath::StringType> extensions;
  extensions.push_back("text");
  extensions.push_back("txt");
  expected_file_type_info.extensions.push_back(extensions);

  EXPECT_CALL(
      *mock_dialog_holder_,
      SelectFile(_, _,
                 testing::Pointee(FileTypeInfoMatcher(expected_file_type_info)),
                 1234, _))
      .Times(1);

  base::MockCallback<SelectFilesCallback> callback;
  arc_select_files_handler_->SelectFiles(request, callback.Get());
}

TEST_F(ArcSelectFilesHandlerTest, SelectFiles_InitialDocumentPath) {
  SelectFilesRequestPtr request = SelectFilesRequest::New();
  request->action_type = SelectFilesActionType::OPEN_DOCUMENT;
  request->initial_document_path = arc::mojom::DocumentPath::New();
  request->initial_document_path->authority = "testing.provider";
  request->initial_document_path->path = {"doc:root", "doc:file1"};

  // "doc:file1" is expected to be ignored.
  base::FilePath expected_file_path = base::FilePath(
      "/special/arc-documents-provider/testing.provider/doc:root");

  EXPECT_CALL(*mock_dialog_holder_,
              SelectFile(_, FilePathMatcher(expected_file_path), _, _, _))
      .Times(1);

  base::MockCallback<SelectFilesCallback> callback;
  arc_select_files_handler_->SelectFiles(request, callback.Get());
}

TEST_F(ArcSelectFilesHandlerTest, FileSelected_CallbackCalled) {
  SelectFilesRequestPtr request = SelectFilesRequest::New();
  request->action_type = SelectFilesActionType::OPEN_DOCUMENT;

  base::MockCallback<SelectFilesCallback> callback;
  arc_select_files_handler_->SelectFiles(request, callback.Get());

  EXPECT_CALL(std::move(callback), Run(_)).Times(1);
  arc_select_files_handler_->FileSelected(base::FilePath(), 0, nullptr);
}

TEST_F(ArcSelectFilesHandlerTest, FileSelected_PickerActivitySelected) {
  std::string package_name = "com.google.photos";
  std::string activity_name = ".PickerActivity";

  SelectFilesRequestPtr request = SelectFilesRequest::New();
  request->action_type = SelectFilesActionType::OPEN_DOCUMENT;

  base::MockCallback<SelectFilesCallback> callback;
  arc_select_files_handler_->SelectFiles(request, callback.Get());

  mojom::SelectFilesResultPtr expected_result = mojom::SelectFilesResult::New();
  expected_result->picker_activity =
      base::StringPrintf("%s/%s", package_name.c_str(), activity_name.c_str());

  EXPECT_CALL(std::move(callback),
              Run(SelectFilesResultMatcher(expected_result.get())))
      .Times(1);

  arc_select_files_handler_->FileSelected(
      ConvertAndroidActivityToFilePath(package_name, activity_name), 0,
      nullptr);
}

TEST_F(ArcSelectFilesHandlerTest, FileSelectionCanceled_CallbackCalled) {
  SelectFilesRequestPtr request = SelectFilesRequest::New();
  request->action_type = SelectFilesActionType::OPEN_DOCUMENT;

  base::MockCallback<SelectFilesCallback> callback;
  arc_select_files_handler_->SelectFiles(request, callback.Get());

  mojom::SelectFilesResultPtr expected_result = mojom::SelectFilesResult::New();

  EXPECT_CALL(std::move(callback),
              Run(SelectFilesResultMatcher(expected_result.get())))
      .Times(1);
  arc_select_files_handler_->FileSelectionCanceled(nullptr);
}

TEST_F(ArcSelectFilesHandlerTest, OnFileSelectorEvent) {
  CallOnFileSelectorEventAndCheckScript(mojom::FileSelectorEventType::CLICK_OK,
                                        "", kScriptClickOk);
  CallOnFileSelectorEventAndCheckScript(
      mojom::FileSelectorEventType::CLICK_CANCEL, "", kScriptClickCancel);
  CallOnFileSelectorEventAndCheckScript(
      mojom::FileSelectorEventType::CLICK_DIRECTORY, "Click Target",
      base::StringPrintf(kScriptClickDirectory, "\"Click Target\""));
  CallOnFileSelectorEventAndCheckScript(
      mojom::FileSelectorEventType::CLICK_FILE, "Click\tTarget",
      base::StringPrintf(kScriptClickFile, "\"Click\\tTarget\""));

  // The handler executes click-cancel script in its destructor.
  EXPECT_CALL(*mock_dialog_holder_, ExecuteJavaScript(kScriptClickCancel, _))
      .Times(1);
}

TEST_F(ArcSelectFilesHandlerTest, GetFileSelectorElements) {
  EXPECT_CALL(*mock_dialog_holder_, ExecuteJavaScript(kScriptGetElements, _))
      .WillOnce(testing::Invoke(
          [](const std::string&, JavaScriptResultCallback callback) {
            std::move(callback).Run(
                base::JSONReader::Read("{\"dirNames\" :[\"dir1\", \"dir2\"],"
                                       " \"fileNames\":[\"file1\",\"file2\"]}")
                    .value());
          }));

  mojom::FileSelectorElementsPtr expectedElements =
      mojom::FileSelectorElements::New();
  expectedElements->directory_elements.push_back(CreateElement("dir1"));
  expectedElements->directory_elements.push_back(CreateElement("dir2"));
  expectedElements->file_elements.push_back(CreateElement("file1"));
  expectedElements->file_elements.push_back(CreateElement("file2"));

  base::MockCallback<mojom::FileSystemHost::GetFileSelectorElementsCallback>
      callback;
  EXPECT_CALL(std::move(callback),
              Run(FileSelectorElementsMatcher(expectedElements.get())))
      .Times(1);

  arc_select_files_handler_->GetFileSelectorElements(
      arc::mojom::GetFileSelectorElementsRequest::New(), callback.Get());

  // The handler executes click-cancel script in its destructor.
  EXPECT_CALL(*mock_dialog_holder_, ExecuteJavaScript(kScriptClickCancel, _))
      .Times(1);
}

}  // namespace arc
