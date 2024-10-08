// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/chromeos/policy/dlp/dlp_scoped_file_access_delegate.h"

#include <memory>

#include "base/base_paths.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/path_service.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "base/test/test_future.h"
#include "chrome/browser/apps/platform_apps/app_browsertest_util.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_file_access_copy_or_move_delegate_factory.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_files_controller.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager_factory.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_scoped_file_access_delegate.h"
#include "chrome/browser/chromeos/policy/dlp/test/mock_dlp_rules_manager.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/file_system_access/chrome_file_system_access_permission_context.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chromeos/dbus/dlp/dlp_client.h"
#include "chromeos/dbus/dlp/dlp_service.pb.h"
#include "chromeos/dbus/dlp/fake_dlp_client.h"
#include "components/file_access/scoped_file_access.h"
#include "components/file_access/test/mock_scoped_file_access_delegate.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/back_forward_cache.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/file_select_listener.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "storage/browser/file_system/file_system_url.h"
#include "ui/base/metadata/base_type_conversion.h"
#include "ui/shell_dialogs/select_file_dialog_factory.h"
#include "ui/shell_dialogs/select_file_policy.h"
#include "ui/shell_dialogs/selected_file_info.h"

namespace policy {
namespace {

// This class is used to answer file chooser requests with `file` without any
// user interaction.
class FileChooserDelegate : public content::WebContentsDelegate {
 public:
  explicit FileChooserDelegate(base::FilePath file) : file_(std::move(file)) {}
  void RunFileChooser(content::RenderFrameHost* render_frame_host,
                      scoped_refptr<content::FileSelectListener> listener,
                      const blink::mojom::FileChooserParams& params) override {
    std::vector<blink::mojom::FileChooserFileInfoPtr> files;
    auto file_info = blink::mojom::FileChooserFileInfo::NewNativeFile(
        blink::mojom::NativeFileInfo::New(file_.AppendASCII(""),
                                          std::u16string()));
    files.push_back(std::move(file_info));
    listener->FileSelected(std::move(files), base::FilePath(), params.mode);
  }
  const base::FilePath file_;
};

// Permission allowing access for file system access to avoid user interaction
// for accepting the access rights.
class TestFileSystemAccessPermissionContext
    : public ChromeFileSystemAccessPermissionContext {
 public:
  explicit TestFileSystemAccessPermissionContext(
      content::BrowserContext* context,
      url::Origin origin,
      const content::PathInfo& path_info)
      : ChromeFileSystemAccessPermissionContext(context),
        directory_(path_info) {
    read_grant_ = GetExtendedReadPermissionGrantForTesting(
        origin, path_info,
        ChromeFileSystemAccessPermissionContext::HandleType::kDirectory);
    write_grant_ = GetExtendedWritePermissionGrantForTesting(
        origin, path_info,
        ChromeFileSystemAccessPermissionContext::HandleType::kDirectory);
  }

  ~TestFileSystemAccessPermissionContext() override = default;

  bool CanObtainReadPermission(const url::Origin& origin) override {
    return true;
  }

  bool CanObtainWritePermission(const url::Origin& origin) override {
    return true;
  }

  content::PathInfo GetLastPickedDirectory(const url::Origin& origin,
                                           const std::string& id) override {
    return directory_;
  }

  void ConfirmSensitiveEntryAccess(
      const url::Origin& origin,
      const content::PathInfo& path_info,
      HandleType handle_type,
      UserAction user_action,
      content::GlobalRenderFrameHostId frame_id,
      base::OnceCallback<void(SensitiveEntryResult)> callback) override {
    std::move(callback).Run(ChromeFileSystemAccessPermissionContext::
                                SensitiveEntryResult::kAllowed);
  }

  scoped_refptr<content::FileSystemAccessPermissionGrant>
  GetReadPermissionGrant(const url::Origin& origin,
                         const content::PathInfo& path_info,
                         HandleType handle_type,
                         UserAction user_action) override {
    if (read_grant_) {
      return read_grant_;
    }
    return ChromeFileSystemAccessPermissionContext::GetReadPermissionGrant(
        origin, path_info, handle_type, user_action);
  }

  scoped_refptr<content::FileSystemAccessPermissionGrant>
  GetWritePermissionGrant(const url::Origin& origin,
                          const content::PathInfo& path_info,
                          HandleType handle_type,
                          UserAction user_action) override {
    if (write_grant_) {
      return write_grant_;
    }
    return ChromeFileSystemAccessPermissionContext::GetWritePermissionGrant(
        origin, path_info, handle_type, user_action);
  }

  scoped_refptr<content::FileSystemAccessPermissionGrant> read_grant_;
  scoped_refptr<content::FileSystemAccessPermissionGrant> write_grant_;
  content::PathInfo directory_;
};

// A class replacing the SelectFileDialog with a fake class that selects a
// predefined file path (file or directory).
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

