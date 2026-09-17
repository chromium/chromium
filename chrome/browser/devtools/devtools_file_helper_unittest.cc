// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/devtools_file_helper.h"

#include <vector>

#include "base/base_paths.h"
#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/files/scoped_temp_file.h"
#include "base/functional/callback_helpers.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/task/current_thread.h"
#include "base/test/android/content_uri_test_utils.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "base/test/test_file_util.h"
#include "base/test/test_future.h"
#include "base/uuid.h"
#include "chrome/browser/download/chrome_download_manager_delegate.h"
#include "chrome/browser/download/download_core_service.h"
#include "chrome/browser/download/download_core_service_factory.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/browser/file_system_access/file_system_access_permission_context_factory.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::IsEmpty;
using ::testing::IsNull;
using ::testing::NotNull;
using ::testing::Optional;
using ::testing::Pointee;
using ::testing::Return;
using ::testing::SizeIs;
using ::testing::StrictMock;
using ::testing::Test;

namespace {

#if BUILDFLAG(IS_WIN)
static const char kDownloadPath[] = "c:\\\\path\\to\\download";
#elif BUILDFLAG(IS_POSIX)
static const char kDownloadPath[] = "/path/to/download";
#endif  // BUILDFLAG(IS_WIN)

class MockDelegate : public DevToolsFileHelper::Delegate {
 public:
  MOCK_METHOD(void,
              FileSystemAdded,
              (const std::string&, const DevToolsFileHelper::FileSystem*),
              (override));
  MOCK_METHOD(void, FileSystemRemoved, (const std::string&), (override));
  MOCK_METHOD(void,
              FilePathsChanged,
              (const std::vector<std::string>&,
               const std::vector<std::string>&,
               const std::vector<std::string>&),
              (override));
};

class MockStorage : public DevToolsFileHelper::Storage {
 public:
  MOCK_METHOD(DevToolsFileHelper::FileSystem,
              RegisterFileSystem,
              (const base::FilePath&, const std::string&),
              (override));
  MOCK_METHOD(void, UnregisterFileSystem, (const base::FilePath&), (override));

  MOCK_METHOD(std::vector<base::FilePath>,
              GetDraggedFileSystemPaths,
              (const GURL&),
              (override));
};

}  // namespace

class DevToolsFileHelperTest : public Test {
 protected:
  StrictMock<MockDelegate>& delegate() const { return *delegate_; }
  DevToolsFileHelper* file_helper() const { return file_helper_.get(); }
  TestingProfile* profile() const { return profile_.get(); }
  StrictMock<MockStorage>& storage() const { return *storage_; }

  DevToolsFileHelper::SelectFileCallback FakeSelectFileCallback(
      ui::SelectedFileInfo file_info) {
    return base::BindLambdaForTesting(
        [file_info](DevToolsFileHelper::SelectedCallback selected_callback,
                    DevToolsFileHelper::CanceledCallback,
                    const base::FilePath&) {
          std::move(selected_callback).Run(file_info);
        });
  }

  // Runs Save() for `url` and returns the path that would be pre-filled in the
  // "Save as" dialog. The selection is canceled, so nothing is written to disk.
  base::FilePath GetSuggestedSavePath(const std::string& url) {
    base::FilePath suggested_path;
    base::test::TestFuture<void> canceled;
    file_helper()->Save(
        url, "some text", /* save_as */ true,
        /* is_base64 */ false,
        base::BindLambdaForTesting(
            [&](DevToolsFileHelper::SelectedCallback,
                DevToolsFileHelper::CanceledCallback canceled_callback,
                const base::FilePath& initial_path) {
              suggested_path = initial_path;
              std::move(canceled_callback).Run();
            }),
        base::DoNothing(), canceled.GetCallback());
    EXPECT_TRUE(canceled.Wait());
    return suggested_path;
  }

