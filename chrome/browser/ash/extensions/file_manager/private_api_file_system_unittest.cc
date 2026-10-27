// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/extensions/file_manager/private_api_file_system.h"

#include <memory>
#include <string>
#include <unordered_map>

#include "ash/webui/file_manager/url_constants.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/json/json_writer.h"
#include "base/memory/raw_ptr.h"
#include "base/pickle.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/ash/file_manager/fileapi_util.h"
#include "chrome/browser/ash/file_manager/path_util.h"
#include "chrome/browser/ash/fileapi/file_system_backend.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/scoped_user_manager.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "extensions/browser/api_test_utils.h"
#include "google_apis/gaia/gaia_id.h"
#include "storage/browser/file_system/external_mount_points.h"
#include "storage/common/file_system/file_system_mount_option.h"
#include "storage/common/file_system/file_system_types.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/clipboard/clipboard_buffer.h"
#include "ui/base/clipboard/clipboard_data.h"
#include "ui/base/clipboard/clipboard_non_backed.h"
#include "ui/base/clipboard/custom_data_helper.h"
#include "ui/base/data_transfer_policy/data_transfer_endpoint.h"
#include "url/gurl.h"

namespace extensions {
namespace {

class FileManagerPrivateGrantAccessUnitTest : public testing::Test {
 protected:
  void SetUp() override {
    testing::Test::SetUp();
    profile_manager_ = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal());
    ASSERT_TRUE(profile_manager_->SetUp());

    const AccountId account_id_a = AccountId::FromUserEmailGaiaId(
        "user_a@example.com", GaiaId("1111111111"));
    const AccountId account_id_b = AccountId::FromUserEmailGaiaId(
        "user_b@example.com", GaiaId("2222222222"));

    profile_a_ =
        profile_manager_->CreateTestingProfile(account_id_a.GetUserEmail());
    profile_b_ =
        profile_manager_->CreateTestingProfile(account_id_b.GetUserEmail());

    user_manager_->AddUserWithAffiliationAndTypeAndProfile(
        account_id_a, /*is_affiliated=*/false, user_manager::UserType::kRegular,
        profile_a_);
    user_manager_->UserLoggedIn(account_id_a,
                                "1111111111111111111111111111111111111111");
    user_manager_->SimulateUserProfileLoad(account_id_a);

    user_manager_->AddUserWithAffiliationAndTypeAndProfile(
        account_id_b, /*is_affiliated=*/false, user_manager::UserType::kRegular,
        profile_b_);
    user_manager_->UserLoggedIn(account_id_b,
                                "2222222222222222222222222222222222222222");
    user_manager_->SimulateUserProfileLoad(account_id_b);

    ASSERT_TRUE(downloads_dir_a_.CreateUniqueTempDir());
    ASSERT_TRUE(downloads_dir_b_.CreateUniqueTempDir());

    mount_name_a_ = file_manager::util::GetDownloadsMountPointName(profile_a_);
    mount_name_b_ = file_manager::util::GetDownloadsMountPointName(profile_b_);
    ASSERT_NE(mount_name_a_, mount_name_b_);

    storage::ExternalMountPoints* const system_mount_points =
        storage::ExternalMountPoints::GetSystemInstance();
    system_mount_points->RevokeFileSystem(mount_name_a_);
    system_mount_points->RevokeFileSystem(mount_name_b_);

    ASSERT_TRUE(system_mount_points->RegisterFileSystem(
        mount_name_a_, storage::kFileSystemTypeLocal,
        storage::FileSystemMountOption(), downloads_dir_a_.GetPath()));
    ASSERT_TRUE(system_mount_points->RegisterFileSystem(
        mount_name_b_, storage::kFileSystemTypeLocal,
        storage::FileSystemMountOption(), downloads_dir_b_.GetPath()));

    file_a_path_ = downloads_dir_a_.GetPath().AppendASCII("files_probe_A.txt");
    file_b_path_ = downloads_dir_b_.GetPath().AppendASCII("files_probe_B.txt");
    dir_b_path_ = downloads_dir_b_.GetPath().AppendASCII("files_probe_dir_B");
    child_b_path_ = dir_b_path_.AppendASCII("files_probe_child_1.txt");