    ui::SelectedFileInfo file(selected_path_, selected_path_);
    listener_->FileSelected(file, /*index=*/0);
  }

  bool IsRunning(gfx::NativeWindow owning_window) const override {
    return true;
  }
  void ListenerDestroyed() override {}
  bool HasMultipleFileTypeChoicesImpl() override { return false; }

 private:
  ~TestSelectFileDialog() override = default;

  // The simulated file path selected by the user.
  base::FilePath selected_path_;
};

// Factory creating the previous defined fake file select dialog.
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

}  // namespace

using ::testing::_;

class DlpScopedFileAccessDelegateBrowserTest : public InProcessBrowserTest {
 public:
  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    base::FilePath test_data_path;
    EXPECT_TRUE(
        base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &test_data_path));
    embedded_test_server()->ServeFilesFromDirectory(
        test_data_path.AppendASCII("chrome/test/data/dlp"));
    host_resolver()->AddRule("*", "127.0.0.1");
    EXPECT_TRUE(embedded_test_server()->Start());
    chromeos::DlpClient::Shutdown();
    chromeos::DlpClient::InitializeFake();
    delegate_ = std::make_unique<DlpScopedFileAccessDelegate>(
        base::BindRepeating(chromeos::DlpClient::Get));
    EXPECT_TRUE(tmp_.CreateUniqueTempDir());

    content::WebContents* const web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    GURL test_url =
        embedded_test_server()->GetURL("localhost", "/dlp_files_test.html");
    EXPECT_TRUE(content::NavigateToURL(web_contents, test_url));
    content::WebContentsConsoleObserver con_observer(web_contents);
    con_observer.SetPattern("db opened");
    EXPECT_TRUE(con_observer.Wait());

    fake_dlp_client_ =
        static_cast<chromeos::FakeDlpClient*>(chromeos::DlpClient::Get());
  }

  void TearDownOnMainThread() override {
    fake_dlp_client_ = nullptr;
  }

  // Executes `action` as JavaScript in the context of the opened website. The
  // actions is expected to trigger printing `expectedConsole` on the console.
  void TestJSAction(std::string action, std::string expectedConsole) {
    content::WebContents* const web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();

    content::WebContentsConsoleObserver console_observer(web_contents);
    console_observer.SetPattern(expectedConsole);

    EXPECT_TRUE(content::ExecJs(web_contents, action));

    EXPECT_TRUE(console_observer.Wait());
  }

  // Setup a delegate to answer file chooser requests with a specific file. The
  // returned value has to be kept in scope as long as requests should be
  // handled this way.
  std::unique_ptr<FileChooserDelegate> PrepareChooser() {
    base::FilePath file = tmp_.GetPath().AppendASCII(kTestFileName);
    {
      base::ScopedAllowBlockingForTesting allow_blocking;
      base::WriteFile(file, kTestContent);
    }
    std::unique_ptr<FileChooserDelegate> delegate(
        new FileChooserDelegate(std::move(file)));
    browser()->tab_strip_model()->GetActiveWebContents()->SetDelegate(
        delegate.get());
    return delegate;
  }

  // Setup a delegate to answer file picker requests with a specific file. If
  // `createFile` is set the file is created.
  void PrepareFilePicker(bool createFile) {
    base::FilePath file = tmp_.GetPath().AppendASCII(kTestFileName);
    if (createFile) {
      base::ScopedAllowBlockingForTesting allow_blocking;
      base::WriteFile(file, kTestContent);
    }
    auto factory = std::make_unique<TestSelectFileDialogFactory>(file);
    ui::SelectFileDialog::SetFactory(std::move(factory));
  }

  class PermissionContextHandle {
   public:
    PermissionContextHandle() = default;

    PermissionContextHandle(
        content::BrowserContext* browser_context,
        std::unique_ptr<TestFileSystemAccessPermissionContext>
            permission_context)
        : browser_context_(browser_context),
          permission_context_(std::move(permission_context)) {
      content::SetFileSystemAccessPermissionContext(browser_context_,
                                                    permission_context_.get());
    }

    ~PermissionContextHandle() {
      if (browser_context_) {
        content::SetFileSystemAccessPermissionContext(browser_context_,
                                                      nullptr);
      }
    }

    PermissionContextHandle(PermissionContextHandle&&) = default;
    PermissionContextHandle& operator=(PermissionContextHandle&&) = default;

   private:
    raw_ptr<content::BrowserContext> browser_context_;
    std::unique_ptr<TestFileSystemAccessPermissionContext> permission_context_;
  };

  // Setup a delegate to answer directory picker requests with a directory of a
  // specific file. If `createFile` is set the file is created.
  [[nodiscard]] PermissionContextHandle PrepareDirPicker(bool createFile) {
    base::FilePath file = tmp_.GetPath().AppendASCII(kTestFileName);
    if (createFile) {
      base::ScopedAllowBlockingForTesting allow_blocking;
      base::WriteFile(file, kTestContent);
    }
    auto factory =
        std::make_unique<TestSelectFileDialogFactory>(file.DirName());
    ui::SelectFileDialog::SetFactory(std::move(factory));

    content::WebContents* const web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();

    auto permission_context = std::make_unique<
        testing::NiceMock<TestFileSystemAccessPermissionContext>>(
        browser()->profile(), embedded_test_server()->GetOrigin(),
        content::PathInfo(file.DirName()));

    return PermissionContextHandle(web_contents->GetBrowserContext(),
                                   std::move(permission_context));
  }

 protected:
  const std::string kTestContent = "This is file content.";
  const std::string kErrorMessage = "Could not read file.";
  const std::string kTestFileName = "test_file.txt";
  std::unique_ptr<DlpScopedFileAccessDelegate> delegate_;
  base::ScopedTempDir tmp_;
  raw_ptr<chromeos::FakeDlpClient> fake_dlp_client_;
};