  // Saves to `path`, which makes it the directory against which subsequent
  // suggested save paths are resolved. DevToolsFileHelper keeps the last save
  // path in a process-global, so tests that care about the directory must
  // prime it explicitly rather than rely on the order tests run in.
  void PrimeLastSaveDirectory(const base::FilePath& path) {
    base::test::TestFuture<const std::string&> saved;
    file_helper()->Save("https://example.com/primer.txt", "primer",
                        /* save_as */ true,
                        /* is_base64 */ false,
                        FakeSelectFileCallback(ui::SelectedFileInfo(path)),
                        saved.GetCallback(), base::DoNothing());
    EXPECT_TRUE(saved.Wait());
  }

  void ConnectAutomaticFileSystem(const base::FilePath& path,
                                  const base::Uuid& uuid,
                                  bool already_known,
                                  bool permission_granted = true) {
    if (already_known) {
      ScopedDictPrefUpdate update(profile()->GetPrefs(),
                                  prefs::kDevToolsFileSystemPaths);
      update.Get().Set(path.AsUTF8Unsafe(), uuid.AsLowercaseString());
    }

    DevToolsFileHelper::FileSystem file_system{
        "automatic", "test", "filesystem:test", path.AsUTF8Unsafe()};

    if (permission_granted) {
      EXPECT_CALL(storage(), RegisterFileSystem(path, "automatic"))
          .WillOnce(Return(file_system));
      EXPECT_CALL(delegate(), FileSystemAdded(IsEmpty(), Pointee(file_system)));
    } else {
      EXPECT_CALL(delegate(), FileSystemAdded("<permission denied>", IsNull()));
    }

    base::MockCallback<DevToolsFileHelper::HandlePermissionsCallback>
        handle_permissions_callback;
    if (!already_known) {
      EXPECT_CALL(handle_permissions_callback, Run)
          .WillOnce(base::test::RunOnceCallback<2>(permission_granted));
    } else {
      EXPECT_CALL(handle_permissions_callback, Run).Times(0);
    }

    base::MockCallback<DevToolsFileHelper::ConnectCallback> connect_cb;
    EXPECT_CALL(connect_cb, Run(permission_granted));

    base::RunLoop run_loop;
    ON_CALL(delegate(), FileSystemAdded).WillByDefault([&] {
      run_loop.Quit();
    });

    file_helper()->ConnectAutomaticFileSystem(
        path.AsUTF8Unsafe(), uuid, /* add_if_missing */ !already_known,
        handle_permissions_callback.Get(), connect_cb.Get());
    run_loop.Run();
  }

  void SetUp() override {
    TestingProfile::Builder builder;
    profile_ = builder.Build();
    storage_ = std::make_unique<StrictMock<MockStorage>>();
    delegate_ = std::make_unique<StrictMock<MockDelegate>>();
    file_helper_ = std::make_unique<DevToolsFileHelper>(
        profile(), delegate_.get(), storage_.get());

    DownloadCoreServiceFactory::GetForBrowserContext(profile())
        ->SetDownloadManagerDelegateForTesting(
            std::make_unique<ChromeDownloadManagerDelegate>(profile()));
    DownloadPrefs::FromBrowserContext(profile())->SetDownloadPath(
        base::FilePath::FromASCII(kDownloadPath));
  }