    ASSERT_TRUE(base::WriteFile(file_a_path_, "A_FILES_HELLOWORLD"));
    ASSERT_TRUE(
        base::WriteFile(file_b_path_, "B_FILES_IAMANOTHERGOOGLEACCOUNTFILES"));
    ASSERT_TRUE(base::CreateDirectory(dir_b_path_));
    ASSERT_TRUE(base::WriteFile(child_b_path_, "B_DIR_FIRST_C214_VIDEO_01"));

    web_contents_a_ =
        content::WebContentsTester::CreateTestWebContents(profile_a_, nullptr);
  }

  void TearDown() override {
    if (auto* const clipboard = ui::Clipboard::GetForCurrentThread()) {
      clipboard->Clear(ui::ClipboardBuffer::kCopyPaste);
    }

    web_contents_a_.reset();

    storage::ExternalMountPoints* const system_mount_points =
        storage::ExternalMountPoints::GetSystemInstance();
    system_mount_points->RevokeFileSystem(mount_name_a_);
    system_mount_points->RevokeFileSystem(mount_name_b_);

    profile_a_ = nullptr;
    profile_b_ = nullptr;
    profile_manager_->DeleteAllTestingProfiles();
    profile_manager_.reset();
    testing::Test::TearDown();
  }

  void RunGrantAccessFromProfileA(const std::string& entry_url) {
    auto function =
        base::MakeRefCounted<FileManagerPrivateGrantAccessFunction>();
    function->SetRenderFrameHost(web_contents_a_->GetPrimaryMainFrame());
    function->set_source_url(GURL(ash::file_manager::kChromeUIFileManagerURL));

    base::ListValue urls;
    urls.Append(entry_url);
    base::ListValue args;
    args.Append(std::move(urls));
    std::string args_json;
    ASSERT_TRUE(base::JSONWriter::Write(args, &args_json));

    EXPECT_TRUE(
        api_test_utils::RunFunction(function.get(), args_json, profile_a_));
  }

  std::string BuildExternalFileSystemUrl(const std::string& mount_name,
                                         const std::string& relative_path) {
    return base::StrCat(
        {"filesystem:", ash::file_manager::kChromeUIFileManagerURL, "external/",
         mount_name, "/", relative_path});
  }

  void SetClipboardFileSystemSources(const std::string& sources_urls) {
    auto* const clipboard = ui::ClipboardNonBacked::GetForCurrentThread();
    ASSERT_TRUE(clipboard);

    base::Pickle pickle;
    ui::WriteCustomDataToPickle(
        std::unordered_map<std::u16string, std::u16string>(
            {{u"fs/tag", u"filemanager-data"},
             {u"fs/sources", base::ASCIIToUTF16(sources_urls)}}),
        &pickle);

    auto data = std::make_unique<ui::ClipboardData>();
    data->SetCustomData(ui::ClipboardFormatType::DataTransferCustomType(),
                        pickle.AsStringView());
    data->set_source(ui::DataTransferEndpoint(
        GURL(ash::file_manager::kChromeUIFileManagerURL)));
    clipboard->WriteClipboardData(std::move(data));
  }