// These tests covers using the File API with dlp files. The parameter
// `isAllowed` indicates if the file access is allowed or nopt by dlp. The
// parameter `jsId` is the HTML id of the button used in the test.
class DlpScopedFileAccessDelegateFileApiBrowserTest
    : public DlpScopedFileAccessDelegateBrowserTest,
      public testing::WithParamInterface<
          std::tuple<bool /*isAllowed*/, std::string /*jsId*/>> {};

IN_PROC_BROWSER_TEST_P(DlpScopedFileAccessDelegateFileApiBrowserTest,
                       UploadFileApi) {
  auto [isAllowed, jsId] = GetParam();
  fake_dlp_client_->SetFileAccessAllowed(isAllowed);
  auto delegate = PrepareChooser();
  TestJSAction("document.getElementById('" + jsId + "').click()",
               isAllowed ? kTestContent.substr(1) : kErrorMessage);
}

INSTANTIATE_TEST_SUITE_P(Default,
                         DlpScopedFileAccessDelegateFileApiBrowserTest,
                         testing::Values(std::tuple(true, "file"),
                                         std::tuple(false, "file"),
                                         std::tuple(true, "file_worker"),
                                         std::tuple(false, "file_worker"),
                                         std::tuple(true, "file_shared"),
                                         std::tuple(false, "file_shared"),
                                         std::tuple(true, "file_service"),
                                         std::tuple(false, "file_service")));