  void TearDown() override {
    DownloadCoreServiceFactory::GetForBrowserContext(profile())
        ->SetDownloadManagerDelegateForTesting(nullptr);

    file_helper_.reset();
    delegate_.reset();
    storage_.reset();
    profile_.reset();
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<StrictMock<MockDelegate>> delegate_;
  std::unique_ptr<StrictMock<MockStorage>> storage_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<DevToolsFileHelper> file_helper_;
};

TEST_F(DevToolsFileHelperTest, SaveToFileBase64) {
  base::ScopedTempFile tf;
  ASSERT_TRUE(tf.Create());
  const std::vector<uint8_t> data{0, 'a', 's', 'm', 1, 0, 0, 0};

  base::RunLoop run_loop;
  file_helper()->Save(
      "https://example.com/test.wasm", "AGFzbQEAAAA=", /* save_as */ true,
      /* is_base64 */ true,
      FakeSelectFileCallback(ui::SelectedFileInfo(tf.path())),
      base::BindLambdaForTesting([&](const std::string&) { run_loop.Quit(); }),
      base::DoNothing());
  run_loop.Run();

  EXPECT_EQ(base::ReadFileToBytes(tf.path()), data);
}

TEST_F(DevToolsFileHelperTest, SaveToFileInvalidBase64) {
  base::ScopedTempFile tf;
  ASSERT_TRUE(tf.Create());

  base::RunLoop run_loop;
  file_helper()->Save(
      "https://example.com/test.wasm", "~~~~",
      /* save_as */ true,
      /* is_base64 */ true,
      FakeSelectFileCallback(ui::SelectedFileInfo(tf.path())),
      base::BindLambdaForTesting([&](const std::string&) { run_loop.Quit(); }),
      base::DoNothing());
  run_loop.Run();

  EXPECT_THAT(base::ReadFileToBytes(tf.path()), Optional(IsEmpty()));
}

TEST_F(DevToolsFileHelperTest, SaveToFileText) {
  base::ScopedTempFile tf;
  ASSERT_TRUE(tf.Create());
  const std::vector<uint8_t> data{'s', 'o', 'm', 'e', ' ', 't', 'e', 'x', 't'};

  base::RunLoop run_loop;
  file_helper()->Save(
      "https://example.com/test.txt", "some text",
      /* save_as */ true,
      /* is_base64 */ false,
      FakeSelectFileCallback(ui::SelectedFileInfo(tf.path())),
      base::BindLambdaForTesting([&](const std::string&) { run_loop.Quit(); }),
      base::DoNothing());
  run_loop.Run();

  EXPECT_EQ(base::ReadFileToBytes(tf.path()), data);
}

TEST_F(DevToolsFileHelperTest, Append) {
  base::ScopedTempFile tf;
  ASSERT_TRUE(tf.Create());
  const std::vector<uint8_t> data{'s', 'o', 'm', 'e', ' ', 't', 'e', 'x', 't'};

  base::test::TestFuture<const std::string&> future1;
  file_helper()->Save("https://example.com/test.txt", "some",
                      /* save_as */ true,
                      /* is_base64 */ false,
                      FakeSelectFileCallback(ui::SelectedFileInfo(tf.path())),
                      future1.GetCallback(), base::DoNothing());
  EXPECT_TRUE(future1.Wait());

  base::test::TestFuture<void> future2;
  file_helper()->Append("https://example.com/test.txt", " text",
                        future2.GetCallback());
  EXPECT_TRUE(future2.Wait());

  EXPECT_EQ(base::ReadFileToBytes(tf.path()), data);
}

TEST_F(DevToolsFileHelperTest, SuggestedSavePathUsesLastSaveDirectory) {
  base::ScopedTempFile tf;
  ASSERT_TRUE(tf.Create());
  PrimeLastSaveDirectory(tf.path());

  EXPECT_EQ(GetSuggestedSavePath("https://example.com/script.js"),
            tf.path().DirName().Append(FILE_PATH_LITERAL("script.js")));
}

TEST_F(DevToolsFileHelperTest, SuggestedSavePathFallsBackToDefaultName) {
  base::ScopedTempFile tf;
  ASSERT_TRUE(tf.Create());
  PrimeLastSaveDirectory(tf.path());
  const base::FilePath dir = tf.path().DirName();

  // URLs without a usable file name must fall back to the default name,
  // rather than turning the raw URL into a file name.
  for (const char* url : {"https://example.com/", "not a url", ""}) {
    EXPECT_EQ(GetSuggestedSavePath(url),
              dir.Append(FILE_PATH_LITERAL("download")))
        << url;
  }
}

// Regression test: `url` is page-controlled, so the file name derived from it
// must never be able to escape the target directory, no matter how the
// separators and parent references are escaped.
TEST_F(DevToolsFileHelperTest, SuggestedSavePathCannotEscapeDirectory) {
  base::ScopedTempFile tf;
  ASSERT_TRUE(tf.Create());
  PrimeLastSaveDirectory(tf.path());
  const base::FilePath dir = tf.path().DirName();

  for (const char* url : {
           "https://example.com/a%2F..%2F..%2Fevil.sh",
           "https://example.com/%2e%2e%2f%2e%2e%2fevil.sh",
           "https://example.com/a%5C..%5C..%5Cevil.sh",
           "https://example.com/%2Fetc%2Fpasswd",
       }) {
    const base::FilePath suggested = GetSuggestedSavePath(url);
    EXPECT_FALSE(suggested.ReferencesParent()) << url;
    EXPECT_EQ(suggested.DirName(), dir) << url;
  }
}

// Regression test: the suggested file name must not carry BiDi control
// characters, which can be used to spoof the extension shown in the dialog.
TEST_F(DevToolsFileHelperTest, SuggestedSavePathStripsBidiControlCharacters) {
  base::ScopedTempFile tf;
  ASSERT_TRUE(tf.Create());
  PrimeLastSaveDirectory(tf.path());

  const base::FilePath suggested =
      GetSuggestedSavePath("https://example.com/report%E2%80%AEfdp.exe");

  EXPECT_EQ(suggested.value().find(FILE_PATH_LITERAL("\u202e")),
            base::FilePath::StringType::npos);
  EXPECT_EQ(suggested.DirName(), tf.path().DirName());
}

#if BUILDFLAG(IS_ANDROID)
TEST_F(DevToolsFileHelperTest, SaveToFileContentUri) {
  base::ScopedTempFile tf;
  ASSERT_TRUE(tf.Create());
  const std::vector<uint8_t> data{'s', 'o', 'm', 'e', ' ', 't', 'e', 'x', 't'};

  base::FilePath content_uri =
      *base::test::android::GetContentUriFromCacheDirFilePath(tf.path());

  ui::SelectedFileInfo file_info(content_uri);
  file_info.display_name = "test.txt";

  base::RunLoop run_loop;
  file_helper()->Save(
      "https://example.com/test.txt", "some text",
      /* save_as */ true,
      /* is_base64 */ false, FakeSelectFileCallback(file_info),
      base::BindLambdaForTesting([&](const std::string&) { run_loop.Quit(); }),
      base::DoNothing());
  run_loop.Run();

  EXPECT_EQ(base::ReadFileToBytes(tf.path()), data);
}

TEST_F(DevToolsFileHelperTest, AppendContentUri) {
  base::ScopedTempFile tf;
  ASSERT_TRUE(tf.Create());
  const std::vector<uint8_t> data{'s', 'o', 'm', 'e', ' ', 't', 'e', 'x', 't'};

  base::FilePath content_uri =
      *base::test::android::GetContentUriFromCacheDirFilePath(tf.path());

  ui::SelectedFileInfo file_info(content_uri);
  file_info.display_name = "test.txt";

  base::test::TestFuture<const std::string&> future1;
  file_helper()->Save("https://example.com/test.txt", "some",
                      /* save_as */ true,
                      /* is_base64 */ false, FakeSelectFileCallback(file_info),
                      future1.GetCallback(), base::DoNothing());
  EXPECT_TRUE(future1.Wait());

  base::test::TestFuture<void> future2;
  file_helper()->Append("https://example.com/test.txt", " text",
                        future2.GetCallback());
  EXPECT_TRUE(future2.Wait());

  EXPECT_EQ(base::ReadFileToBytes(tf.path()), data);
}
#endif  // BUILDFLAG(IS_ANDROID)

TEST_F(DevToolsFileHelperTest, AddFileSystemWithIllegalTypeAutomatic) {
  EXPECT_CALL(delegate(), FileSystemAdded("<illegal type>", IsNull()));

  file_helper()->AddFileSystem("automatic", base::DoNothing(),
                               base::DoNothing());

  EXPECT_THAT(profile()->GetPrefs()->GetDict(prefs::kDevToolsFileSystemPaths),
              IsEmpty());
}

TEST_F(DevToolsFileHelperTest, AddFileSystemWithIllegalTypeUUID) {
  EXPECT_CALL(delegate(), FileSystemAdded("<illegal type>", IsNull()));

  file_helper()->AddFileSystem(
      base::Uuid::GenerateRandomV4().AsLowercaseString(), base::DoNothing(),
      base::DoNothing());

  EXPECT_THAT(profile()->GetPrefs()->GetDict(prefs::kDevToolsFileSystemPaths),
              IsEmpty());
}

TEST_F(DevToolsFileHelperTest, AddFileSystemWithSelectionCanceled) {
  base::MockCallback<DevToolsFileHelper::SelectFileCallback> select_file_cb;
  EXPECT_CALL(select_file_cb, Run).WillOnce(base::test::RunOnceClosure<1>());
  EXPECT_CALL(delegate(), FileSystemAdded("<selection cancelled>", IsNull()));

  file_helper()->AddFileSystem("", select_file_cb.Get(), base::DoNothing());

  EXPECT_THAT(profile()->GetPrefs()->GetDict(prefs::kDevToolsFileSystemPaths),
              IsEmpty());
}

TEST_F(DevToolsFileHelperTest, ConnectAutomaticFileSystemWithRelativePath) {
  base::MockCallback<DevToolsFileHelper::ConnectCallback> connect_cb;
  EXPECT_CALL(connect_cb, Run(false));
  EXPECT_CALL(delegate(), FileSystemAdded("<illegal path>", IsNull()));

  file_helper()->ConnectAutomaticFileSystem(
      "path/to/folder", base::Uuid::GenerateRandomV4(),
      /* add_if_missing */ false, base::DoNothing(), connect_cb.Get());

  EXPECT_THAT(profile()->GetPrefs()->GetDict(prefs::kDevToolsFileSystemPaths),
              IsEmpty());
}

TEST_F(DevToolsFileHelperTest, ConnectAutomaticFileSystemWithNetworkPath) {
  std::vector<std::string> network_paths = {"//attacker.com/share"};
#if BUILDFLAG(IS_WIN)
  network_paths.push_back("\\\\attacker.com\\share");
#endif

  for (const std::string& path_str : network_paths) {
    base::MockCallback<DevToolsFileHelper::ConnectCallback> connect_cb;
    EXPECT_CALL(connect_cb, Run(false));
    EXPECT_CALL(delegate(), FileSystemAdded("<illegal path>", IsNull()));

    file_helper()->ConnectAutomaticFileSystem(
        path_str, base::Uuid::GenerateRandomV4(),
        /* add_if_missing */ false, base::DoNothing(), connect_cb.Get());

    EXPECT_THAT(profile()->GetPrefs()->GetDict(prefs::kDevToolsFileSystemPaths),
                IsEmpty());
  }
}

TEST_F(DevToolsFileHelperTest, ConnectAutomaticFileSystemWithParentReferences) {
#if BUILDFLAG(IS_WIN)
  std::string traversal_path = "c:\\foo\\bar\\..\\baz";
#else
  std::string traversal_path = "/foo/bar/../baz";
#endif

  base::MockCallback<DevToolsFileHelper::ConnectCallback> connect_cb;
  EXPECT_CALL(connect_cb, Run(false));
  EXPECT_CALL(delegate(), FileSystemAdded("<illegal path>", IsNull()));

  file_helper()->ConnectAutomaticFileSystem(
      traversal_path, base::Uuid::GenerateRandomV4(),
      /* add_if_missing */ false, base::DoNothing(), connect_cb.Get());

  EXPECT_THAT(profile()->GetPrefs()->GetDict(prefs::kDevToolsFileSystemPaths),
              IsEmpty());
}

TEST_F(DevToolsFileHelperTest, ConnectAutomaticFileSystemWithNonExistentPath) {
  base::ScopedTempDir td;
  ASSERT_TRUE(td.CreateUniqueTempDirUnderPath(base::GetTempDirForTesting()));
  base::FilePath path = td.GetPath().AppendASCII("NonExistent");
  base::MockCallback<DevToolsFileHelper::ConnectCallback> connect_cb;
  EXPECT_CALL(connect_cb, Run(false));
  EXPECT_CALL(delegate(), FileSystemAdded("<illegal path>", IsNull()));

  base::RunLoop run_loop;
  ON_CALL(delegate(), FileSystemAdded).WillByDefault([&] { run_loop.Quit(); });
  file_helper()->ConnectAutomaticFileSystem(
      path.AsUTF8Unsafe(), base::Uuid::GenerateRandomV4(),
      /* add_if_missing */ true, base::DoNothing(), connect_cb.Get());
  run_loop.Run();

  EXPECT_THAT(profile()->GetPrefs()->GetDict(prefs::kDevToolsFileSystemPaths),
              IsEmpty());
}

TEST_F(DevToolsFileHelperTest, ConnectAutomaticFileSystemButNotAddingMissing) {
  base::ScopedTempDir td;
  ASSERT_TRUE(td.CreateUniqueTempDirUnderPath(base::GetTempDirForTesting()));
  base::FilePath path = td.GetPath();
  base::MockCallback<DevToolsFileHelper::ConnectCallback> connect_cb;
  base::RunLoop run_loop;
  EXPECT_CALL(connect_cb, Run(false)).WillOnce([&]() { run_loop.Quit(); });

  file_helper()->ConnectAutomaticFileSystem(
      path.AsUTF8Unsafe(), base::Uuid::GenerateRandomV4(),
      /* add_if_missing */ false, base::DoNothing(), connect_cb.Get());

  run_loop.Run();

  EXPECT_THAT(profile()->GetPrefs()->GetDict(prefs::kDevToolsFileSystemPaths),
              IsEmpty());
}

TEST_F(DevToolsFileHelperTest, ConnectAutomaticFileSystemWithSensitivePath) {
  base::FilePath sensitive_path = profile()->GetPath();

  base::MockCallback<DevToolsFileHelper::ConnectCallback> connect_cb;
  EXPECT_CALL(connect_cb, Run(false));
  EXPECT_CALL(delegate(), FileSystemAdded("<illegal path>", IsNull()));

  // The ConnectAutomaticFileSystem will call ConfirmSensitiveEntryAccess,
  // which runs asynchronously to check the blocklist.
  base::RunLoop run_loop;
  ON_CALL(delegate(), FileSystemAdded).WillByDefault([&] { run_loop.Quit(); });

  file_helper()->ConnectAutomaticFileSystem(
      sensitive_path.AsUTF8Unsafe(), base::Uuid::GenerateRandomV4(),
      /* add_if_missing */ true, base::DoNothing(), connect_cb.Get());

  run_loop.Run();

  EXPECT_THAT(profile()->GetPrefs()->GetDict(prefs::kDevToolsFileSystemPaths),
              IsEmpty());
}

TEST_F(DevToolsFileHelperTest, ConnectAutomaticFileSystemInfoBarDenied) {
  base::ScopedTempDir td;
  ASSERT_TRUE(td.CreateUniqueTempDirUnderPath(base::GetTempDirForTesting()));
  base::FilePath path = td.GetPath();

  ConnectAutomaticFileSystem(path, base::Uuid::GenerateRandomV4(),
                             /* already_known */ false,
                             /* permission_granted */ false);

  EXPECT_THAT(profile()->GetPrefs()->GetDict(prefs::kDevToolsFileSystemPaths),
              IsEmpty());
}

TEST_F(DevToolsFileHelperTest, ConnectAutomaticFileSystemAlreadyKnown) {
  base::ScopedTempDir td;
  ASSERT_TRUE(td.CreateUniqueTempDirUnderPath(base::GetTempDirForTesting()));
  base::FilePath path = td.GetPath();
  base::Uuid uuid = base::Uuid::GenerateRandomV4();

  EXPECT_THAT(file_helper()->GetFileSystems(), IsEmpty());

  ConnectAutomaticFileSystem(path, uuid, /* already_known */ true);

  const base::DictValue& file_system_paths_value =
      profile()->GetPrefs()->GetDict(prefs::kDevToolsFileSystemPaths);
  EXPECT_THAT(file_system_paths_value, SizeIs(1));
  EXPECT_THAT(file_system_paths_value.FindString(path.AsUTF8Unsafe()),
              Pointee(uuid.AsLowercaseString()));
}

TEST_F(DevToolsFileHelperTest, ConnectAutomaticFileSystemNewlyAdded) {
  EXPECT_THAT(file_helper()->GetFileSystems(), IsEmpty());
  base::ScopedTempDir td;
  ASSERT_TRUE(td.CreateUniqueTempDirUnderPath(base::GetTempDirForTesting()));
  base::FilePath path = td.GetPath();
  base::Uuid uuid = base::Uuid::GenerateRandomV4();

  ConnectAutomaticFileSystem(path, uuid, /* already_known */ false);

  const base::DictValue& file_system_paths_value =
      profile()->GetPrefs()->GetDict(prefs::kDevToolsFileSystemPaths);
  EXPECT_THAT(file_system_paths_value, SizeIs(1));
  EXPECT_THAT(file_system_paths_value.FindString(path.AsUTF8Unsafe()),
              Pointee(uuid.AsLowercaseString()));
}

TEST_F(DevToolsFileHelperTest, ConnectAndDisconnectKnownAutomaticFileSystem) {
  base::ScopedTempDir td;
  ASSERT_TRUE(td.CreateUniqueTempDirUnderPath(base::GetTempDirForTesting()));
  base::FilePath path = td.GetPath();
  base::Uuid uuid = base::Uuid::GenerateRandomV4();

  EXPECT_THAT(file_helper()->GetFileSystems(), IsEmpty());

  {
    // Connect the known automatic file system.
    ConnectAutomaticFileSystem(path, uuid, /* already_known */ true);
    EXPECT_TRUE(file_helper()->IsFileSystemAdded(path.AsUTF8Unsafe()));
  }

  EXPECT_CALL(storage(), UnregisterFileSystem(path));
  EXPECT_CALL(delegate(), FileSystemRemoved(path.AsUTF8Unsafe()));

  {
    // Disconnect the previously connected automatic file system.
    base::RunLoop run_loop;
    ON_CALL(delegate(), FileSystemRemoved).WillByDefault([&] {
      run_loop.Quit();
    });
    file_helper()->DisconnectAutomaticFileSystem(path.AsUTF8Unsafe());
    run_loop.Run();

    EXPECT_FALSE(file_helper()->IsFileSystemAdded(path.AsUTF8Unsafe()));
  }
}

TEST_F(DevToolsFileHelperTest, DisconnectAutomaticFileSystemNotConnected) {
  base::ScopedTempDir td;
  ASSERT_TRUE(td.CreateUniqueTempDirUnderPath(base::GetTempDirForTesting()));

  file_helper()->DisconnectAutomaticFileSystem(td.GetPath().AsUTF8Unsafe());
}

TEST_F(DevToolsFileHelperTest, RemoveAutomaticFileSystemNotConnected) {
  base::ScopedTempDir td;
  ASSERT_TRUE(td.CreateUniqueTempDirUnderPath(base::GetTempDirForTesting()));
  base::FilePath path = td.GetPath();
  base::Uuid uuid = base::Uuid::GenerateRandomV4();
  {
    ScopedDictPrefUpdate update(profile()->GetPrefs(),
                                prefs::kDevToolsFileSystemPaths);
    update.Get().Set(path.AsUTF8Unsafe(), uuid.AsLowercaseString());
  }

  file_helper()->RemoveFileSystem(path.AsUTF8Unsafe());

  EXPECT_THAT(profile()->GetPrefs()->GetDict(prefs::kDevToolsFileSystemPaths),
              IsEmpty());
}

TEST_F(DevToolsFileHelperTest, IsFileInFileSystem) {
  EXPECT_THAT(file_helper()->GetFileSystems(), IsEmpty());

  base::ScopedTempDir td;
  ASSERT_TRUE(td.CreateUniqueTempDirUnderPath(base::GetTempDirForTesting()));
  base::FilePath fs_path = td.GetPath();
  base::Uuid uuid = base::Uuid::GenerateRandomV4();

  // 1. Initially, should be false since no filesystem is connected/added.
  base::FilePath file_path = fs_path.AppendASCII("src").AppendASCII("index.js");
  EXPECT_FALSE(file_helper()->IsFileInFileSystem(file_path.AsUTF8Unsafe()));
  EXPECT_FALSE(file_helper()->IsFileInFileSystem(fs_path.AsUTF8Unsafe()));

  // 2. Add and connect a file system.
  ConnectAutomaticFileSystem(fs_path, uuid, /* already_known */ true);

  // 3. Now verify IsFileInFileSystem behaviors.
  // - Exact match should be true.
  EXPECT_TRUE(file_helper()->IsFileInFileSystem(fs_path.AsUTF8Unsafe()));
  // - Subpath should be true.
  EXPECT_TRUE(file_helper()->IsFileInFileSystem(file_path.AsUTF8Unsafe()));
  // - Siblings/outside paths should be false.
  base::FilePath parent_path = fs_path.DirName();
  EXPECT_FALSE(file_helper()->IsFileInFileSystem(parent_path.AsUTF8Unsafe()));

  base::FilePath other_path =
      parent_path.AppendASCII("OtherFolder").AppendASCII("file.txt");
  EXPECT_FALSE(file_helper()->IsFileInFileSystem(other_path.AsUTF8Unsafe()));

  // - Directory traversal attacks (using parent references) should be blocked.
  base::FilePath traversal_path =
      fs_path.AppendASCII("..").AppendASCII("escape.txt");
  EXPECT_FALSE(
      file_helper()->IsFileInFileSystem(traversal_path.AsUTF8Unsafe()));

  base::FilePath deep_traversal_path = fs_path.AppendASCII("src")
                                           .AppendASCII("..")
                                           .AppendASCII("..")
                                           .AppendASCII("escape.txt");
  EXPECT_FALSE(
      file_helper()->IsFileInFileSystem(deep_traversal_path.AsUTF8Unsafe()));

  // 4. Disconnect the file system and verify it returns to false.
  EXPECT_CALL(storage(), UnregisterFileSystem(fs_path));
  EXPECT_CALL(delegate(), FileSystemRemoved(fs_path.AsUTF8Unsafe()));
  {
    base::RunLoop run_loop_disconnect;
    ON_CALL(delegate(), FileSystemRemoved).WillByDefault([&] {
      run_loop_disconnect.Quit();
    });
    file_helper()->DisconnectAutomaticFileSystem(fs_path.AsUTF8Unsafe());
    run_loop_disconnect.Run();
  }

  EXPECT_FALSE(file_helper()->IsFileInFileSystem(fs_path.AsUTF8Unsafe()));
  EXPECT_FALSE(file_helper()->IsFileInFileSystem(file_path.AsUTF8Unsafe()));
}