  content::BrowserTaskEnvironment task_environment_;
  content::RenderViewHostTestEnabler rvh_test_enabler_;
  user_manager::TypedScopedUserManager<ash::FakeChromeUserManager>
      user_manager_{std::make_unique<ash::FakeChromeUserManager>()};
  std::unique_ptr<TestingProfileManager> profile_manager_;
  raw_ptr<TestingProfile> profile_a_ = nullptr;
  raw_ptr<TestingProfile> profile_b_ = nullptr;
  base::ScopedTempDir downloads_dir_a_;
  base::ScopedTempDir downloads_dir_b_;
  std::string mount_name_a_;
  std::string mount_name_b_;
  base::FilePath file_a_path_;
  base::FilePath file_b_path_;
  base::FilePath dir_b_path_;
  base::FilePath child_b_path_;
  std::unique_ptr<content::WebContents> web_contents_a_;
};

TEST_F(FileManagerPrivateGrantAccessUnitTest,
       CrossProfileDownloadsGrantAccessShouldNotGrantPermissionToUserBFiles) {
  auto* const policy = content::ChildProcessSecurityPolicy::GetInstance();
  content::RenderProcessHost* const process_a =
      web_contents_a_->GetPrimaryMainFrame()->GetProcess();
  const content::ChildProcessId child_process_id_a = process_a->GetID();
  const int process_id_a = process_a->GetDeprecatedID();

  // Before any grant, User A's renderer cannot read or write User B's files.
  EXPECT_FALSE(policy->CanReadFile(child_process_id_a, file_b_path_));
  EXPECT_FALSE(policy->CanCreateReadWriteFile(process_id_a, file_b_path_));
  EXPECT_FALSE(policy->CanReadFile(child_process_id_a, child_b_path_));

  // Granting User A's own file grants access to A's file, but not B's file.
  const std::string url_a =
      BuildExternalFileSystemUrl(mount_name_a_, "files_probe_A.txt");
  RunGrantAccessFromProfileA(url_a);
  EXPECT_TRUE(policy->CanCreateReadWriteFile(process_id_a, file_a_path_));
  EXPECT_FALSE(policy->CanReadFile(child_process_id_a, file_b_path_));

  // User A requests grantAccess for User B's unshared Downloads file and dir.
  const std::string url_b_file =
      BuildExternalFileSystemUrl(mount_name_b_, "files_probe_B.txt");
  const std::string url_b_dir =
      BuildExternalFileSystemUrl(mount_name_b_, "files_probe_dir_B");
  RunGrantAccessFromProfileA(url_b_file);
  RunGrantAccessFromProfileA(url_b_dir);

  // Expected security boundary: User A's renderer should NOT be granted
  // read/create/write permissions to User B's unshared Downloads file or dir.
  EXPECT_FALSE(policy->CanReadFile(child_process_id_a, file_b_path_));
  EXPECT_FALSE(policy->CanCreateReadWriteFile(process_id_a, file_b_path_));
  EXPECT_FALSE(policy->CanReadFile(child_process_id_a, child_b_path_));
  EXPECT_FALSE(policy->CanCreateReadWriteFile(
      process_id_a, dir_b_path_.AppendASCII("files_probe_created_by_A.txt")));
}

TEST_F(FileManagerPrivateGrantAccessUnitTest,
       CrossProfileDownloadsGrantAccessAllowsClipboardSources) {
  auto* const policy = content::ChildProcessSecurityPolicy::GetInstance();
  content::RenderProcessHost* const process_a =
      web_contents_a_->GetPrimaryMainFrame()->GetProcess();
  const content::ChildProcessId child_process_id_a = process_a->GetID();
  const int process_id_a = process_a->GetDeprecatedID();

  const std::string url_b_file =
      BuildExternalFileSystemUrl(mount_name_b_, "files_probe_B.txt");
  const std::string url_b_dir =
      BuildExternalFileSystemUrl(mount_name_b_, "files_probe_dir_B");

  // Place only `url_b_file` on the system clipboard under 'fs/sources'.
  SetClipboardFileSystemSources(url_b_file);

  RunGrantAccessFromProfileA(url_b_file);
  RunGrantAccessFromProfileA(url_b_dir);

  // Only `file_b_path_` (which was on the clipboard) is granted access;
  // `dir_b_path_` and `child_b_path_` remain inaccessible.
  EXPECT_TRUE(policy->CanCreateReadWriteFile(process_id_a, file_b_path_));
  EXPECT_FALSE(policy->CanReadFile(child_process_id_a, child_b_path_));
  EXPECT_FALSE(policy->CanCreateReadWriteFile(
      process_id_a, dir_b_path_.AppendASCII("files_probe_created_by_A.txt")));
}

}  // namespace
}  // namespace extensions