IN_PROC_BROWSER_TEST_F(DlpScopedFileAccessDelegateBrowserTest,
                       UploadFrameIDBProtectedAllow) {
  fake_dlp_client_->SetFileAccessAllowed(true);
  auto delegate = PrepareChooser();
  TestJSAction("document.getElementById('idb_clear').click()", "cleared");
  TestJSAction("document.getElementById('idb_save').click()", "saved");
  TestJSAction("document.getElementById('idb_open').click()",
               kTestContent.substr(1));
}

IN_PROC_BROWSER_TEST_F(DlpScopedFileAccessDelegateBrowserTest,
                       UploadFrameIDBProtectedDeny) {
  fake_dlp_client_->SetFileAccessAllowed(false);
  auto delegate = PrepareChooser();
  TestJSAction("document.getElementById('idb_clear').click()", "cleared");
  TestJSAction("document.getElementById('idb_save').click()", "saved");
  TestJSAction("document.getElementById('idb_open').click()", kErrorMessage);
}

IN_PROC_BROWSER_TEST_F(DlpScopedFileAccessDelegateBrowserTest,
                       UploadFrameRestoreProtectedAllow) {
  fake_dlp_client_->SetFileAccessAllowed(true);
  auto delegate = PrepareChooser();
  TestJSAction("document.getElementById('idb_save').click()", "saved");

  NavigateParams params(browser(), GURL("about:blank"),
                        ui::PAGE_TRANSITION_AUTO_TOPLEVEL);
  params.disposition = WindowOpenDisposition::NEW_BACKGROUND_TAB;
  Navigate(&params);

  TestJSAction("document.getElementById('idb_cached').click()",
               kTestContent.substr(1));

  chrome::CloseTab(browser());

  chrome::RestoreTab(browser());

  content::WebContentsConsoleObserver console_observer(
      browser()->tab_strip_model()->GetActiveWebContents());
  console_observer.SetPattern("db opened");
  EXPECT_TRUE(console_observer.Wait());

  TestJSAction("document.getElementById('idb_cached').click()",
               kTestContent.substr(1));
}

IN_PROC_BROWSER_TEST_F(DlpScopedFileAccessDelegateBrowserTest,
                       UploadFrameRestoreProtectedDenyRestore) {
  fake_dlp_client_->SetFileAccessAllowed(true);
  auto delegate = PrepareChooser();
  TestJSAction("document.getElementById('idb_save').click()", "saved");

  NavigateParams params(browser(), GURL("about:blank"),
                        ui::PAGE_TRANSITION_AUTO_TOPLEVEL);
  params.disposition = WindowOpenDisposition::NEW_BACKGROUND_TAB;
  Navigate(&params);

  TestJSAction("document.getElementById('idb_cached').click()",
               kTestContent.substr(1));

  chrome::CloseTab(browser());

  fake_dlp_client_->SetFileAccessAllowed(false);

  chrome::RestoreTab(browser());

  content::WebContentsConsoleObserver console_observer(
      browser()->tab_strip_model()->GetActiveWebContents());
  console_observer.SetPattern("db opened");
  EXPECT_TRUE(console_observer.Wait());

  TestJSAction("document.getElementById('idb_cached').click()", kErrorMessage);
}

class MockDlpFilesController : public DlpFilesController {
 public:
  explicit MockDlpFilesController(DlpRulesManager& rules_manager)
      : DlpFilesController(rules_manager) {}
  MOCK_METHOD(
      (void),
      RequestCopyAccess,
      (const storage::FileSystemURL& source,
       const storage::FileSystemURL& destination,
       base::OnceCallback<void(std::unique_ptr<file_access::ScopedFileAccess>)>
           result_callback),
      (override));

 protected:
  MOCK_METHOD((std::optional<data_controls::Component>),
              MapFilePathToPolicyComponent,
              (Profile * profile, const base::FilePath& file_path),
              (override));

  MOCK_METHOD(bool,
              IsInLocalFileSystem,
              (const base::FilePath& file_path),
              (override));

  MOCK_METHOD(void,
              ShowDlpBlockedFiles,
              (std::optional<uint64_t> task_id,
               std::vector<base::FilePath> blocked_files,
               dlp::FileAction action),
              (override));
};

class DlpFileSystemAccessMoveTest
    : public DlpScopedFileAccessDelegateBrowserTest {
 protected:
  std::unique_ptr<MockDlpFilesController> files_controller;
};

IN_PROC_BROWSER_TEST_F(DlpFileSystemAccessMoveTest,
                       FileSystemAccessMoveProtectedAllow) {
  PermissionContextHandle permission_context_handle =
      PrepareDirPicker(/*createFile=*/true);

  DlpScopedFileAccessDelegate::Initialize(base::BindLambdaForTesting(
      [this]() -> chromeos::DlpClient* { return fake_dlp_client_.get(); }));

  policy::DlpRulesManagerFactory::GetInstance()->SetTestingFactory(
      browser()->profile(),
      base::BindLambdaForTesting(
          [this](content::BrowserContext* context)
              -> std::unique_ptr<KeyedService> {
            auto rules_manager = std::make_unique<MockDlpRulesManager>(
                Profile::FromBrowserContext(context));
            files_controller =
                std::make_unique<MockDlpFilesController>(*rules_manager);
            ON_CALL(*rules_manager, IsFilesPolicyEnabled)
                .WillByDefault(testing::Return(true));

            ON_CALL(*rules_manager, GetDlpFilesController())
                .WillByDefault(::testing::Return(&*files_controller));
            EXPECT_CALL(
                *files_controller,
                RequestCopyAccess(
                    testing::Property(
                        &storage::FileSystemURL::path,
                        testing::Property(&base::FilePath::value,
                                          testing::EndsWith(kTestFileName))),
                    testing::Property(
                        &storage::FileSystemURL::path,
                        testing::Property(&base::FilePath::value,
                                          testing::EndsWith("moved.txt"))),
                    _))
                .WillOnce(base::test::RunOnceCallback<2>(
                    std::make_unique<file_access::ScopedFileAccess>(
                        file_access::ScopedFileAccess::Allowed())));
            return rules_manager;
          }));

  base::MockRepeatingCallback<void(const ::dlp::AddFilesRequest,
                                   chromeos::DlpClient::AddFilesCallback)>
      addFiles;

  EXPECT_CALL(addFiles,
              Run(testing::Property(&::dlp::AddFilesRequest::add_file_requests,
                                    testing::ElementsAre(testing::Property(
                                        &::dlp::AddFileRequest::file_path,
                                        testing::EndsWith("moved.txt")))),
                  _))
      .WillOnce(base::test::RunOnceCallback<1>(
          ::dlp::AddFilesResponse::default_instance()));

  fake_dlp_client_->SetAddFilesMock(addFiles.Get());

  TestJSAction("document.getElementById('move_file').click()", "moved");
}

// These tests covers using the File Access API with dlp files. The parameter
// `isAllowed` indicates if the file access is allowed or nopt by dlp. The
// parameter `directoryPicker` tells if a directory picker should be used in
// this test, otherwise it is assumed that a file picker is used. The parameter
// `jsId` is the HTML id of the button used in the test.
class DlpScopedFileAccessDelegateFileSystemAccessApiBrowserTest
    : public DlpScopedFileAccessDelegateBrowserTest,
      public testing::WithParamInterface<std::tuple<bool /*isAllowed*/,
                                                    bool /*directoryPicker*/,
                                                    std::string /*jsId*/>> {};

IN_PROC_BROWSER_TEST_P(
    DlpScopedFileAccessDelegateFileSystemAccessApiBrowserTest,
    UploadFileSystemAccessApi) {
  auto [isAllowed, directoryPicker, jsId] = GetParam();
  fake_dlp_client_->SetFileAccessAllowed(isAllowed);

  std::optional<PermissionContextHandle> permission_context_handle;
  if (directoryPicker) {
    permission_context_handle = PrepareDirPicker(/*createFile=*/true);
  } else {
    PrepareFilePicker(/*createFile=*/true);
  }
  TestJSAction("document.getElementById('" + jsId + "').click()",
               isAllowed ? kTestContent.substr(1) : kErrorMessage);
}

INSTANTIATE_TEST_SUITE_P(
    Default,
    DlpScopedFileAccessDelegateFileSystemAccessApiBrowserTest,
    testing::Values(std::tuple(true, false, "open_file"),
                    std::tuple(false, false, "open_file"),
                    std::tuple(true, true, "open_file_dir"),
                    std::tuple(false, true, "open_file_dir"),
                    std::tuple(true, false, "open_file_worker"),
                    std::tuple(false, false, "open_file_worker"),
                    std::tuple(true, true, "open_file_dir_worker"),
                    std::tuple(false, true, "open_file_dir_worker"),
                    std::tuple(true, false, "open_file_shared"),
                    std::tuple(false, false, "open_file_shared"),
                    std::tuple(true, true, "open_file_dir_shared"),
                    std::tuple(false, true, "open_file_dir_shared"),
                    std::tuple(true, false, "open_file_service"),
                    std::tuple(false, false, "open_file_service"),
                    std::tuple(true, true, "open_file_dir_service"),
                    std::tuple(false, true, "open_file_dir_service")));

// These tests check if files written by the File System Access API are tried to
// be added the dlp daemon via the DlpClient.
class DlpScopedFileAccessDelegateBrowserTestFileSystemAccessDownload
    : public DlpScopedFileAccessDelegateBrowserTest,
      public testing::WithParamInterface<
          std::tuple<bool /*directoryPicker*/, std::string /*jsId*/>> {};

IN_PROC_BROWSER_TEST_P(
    DlpScopedFileAccessDelegateBrowserTestFileSystemAccessDownload,
    DownloadFileSystemAccessApi) {
  auto [directoryPicker, jsId] = GetParam();
  base::MockRepeatingCallback<void(const ::dlp::AddFilesRequest,
                                   chromeos::FakeDlpClient::AddFilesCallback)>
      request;
  EXPECT_CALL(request,
              Run(testing::Property(&::dlp::AddFilesRequest::add_file_requests,
                                    testing::ElementsAre(testing::Property(
                                        &::dlp::AddFileRequest::file_path,
                                        testing::EndsWith(kTestFileName)))),
                  _))
      .WillOnce(base::test::RunOnceCallback<1>(
          ::dlp::AddFilesResponse::default_instance()));
  fake_dlp_client_->SetAddFilesMock(request.Get());

  std::optional<PermissionContextHandle> permission_context_handle;
  if (directoryPicker) {
    permission_context_handle = PrepareDirPicker(/*createFile=*/false);
  } else {
    PrepareFilePicker(/*createFile=*/false);
  }

  TestJSAction("document.getElementById('" + jsId + "').click()", "saved");

  base::FilePath file = tmp_.GetPath().AppendASCII(kTestFileName);
  std::string content;
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    EXPECT_TRUE(base::ReadFileToString(file, &content));
  }
  EXPECT_EQ(kTestContent, content);
}

INSTANTIATE_TEST_SUITE_P(
    Default,
    DlpScopedFileAccessDelegateBrowserTestFileSystemAccessDownload,
    testing::Values(std::tuple(false, "save_file"),
                    std::tuple(true, "save_file_dir"),
                    std::tuple(false, "save_file_worker"),
                    std::tuple(true, "save_file_dir_worker"),
                    std::tuple(false, "save_file_shared"),
                    std::tuple(true, "save_file_dir_shared"),
                    std::tuple(false, "save_file_service"),
                    std::tuple(true, "save_file_dir_service")));

}  // namespace policy
