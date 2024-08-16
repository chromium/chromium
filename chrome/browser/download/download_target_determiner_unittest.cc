// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/download/download_target_determiner.h"

#include <stddef.h>
#include <stdint.h>

#include <optional>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/json/values_util.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/observer_list.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/test_future.h"
#include "build/build_config.h"
#include "chrome/browser/download/chrome_download_manager_delegate.h"
#include "chrome/browser/download/download_confirmation_result.h"
#include "chrome/browser/download/download_crx_util.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/browser/download/download_prompt_status.h"
#include "chrome/browser/download/download_stats.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/download/public/common/download_interrupt_reasons.h"
#include "components/download/public/common/download_target_info.h"
#include "components/download/public/common/mock_download_item.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/history_types.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/content/common/file_type_policies.h"
#include "components/safe_browsing/content/common/file_type_policies_test_util.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/user_manager/scoped_user_manager.h"
#include "content/public/browser/download_item_utils.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "extensions/buildflags/buildflags.h"
#include "net/base/mime_util.h"
#include "ppapi/buildflags/buildflags.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/shell_dialogs/selected_file_info.h"
#include "url/origin.h"

#if BUILDFLAG(ENABLE_PLUGINS)
#include "content/public/browser/plugin_service.h"
#include "content/public/browser/plugin_service_filter.h"
#include "content/public/common/webplugininfo.h"
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "extensions/common/extension.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/ash/policy/dlp/dlp_files_controller_ash.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_file_destination.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager_factory.h"
#include "chrome/browser/chromeos/policy/dlp/test/mock_dlp_rules_manager.h"
#endif

using download::DownloadItem;
using download::DownloadPathReservationTracker;
using ::testing::_;
using ::testing::AnyNumber;
using ::testing::Invoke;
using ::testing::Ref;
using ::testing::Return;
using ::testing::ReturnRef;
using ::testing::ReturnRefOfCopy;
using ::testing::Truly;
using ::testing::WithArg;
using ConflictAction = DownloadPathReservationTracker::FilenameConflictAction;
using safe_browsing::DownloadFileType;
using safe_browsing::FileTypePolicies;

namespace {

const char kTransientPathGenerationHistogram[] =
    "Download.PathGenerationEvent.Transient";

const char kTransientPathValidationHistogram[] =
    "Download.PathValidationResult.Transient";

template <typename T>
base::HistogramBase::Sample ToHistogramSample(T t) {
  return static_cast<base::HistogramBase::Sample>(t);
}

// No-op delegate.
class NullWebContentsDelegate : public content::WebContentsDelegate {
 public:
  NullWebContentsDelegate() {}
  ~NullWebContentsDelegate() override {}
};

// Google Mock action that posts a task to the current message loop that invokes
// the first argument of the mocked method as a callback. Said argument must be
// a base::OnceCallback<void(ParamType)>&. |result| must be of |ParamType| and
// is bound as that parameter. Example:
//   class FooClass {
//    public:
//     virtual void Foo(base::OnceCallback<void(bool)> callback);
//   };
//   ...
//   EXPECT_CALL(mock_fooclass_instance, Foo(callback))
//     .WillOnce(ScheduleCallback(false));
ACTION_P(ScheduleCallback, result0) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(arg0), result0));
}

ACTION_P2(ScheduleCallback2, result0, result1) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(arg0), result0, result1));
}

// Used with DownloadTestCase. Indicates the type of test case. The expectations
// for the test is set based on the type.
enum TestCaseType {
  SAVE_AS,
  AUTOMATIC,
  FORCED,  // Requires that forced_file_path be non-empty.
  TRANSIENT,
};

// Used with DownloadTestCase. Type of intermediate filename to expect.
enum TestCaseExpectIntermediate {
  EXPECT_CRDOWNLOAD,   // Expect path/to/target.crdownload.
  EXPECT_UNCONFIRMED,  // Expect path/to/Unconfirmed xxx.crdownload.
  EXPECT_LOCAL_PATH,   // Expect target path.
  EXPECT_EMPTY,        // Expect empty path. Only for downloads which will be
                       // marked interrupted or cancelled.
};

// Typical download test case. Used with
// DownloadTargetDeterminerTest::RunTestCase().
struct DownloadTestCase {
  // Type of test.
  TestCaseType test_type;

  // Expected danger type. Verified at the end of target determination.
  download::DownloadDangerType expected_danger_type;

  // Expected danger level. Verified at the end of target determination.
  DownloadFileType::DangerLevel expected_danger_level;

  // Value of DownloadItem::GetURL()
  const char* url;

  // Value of DownloadItem::GetMimeType()
  const char* mime_type;

  // Should be non-empty if |test_type| == FORCED. Value of GetForcedFilePath().
  const base::FilePath::CharType* forced_file_path;

  // Expected local path. Specified relative to the test download path.
  const base::FilePath::CharType* expected_local_path;

  // Expected target disposition. If this is TARGET_DISPOSITION_PROMPT, then the
  // test run will expect ChromeDownloadManagerDelegate to prompt the user for a
  // download location.
  DownloadItem::TargetDisposition expected_disposition;

  // Type of intermediate path to expect.
  TestCaseExpectIntermediate expected_intermediate;
};

// In this class, overridden methods thunk to mocked methods (names ending with
// underscore) because WithArgs doesn't support moving. The mocked methods thus
// take a reference to the callback.
class MockDownloadTargetDeterminerDelegate
    : public DownloadTargetDeterminerDelegate {
 public:
  void GetInsecureDownloadStatus(
      download::DownloadItem* item,
      const base::FilePath& path,
      GetInsecureDownloadStatusCallback cb) override {
    GetInsecureDownloadStatus_(item, path, cb);
  }
  MOCK_METHOD3(GetInsecureDownloadStatus_,
               void(download::DownloadItem*,
                    const base::FilePath&,
                    GetInsecureDownloadStatusCallback&));
  void CheckDownloadUrl(download::DownloadItem* item,
                        const base::FilePath& path,
                        CheckDownloadUrlCallback cb) override {
    CheckDownloadUrl_(item, path, cb);
  }
  MOCK_METHOD3(CheckDownloadUrl_,
               void(download::DownloadItem*,
                    const base::FilePath&,
                    CheckDownloadUrlCallback&));
  void NotifyExtensions(download::DownloadItem* item,
                        const base::FilePath& path,
                        NotifyExtensionsCallback cb) override {
    NotifyExtensions_(item, path, cb);
  }
  MOCK_METHOD3(NotifyExtensions_,
               void(download::DownloadItem*,
                    const base::FilePath&,
                    NotifyExtensionsCallback&));
  void RequestConfirmation(download::DownloadItem* item,
                           const base::FilePath& path,
                           DownloadConfirmationReason reason,
                           ConfirmationCallback cb) override {
    RequestConfirmation_(item, path, reason, cb);
  }
  MOCK_METHOD4(RequestConfirmation_,
               void(download::DownloadItem*,
                    const base::FilePath&,
                    DownloadConfirmationReason,
                    ConfirmationCallback&));
#if BUILDFLAG(IS_ANDROID)
  void RequestIncognitoWarningConfirmation(
      IncognitoWarningConfirmationCallback cb) override {
    RequestIncognitoWarningConfirmation_(std::move(cb));
  }
  MOCK_METHOD1(RequestIncognitoWarningConfirmation_,
               void(IncognitoWarningConfirmationCallback cb));
#endif
  void DetermineLocalPath(DownloadItem* item,
                          const base::FilePath& path,
                          download::LocalPathCallback cb) override {
    DetermineLocalPath_(item, path, cb);
  }
  MOCK_METHOD3(DetermineLocalPath_,
               void(DownloadItem*,
                    const base::FilePath&,
                    download::LocalPathCallback&));
  void ReserveVirtualPath(
      DownloadItem* download,
      const base::FilePath& virtual_path,
      bool create_directory,
      DownloadPathReservationTracker::FilenameConflictAction action,
      ReservedPathCallback cb) override {
    ReserveVirtualPath_(download, virtual_path, create_directory, action, cb);
  }
  MOCK_METHOD5(ReserveVirtualPath_,
               void(DownloadItem*,
                    const base::FilePath&,
                    bool,
                    DownloadPathReservationTracker::FilenameConflictAction,
                    ReservedPathCallback&));
  void GetFileMimeType(const base::FilePath& path,
                       GetFileMimeTypeCallback cb) override {
    GetFileMimeType_(path, cb);
  }
  MOCK_METHOD2(GetFileMimeType_,
               void(const base::FilePath&, GetFileMimeTypeCallback&));

  void SetupDefaults() {
    ON_CALL(*this, GetInsecureDownloadStatus_(_, _, _))
        .WillByDefault(WithArg<2>(ScheduleCallback(
            download::DownloadItem::InsecureDownloadStatus::UNKNOWN)));
    ON_CALL(*this, CheckDownloadUrl_(_, _, _))
        .WillByDefault(WithArg<2>(
            ScheduleCallback(download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS)));
    ON_CALL(*this, NotifyExtensions_(_, _, _))
        .WillByDefault(WithArg<2>(ScheduleCallback2(
            base::FilePath(), DownloadPathReservationTracker::UNIQUIFY)));
    ON_CALL(*this, ReserveVirtualPath_(_, _, _, _, _))
        .WillByDefault(Invoke(
            [](DownloadItem* download, const base::FilePath& virtual_path,
               bool create_directory,
               DownloadPathReservationTracker::FilenameConflictAction action,
               ReservedPathCallback& callback) {
              std::move(callback).Run(download::PathValidationResult::SUCCESS,
                                      virtual_path);
            }));
    ON_CALL(*this, RequestConfirmation_(_, _, _, _))
        .WillByDefault(
            Invoke(&MockDownloadTargetDeterminerDelegate::NullPromptUser));
    ON_CALL(*this, DetermineLocalPath_(_, _, _))
        .WillByDefault(Invoke(
            &MockDownloadTargetDeterminerDelegate::NullDetermineLocalPath));
    ON_CALL(*this, GetFileMimeType_(_, _))
        .WillByDefault(WithArg<1>(ScheduleCallback("")));
  }
 private:
  static void NullPromptUser(DownloadItem* download,
                             const base::FilePath& suggested_path,
                             DownloadConfirmationReason reason,
                             ConfirmationCallback& callback);
  static void NullDetermineLocalPath(DownloadItem* download,
                                     const base::FilePath& virtual_path,
                                     download::LocalPathCallback& callback);
};

class DownloadTargetDeterminerTest : public ChromeRenderViewHostTestHarness {
 public:
  DownloadTargetDeterminerTest() = default;

  DownloadTargetDeterminerTest(const DownloadTargetDeterminerTest&) = delete;
  DownloadTargetDeterminerTest& operator=(const DownloadTargetDeterminerTest&) =
      delete;

  // ::testing::Test
  void SetUp() override;
  void TearDown() override;

  // Creates MockDownloadItem and sets up default expectations.
  std::unique_ptr<download::MockDownloadItem> CreateActiveDownloadItem(
      int32_t id,
      const DownloadTestCase& test_case);

  // Sets the AutoOpenBasedOnExtension user preference for |path|.
  void EnableAutoOpenByUserBasedOnExtension(const base::FilePath& path);

  // Set the kDownloadDefaultDirectory managed preference to |path|.
  void SetManagedDownloadPath(const base::FilePath& path);

  // Set the kPromptForDownload user preference to |prompt|.
  void SetPromptForDownload(bool prompt);

  // Given the relative path |path|, returns the full path under the temporary
  // downloads directory.
  base::FilePath GetPathInDownloadDir(const base::FilePath::StringType& path);

  // Run |test_case| using |item|.
  void RunTestCase(const DownloadTestCase& test_case,
                   const base::FilePath& initial_virtual_path,
                   download::MockDownloadItem* item);

  // Runs |test_case| with |item|. When the DownloadTargetDeterminer is done,
  // returns the resulting DownloadTargetInfo and DangerLevel.
  struct TargetInfoAndDangerLevel {
    download::DownloadTargetInfo target_info;
    safe_browsing::DownloadFileType::DangerLevel danger_level;
  };
  TargetInfoAndDangerLevel RunDownloadTargetDeterminer(
      const base::FilePath& initial_virtual_path,
      download::MockDownloadItem* item);

  // Run through |test_case_count| tests in |test_cases|. A new MockDownloadItem
  // will be created for each test case and destroyed when the test case is
  // complete.
  void RunTestCasesWithActiveItem(const DownloadTestCase test_cases[],
                                  size_t test_case_count);

  // Verifies that |target_path|, |disposition|, |expected_danger_type| and
  // |intermediate_path| matches the expectations of |test_case|. Posts
  // |closure| to the current message loop when done.
  void VerifyDownloadTarget(const DownloadTestCase& test_case,
                            TargetInfoAndDangerLevel info);

  base::FilePath test_download_dir() const { return test_download_dir_; }

  const base::FilePath& test_virtual_dir() const {
    return test_virtual_dir_;
  }

  MockDownloadTargetDeterminerDelegate* delegate() {
    return &delegate_;
  }

  DownloadPrefs* download_prefs() {
    return download_prefs_.get();
  }

 protected:
  // ChromeRenderViewHostTestHarness overrides.
  TestingProfile::TestingFactories GetTestingFactories() const override {
    return {TestingProfile::TestingFactory{
        HistoryServiceFactory::GetInstance(),
        HistoryServiceFactory::GetDefaultFactory()}};
  }

 private:
  void SetUpFileTypePolicies();

  base::FilePath test_download_dir_;
  std::unique_ptr<DownloadPrefs> download_prefs_;
  ::testing::NiceMock<MockDownloadTargetDeterminerDelegate> delegate_;
  NullWebContentsDelegate web_contents_delegate_;
  base::FilePath test_virtual_dir_;
  safe_browsing::FileTypePoliciesTestOverlay file_type_configuration_;
};

void DownloadTargetDeterminerTest::SetUp() {
  ChromeRenderViewHostTestHarness::SetUp();
  CHECK(profile());

  test_download_dir_ = profile()->GetPath().AppendASCII("TestDownloadDir");

  download_prefs_ = std::make_unique<DownloadPrefs>(profile());
  download_prefs_->SkipSanitizeDownloadTargetPathForTesting();
  download_prefs_->SetDownloadPath(test_download_dir());
  web_contents()->SetDelegate(&web_contents_delegate_);
  test_virtual_dir_ = test_download_dir().Append(FILE_PATH_LITERAL("virtual"));
  delegate_.SetupDefaults();
  SetUpFileTypePolicies();
#if BUILDFLAG(IS_ANDROID)
  profile()->GetTestingPrefService()->SetInteger(
      prefs::kPromptForDownloadAndroid,
      static_cast<int>(DownloadPromptStatus::DONT_SHOW));
#endif
}

void DownloadTargetDeterminerTest::TearDown() {
  download_prefs_.reset();
  ChromeRenderViewHostTestHarness::TearDown();
}

std::unique_ptr<download::MockDownloadItem>
DownloadTargetDeterminerTest::CreateActiveDownloadItem(
    int32_t id,
    const DownloadTestCase& test_case) {
  std::unique_ptr<download::MockDownloadItem> item =
      std::make_unique<::testing::NiceMock<download::MockDownloadItem>>();
  GURL download_url(test_case.url);
  std::vector<GURL> url_chain;
  url_chain.push_back(download_url);
  base::FilePath forced_file_path =
      GetPathInDownloadDir(test_case.forced_file_path);
  DownloadItem::TargetDisposition initial_disposition =
      (test_case.test_type == SAVE_AS) ?
      DownloadItem::TARGET_DISPOSITION_PROMPT :
      DownloadItem::TARGET_DISPOSITION_OVERWRITE;
  EXPECT_TRUE((test_case.test_type != FORCED) || !forced_file_path.empty());
  content::DownloadItemUtils::AttachInfoForTesting(item.get(), profile(),
                                                   web_contents());

  ON_CALL(*item, GetDangerType())
      .WillByDefault(Return(download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS));
  ON_CALL(*item, GetForcedFilePath())
      .WillByDefault(ReturnRefOfCopy(forced_file_path));
  ON_CALL(*item, GetFullPath())
      .WillByDefault(ReturnRefOfCopy(base::FilePath()));
  ON_CALL(*item, GetHash())
      .WillByDefault(ReturnRefOfCopy(std::string()));
  ON_CALL(*item, GetId())
      .WillByDefault(Return(id));
  ON_CALL(*item, GetLastReason())
      .WillByDefault(Return(download::DOWNLOAD_INTERRUPT_REASON_NONE));
  ON_CALL(*item, GetMimeType())
      .WillByDefault(Return(test_case.mime_type));
  ON_CALL(*item, GetReferrerUrl())
      .WillByDefault(ReturnRefOfCopy(download_url));
  ON_CALL(*item, GetState())
      .WillByDefault(Return(DownloadItem::IN_PROGRESS));
  ON_CALL(*item, GetTargetDisposition())
      .WillByDefault(Return(initial_disposition));
  ON_CALL(*item, GetTargetFilePath())
      .WillByDefault(ReturnRefOfCopy(base::FilePath()));
  ON_CALL(*item, GetTransitionType())
      .WillByDefault(Return(ui::PAGE_TRANSITION_LINK));
  ON_CALL(*item, GetURL())
      .WillByDefault(ReturnRefOfCopy(download_url));
  ON_CALL(*item, GetUrlChain())
      .WillByDefault(ReturnRefOfCopy(url_chain));
  ON_CALL(*item, HasUserGesture())
      .WillByDefault(Return(true));
  ON_CALL(*item, IsDangerous())
      .WillByDefault(Return(false));
  ON_CALL(*item, IsTemporary())
      .WillByDefault(Return(false));
  ON_CALL(*item, IsTransient())
      .WillByDefault(Return(test_case.test_type == TestCaseType::TRANSIENT));

  return item;
}

void DownloadTargetDeterminerTest::EnableAutoOpenByUserBasedOnExtension(
    const base::FilePath& path) {
  EXPECT_TRUE(download_prefs_->EnableAutoOpenByUserBasedOnExtension(path));
}

void DownloadTargetDeterminerTest::SetManagedDownloadPath(
    const base::FilePath& path) {
  profile()->GetTestingPrefService()->SetManagedPref(
      prefs::kDownloadDefaultDirectory,
      base::Value::ToUniquePtrValue(base::FilePathToValue(path)));
}

void DownloadTargetDeterminerTest::SetPromptForDownload(bool prompt) {
  profile()->GetTestingPrefService()->
      SetBoolean(prefs::kPromptForDownload, prompt);
#if BUILDFLAG(IS_ANDROID)
  DownloadPromptStatus download_prompt_status =
      prompt ? DownloadPromptStatus::SHOW_PREFERENCE
             : DownloadPromptStatus::DONT_SHOW;
  profile()->GetTestingPrefService()->SetInteger(
      prefs::kPromptForDownloadAndroid,
      static_cast<int>(download_prompt_status));
#endif
}

base::FilePath DownloadTargetDeterminerTest::GetPathInDownloadDir(
    const base::FilePath::StringType& relative_path) {
  if (relative_path.empty())
    return base::FilePath();
  base::FilePath full_path(test_download_dir().Append(relative_path));
  return full_path.NormalizePathSeparators();
}

void DownloadTargetDeterminerTest::RunTestCase(
    const DownloadTestCase& test_case,
    const base::FilePath& initial_virtual_path,
    download::MockDownloadItem* item) {
  TargetInfoAndDangerLevel target_info =
      RunDownloadTargetDeterminer(initial_virtual_path, item);
  VerifyDownloadTarget(test_case, std::move(target_info));
}

DownloadTargetDeterminerTest::TargetInfoAndDangerLevel
DownloadTargetDeterminerTest::RunDownloadTargetDeterminer(
    const base::FilePath& initial_virtual_path,
    download::MockDownloadItem* item) {
  base::test::TestFuture<download::DownloadTargetInfo,
                         safe_browsing::DownloadFileType::DangerLevel>
      future;
  DownloadTargetDeterminer::Start(
      item, initial_virtual_path, DownloadPathReservationTracker::UNIQUIFY,
      download_prefs_.get(), delegate(), future.GetCallback());
  TargetInfoAndDangerLevel info{.target_info = future.Get<0>(),
                                .danger_level = future.Get<1>()};
  ::testing::Mock::VerifyAndClearExpectations(delegate());
  return info;
}

void DownloadTargetDeterminerTest::RunTestCasesWithActiveItem(
    const DownloadTestCase test_cases[],
    size_t test_case_count) {
  for (size_t i = 0; i < test_case_count; ++i) {
    std::unique_ptr<download::MockDownloadItem> item =
        CreateActiveDownloadItem(i, test_cases[i]);
    SCOPED_TRACE(testing::Message() << "Running test case " << i);
    RunTestCase(test_cases[i], base::FilePath(), item.get());
  }
}

void DownloadTargetDeterminerTest::VerifyDownloadTarget(
    const DownloadTestCase& test_case,
    TargetInfoAndDangerLevel info) {
  base::FilePath expected_local_path(
      GetPathInDownloadDir(test_case.expected_local_path));
  EXPECT_EQ(expected_local_path.value(), info.target_info.target_path.value());
  EXPECT_EQ(test_case.expected_disposition,
            info.target_info.target_disposition);
  EXPECT_EQ(test_case.expected_danger_type, info.target_info.danger_type);
  EXPECT_EQ(test_case.expected_danger_level, info.danger_level);

  switch (test_case.expected_intermediate) {
    case EXPECT_CRDOWNLOAD:
      EXPECT_EQ(DownloadTargetDeterminer::GetCrDownloadPath(
                    info.target_info.target_path)
                    .value(),
                info.target_info.intermediate_path.value());
      break;

    case EXPECT_UNCONFIRMED:
      // The paths (in English) look like: /path/Unconfirmed xxx.crdownload.
      // Of this, we only check that the path is:
      // 1. Not "/path/target.crdownload",
      // 2. Points to the same directory as the target.
      // 3. Has extension ".crdownload".
      // 4. Basename starts with "Unconfirmed ".
      EXPECT_NE(DownloadTargetDeterminer::GetCrDownloadPath(expected_local_path)
                    .value(),
                info.target_info.intermediate_path.value());
      EXPECT_EQ(expected_local_path.DirName().value(),
                info.target_info.intermediate_path.DirName().value());
      EXPECT_TRUE(info.target_info.intermediate_path.MatchesExtension(
          FILE_PATH_LITERAL(".crdownload")));
      EXPECT_EQ(0u, info.target_info.intermediate_path.BaseName().value().find(
                        FILE_PATH_LITERAL("Unconfirmed ")));
      break;

    case EXPECT_LOCAL_PATH:
      EXPECT_EQ(expected_local_path.value(),
                info.target_info.intermediate_path.value());
      break;

    case EXPECT_EMPTY:
      EXPECT_TRUE(info.target_info.intermediate_path.empty());
      break;
  }
}

void DownloadTargetDeterminerTest::SetUpFileTypePolicies() {
  std::unique_ptr<safe_browsing::DownloadFileTypeConfig> fake_file_type_config =
      std::make_unique<safe_browsing::DownloadFileTypeConfig>();
  auto* file_type = fake_file_type_config->mutable_default_file_type();
  file_type->set_uma_value(-1);
  auto* platform_settings = file_type->add_platform_settings();
  platform_settings->set_danger_level(DownloadFileType::NOT_DANGEROUS);
  platform_settings->set_auto_open_hint(DownloadFileType::ALLOW_AUTO_OPEN);

  file_type = fake_file_type_config->add_file_types();
  file_type->set_extension("kindabad");
  file_type->set_uma_value(-1);
  platform_settings = file_type->add_platform_settings();
  platform_settings->set_danger_level(DownloadFileType::ALLOW_ON_USER_GESTURE);
  platform_settings->set_auto_open_hint(DownloadFileType::ALLOW_AUTO_OPEN);

  file_type = fake_file_type_config->add_file_types();
  file_type->set_extension("bad");
  file_type->set_uma_value(-1);
  file_type->set_ping_setting(DownloadFileType::FULL_PING);
  platform_settings = file_type->add_platform_settings();
  platform_settings->set_danger_level(DownloadFileType::DANGEROUS);
  platform_settings->set_auto_open_hint(DownloadFileType::DISALLOW_AUTO_OPEN);

  file_type_configuration_.SwapConfig(fake_file_type_config);
}

// static
void MockDownloadTargetDeterminerDelegate::NullPromptUser(
    DownloadItem* download,
    const base::FilePath& suggested_path,
    DownloadConfirmationReason reason,
    ConfirmationCallback& callback) {
  std::move(callback).Run(DownloadConfirmationResult::CONFIRMED,
                          ui::SelectedFileInfo(suggested_path));
}

// static
void MockDownloadTargetDeterminerDelegate::NullDetermineLocalPath(
    DownloadItem* download,
    const base::FilePath& virtual_path,
    download::LocalPathCallback& callback) {
  std::move(callback).Run(virtual_path, virtual_path.BaseName());
}

// NotifyExtensions implementation that overrides the path so that the target
// file is in a subdirectory called 'overridden'. If the extension is '.remove',
// the extension is removed.
void NotifyExtensionsOverridePath(
    download::DownloadItem* download,
    const base::FilePath& path,
    DownloadTargetDeterminerDelegate::NotifyExtensionsCallback& callback) {
  base::FilePath new_path =
      base::FilePath()
      .AppendASCII("overridden")
      .Append(path.BaseName());
  if (new_path.MatchesExtension(FILE_PATH_LITERAL(".remove")))
    new_path = new_path.RemoveExtension();
  std::move(callback).Run(new_path, DownloadPathReservationTracker::UNIQUIFY);
}

TEST_F(DownloadTargetDeterminerTest, Basic) {
  const DownloadTestCase kBasicTestCases[] = {
      {// Automatic Safe
       AUTOMATIC, download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
       DownloadFileType::NOT_DANGEROUS, "http://example.com/foo.txt",
       "text/plain", FILE_PATH_LITERAL(""),

       FILE_PATH_LITERAL("foo.txt"), DownloadItem::TARGET_DISPOSITION_OVERWRITE,

       EXPECT_CRDOWNLOAD},

      {// Save_As Safe
       SAVE_AS, download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
       DownloadFileType::NOT_DANGEROUS, "http://example.com/foo.txt",
       "text/plain", FILE_PATH_LITERAL(""),

       FILE_PATH_LITERAL("foo.txt"), DownloadItem::TARGET_DISPOSITION_PROMPT,

       EXPECT_CRDOWNLOAD},

      {// Automatic Dangerous
       AUTOMATIC, download::DOWNLOAD_DANGER_TYPE_DANGEROUS_FILE,
       DownloadFileType::ALLOW_ON_USER_GESTURE,
       "http://example.com/foo.kindabad", "", FILE_PATH_LITERAL(""),

       FILE_PATH_LITERAL("foo.kindabad"),
       DownloadItem::TARGET_DISPOSITION_OVERWRITE,

       EXPECT_UNCONFIRMED},

      {// Forced Safe
       FORCED, download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
       DownloadFileType::NOT_DANGEROUS, "http://example.com/foo.txt", "",
       FILE_PATH_LITERAL("forced-foo.txt"),

       FILE_PATH_LITERAL("forced-foo.txt"),
       DownloadItem::TARGET_DISPOSITION_OVERWRITE,

       EXPECT_LOCAL_PATH},
  };

  // The test assumes that .kindabad files have a danger level of
  // ALLOW_ON_USER_GESTURE.
  ASSERT_EQ(
      DownloadFileType::ALLOW_ON_USER_GESTURE,
      safe_browsing::FileTypePolicies::GetInstance()->GetFileDangerLevel(
          base::FilePath(FILE_PATH_LITERAL("foo.kindabad")), GURL{}, nullptr));
  RunTestCasesWithActiveItem(kBasicTestCases, std::size(kBasicTestCases));
}

TEST_F(DownloadTargetDeterminerTest, CancelSaveAs) {
  const DownloadTestCase kCancelSaveAsTestCases[] = {
      {// 0: Save_As Safe, Cancelled.
       SAVE_AS, download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
       DownloadFileType::NOT_DANGEROUS, "http://example.com/foo.txt",
       "text/plain", FILE_PATH_LITERAL(""),

       FILE_PATH_LITERAL(""), DownloadItem::TARGET_DISPOSITION_PROMPT,

       EXPECT_LOCAL_PATH}};
  ON_CALL(*delegate(), RequestConfirmation_(_, _, _, _))
      .WillByDefault(WithArg<3>(ScheduleCallback2(
          DownloadConfirmationResult::CANCELED, ui::SelectedFileInfo())));
  RunTestCasesWithActiveItem(kCancelSaveAsTestCases,
                             std::size(kCancelSaveAsTestCases));
}

// The SafeBrowsing check is performed early. Make sure that a download item
// that has been marked as DANGEROUS_URL behaves correctly.
TEST_F(DownloadTargetDeterminerTest, DangerousUrl) {
  const DownloadTestCase kSafeBrowsingTestCases[] = {
      {// 0: Automatic Dangerous URL
       AUTOMATIC, download::DOWNLOAD_DANGER_TYPE_DANGEROUS_URL,
       DownloadFileType::NOT_DANGEROUS, "http://phishing.example.com/foo.txt",
       "", FILE_PATH_LITERAL(""),

       FILE_PATH_LITERAL("foo.txt"), DownloadItem::TARGET_DISPOSITION_OVERWRITE,

       EXPECT_UNCONFIRMED},

      {// 1: Save As Dangerous URL
       SAVE_AS, download::DOWNLOAD_DANGER_TYPE_DANGEROUS_URL,
       DownloadFileType::NOT_DANGEROUS, "http://phishing.example.com/foo.txt",
       "", FILE_PATH_LITERAL(""),

       FILE_PATH_LITERAL("foo.txt"), DownloadItem::TARGET_DISPOSITION_PROMPT,

       EXPECT_UNCONFIRMED},

      {// 2: Forced Dangerous URL
       FORCED, download::DOWNLOAD_DANGER_TYPE_DANGEROUS_URL,
       DownloadFileType::NOT_DANGEROUS, "http://phishing.example.com/foo.txt",
       "", FILE_PATH_LITERAL("forced-foo.txt"),

       FILE_PATH_LITERAL("forced-foo.txt"),
       DownloadItem::TARGET_DISPOSITION_OVERWRITE,

       EXPECT_UNCONFIRMED},

      {// 3: Automatic Dangerous URL + Dangerous file. Dangerous URL takes
       // precedence.
       AUTOMATIC, download::DOWNLOAD_DANGER_TYPE_DANGEROUS_URL,
       DownloadFileType::NOT_DANGEROUS, "http://phishing.example.com/foo.html",
       "", FILE_PATH_LITERAL(""),

       FILE_PATH_LITERAL("foo.html"),
       DownloadItem::TARGET_DISPOSITION_OVERWRITE,

       EXPECT_UNCONFIRMED},

      {// 4: Save As Dangerous URL + Dangerous file
       SAVE_AS, download::DOWNLOAD_DANGER_TYPE_DANGEROUS_URL,
       DownloadFileType::NOT_DANGEROUS, "http://phishing.example.com/foo.html",
       "", FILE_PATH_LITERAL(""),

       FILE_PATH_LITERAL("foo.html"), DownloadItem::TARGET_DISPOSITION_PROMPT,

       EXPECT_UNCONFIRMED},

      {// 5: Forced Dangerous URL + Dangerous file
       FORCED, download::DOWNLOAD_DANGER_TYPE_DANGEROUS_URL,
       DownloadFileType::NOT_DANGEROUS, "http://phishing.example.com/foo.html",
       "", FILE_PATH_LITERAL("forced-foo.html"),

       FILE_PATH_LITERAL("forced-foo.html"),
       DownloadItem::TARGET_DISPOSITION_OVERWRITE,

       EXPECT_UNCONFIRMED},
  };

  ON_CALL(*delegate(), CheckDownloadUrl_(_, _, _))
      .WillByDefault(WithArg<2>(
          ScheduleCallback(download::DOWNLOAD_DANGER_TYPE_DANGEROUS_URL)));
  RunTestCasesWithActiveItem(kSafeBrowsingTestCases,
                             std::size(kSafeBrowsingTestCases));
}

// The SafeBrowsing check is performed early. Make sure that a download item
// that has been marked as MAYBE_DANGEROUS_CONTENT behaves correctly.
TEST_F(DownloadTargetDeterminerTest, MaybeDangerousContent) {
  const DownloadTestCase kSafeBrowsingTestCases[] = {
      {// 0: Automatic Maybe dangerous content
       AUTOMATIC, download::DOWNLOAD_DANGER_TYPE_MAYBE_DANGEROUS_CONTENT,
       DownloadFileType::ALLOW_ON_USER_GESTURE,
       "http://phishing.example.com/foo.kindabad", "", FILE_PATH_LITERAL(""),

       FILE_PATH_LITERAL("foo.kindabad"),
       DownloadItem::TARGET_DISPOSITION_OVERWRITE, EXPECT_UNCONFIRMED},

      {// 1: Automatic Maybe dangerous content with DANGEROUS type.
       AUTOMATIC, download::DOWNLOAD_DANGER_TYPE_MAYBE_DANGEROUS_CONTENT,
       DownloadFileType::DANGEROUS, "http://phishing.example.com/foo.bad", "",
       FILE_PATH_LITERAL(""),

       FILE_PATH_LITERAL("foo.bad"), DownloadItem::TARGET_DISPOSITION_OVERWRITE,
       EXPECT_UNCONFIRMED},

      {// 2: Save As Maybe dangerous content
       SAVE_AS, download::DOWNLOAD_DANGER_TYPE_MAYBE_DANGEROUS_CONTENT,
       DownloadFileType::NOT_DANGEROUS,
       "http://phishing.example.com/foo.kindabad", "", FILE_PATH_LITERAL(""),

       FILE_PATH_LITERAL("foo.kindabad"),
       DownloadItem::TARGET_DISPOSITION_PROMPT,

       EXPECT_UNCONFIRMED},

      {// 3: Forced Maybe dangerous content
       FORCED, download::DOWNLOAD_DANGER_TYPE_MAYBE_DANGEROUS_CONTENT,
       DownloadFileType::NOT_DANGEROUS,
       "http://phishing.example.com/foo.kindabad", "",
       FILE_PATH_LITERAL("forced-foo.kindabad"),

       FILE_PATH_LITERAL("forced-foo.kindabad"),
       DownloadItem::TARGET_DISPOSITION_OVERWRITE,

       EXPECT_UNCONFIRMED}};

  // Test assumptions:
  ASSERT_EQ(
      DownloadFileType::ALLOW_ON_USER_GESTURE,
      safe_browsing::FileTypePolicies::GetInstance()->GetFileDangerLevel(
          base::FilePath(FILE_PATH_LITERAL("foo.kindabad")), GURL{}, nullptr));
  ASSERT_EQ(DownloadFileType::DANGEROUS,
            safe_browsing::FileTypePolicies::GetInstance()->GetFileDangerLevel(
                base::FilePath(FILE_PATH_LITERAL("foo.bad")), GURL{}, nullptr));

  ON_CALL(*delegate(), CheckDownloadUrl_(_, _, _))
      .WillByDefault(WithArg<2>(ScheduleCallback(
          download::DOWNLOAD_DANGER_TYPE_MAYBE_DANGEROUS_CONTENT)));
  RunTestCasesWithActiveItem(kSafeBrowsingTestCases,
                             std::size(kSafeBrowsingTestCases));
}

// Test whether the last saved directory is used for 'Save As' downloads.
TEST_F(DownloadTargetDeterminerTest, LastSavePath) {
  const DownloadTestCase kLastSavePathTestCasesPre[] = {
      {// 0: If the last save path is empty, then the default download directory
       //    should be used.
       SAVE_AS, download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
       DownloadFileType::NOT_DANGEROUS, "http://example.com/foo.txt",
       "text/plain", FILE_PATH_LITERAL(""),

       FILE_PATH_LITERAL("foo.txt"), DownloadItem::TARGET_DISPOSITION_PROMPT,

       EXPECT_CRDOWNLOAD}};

  // These test cases are run with a last save path set to a non-emtpy local
  // download directory.
  const DownloadTestCase kLastSavePathTestCasesPost[] = {
      {// 0: This test case is run with the last download directory set to
       //    '<test_download_dir()>/foo'.
       SAVE_AS, download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
       DownloadFileType::NOT_DANGEROUS, "http://example.com/foo.txt",
       "text/plain", FILE_PATH_LITERAL(""),

       FILE_PATH_LITERAL("foo/foo.txt"),
       DownloadItem::TARGET_DISPOSITION_PROMPT,

       EXPECT_CRDOWNLOAD},

      {// 1: Start an automatic download. This should be saved to the user's
       //    default download directory and not the last used Save As directory.
       AUTOMATIC, download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
       DownloadFileType::NOT_DANGEROUS, "http://example.com/foo.txt",
       "text/plain", FILE_PATH_LITERAL(""),

       FILE_PATH_LITERAL("foo.txt"), DownloadItem::TARGET_DISPOSITION_OVERWRITE,

       EXPECT_CRDOWNLOAD},
  };

  // This test case is run with the last save path set to a non-empty virtual
  // directory.
  const DownloadTestCase kLastSavePathTestCasesVirtual[] = {
      {SAVE_AS, download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
       DownloadFileType::NOT_DANGEROUS, "http://example.com/foo.txt",
       "text/plain", FILE_PATH_LITERAL(""),

       FILE_PATH_LITERAL("bar.txt"), DownloadItem::TARGET_DISPOSITION_PROMPT,

       EXPECT_LOCAL_PATH},
  };

  {
    SCOPED_TRACE(testing::Message()
                 << "Running with default download path");
    base::FilePath prompt_path =
        GetPathInDownloadDir(FILE_PATH_LITERAL("foo.txt"));
    EXPECT_CALL(*delegate(),
                RequestConfirmation_(_, prompt_path,
                                     DownloadConfirmationReason::SAVE_AS, _));
    RunTestCasesWithActiveItem(kLastSavePathTestCasesPre,
                               std::size(kLastSavePathTestCasesPre));
  }

  // Try with a non-empty last save path.
  {
    SCOPED_TRACE(testing::Message()
                 << "Running with local last_selected_directory");
    download_prefs()->SetSaveFilePath(test_download_dir().AppendASCII("foo"));
    base::FilePath prompt_path =
        GetPathInDownloadDir(FILE_PATH_LITERAL("foo/foo.txt"));
    EXPECT_CALL(*delegate(),
                RequestConfirmation_(_, prompt_path,
                                     DownloadConfirmationReason::SAVE_AS, _));
    RunTestCasesWithActiveItem(kLastSavePathTestCasesPost,
                               std::size(kLastSavePathTestCasesPost));
  }

  // And again, but this time use a virtual directory.
  {
    SCOPED_TRACE(testing::Message()
                 << "Running with virtual last_selected_directory");
    base::FilePath last_selected_dir = test_virtual_dir().AppendASCII("foo");
    base::FilePath virtual_path = last_selected_dir.AppendASCII("foo.txt");
    download_prefs()->SetSaveFilePath(last_selected_dir);
    EXPECT_CALL(*delegate(), RequestConfirmation_(
                                 _, last_selected_dir.AppendASCII("foo.txt"),
                                 DownloadConfirmationReason::SAVE_AS, _));
    EXPECT_CALL(*delegate(), DetermineLocalPath_(_, virtual_path, _))
        .WillOnce(WithArg<2>(ScheduleCallback2(
            GetPathInDownloadDir(FILE_PATH_LITERAL("bar.txt")),
            base::FilePath())));
    RunTestCasesWithActiveItem(kLastSavePathTestCasesVirtual,
                               std::size(kLastSavePathTestCasesVirtual));
  }
}

// These tests are run with the default downloads folder set to a virtual
// directory.
TEST_F(DownloadTargetDeterminerTest, DefaultVirtual) {
  // The default download directory is the virutal path.
  download_prefs()->SetDownloadPath(test_virtual_dir());

  {
    SCOPED_TRACE(testing::Message() << "Automatic Safe Download");
    const DownloadTestCase kAutomaticDownloadToVirtualDir = {
        AUTOMATIC,
        download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
        DownloadFileType::NOT_DANGEROUS,
        "http://example.com/foo.txt",
        "text/plain",
        FILE_PATH_LITERAL(""),

        FILE_PATH_LITERAL("foo-local.txt"),
        DownloadItem::TARGET_DISPOSITION_OVERWRITE,

        EXPECT_LOCAL_PATH};
    EXPECT_CALL(*delegate(), DetermineLocalPath_(_, _, _))
        .WillOnce(WithArg<2>(ScheduleCallback2(
            GetPathInDownloadDir(FILE_PATH_LITERAL("foo-local.txt")),
            base::FilePath())));
    RunTestCasesWithActiveItem(&kAutomaticDownloadToVirtualDir, 1);
  }

  {
    SCOPED_TRACE(testing::Message() << "Save As to virtual directory");
    const DownloadTestCase kSaveAsToVirtualDir = {
        SAVE_AS,
        download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
        DownloadFileType::NOT_DANGEROUS,
        "http://example.com/bar.txt",
        "text/plain",
        FILE_PATH_LITERAL(""),

        FILE_PATH_LITERAL("foo-local.txt"),
        DownloadItem::TARGET_DISPOSITION_PROMPT,

        EXPECT_LOCAL_PATH};
    EXPECT_CALL(*delegate(), DetermineLocalPath_(_, _, _))
        .WillOnce(WithArg<2>(ScheduleCallback2(
            GetPathInDownloadDir(FILE_PATH_LITERAL("foo-local.txt")),
            base::FilePath())));
    EXPECT_CALL(*delegate(), RequestConfirmation_(
                                 _, test_virtual_dir().AppendASCII("bar.txt"),
                                 DownloadConfirmationReason::SAVE_AS, _))
        .WillOnce(WithArg<3>(ScheduleCallback2(
            DownloadConfirmationResult::CONFIRMED,
            ui::SelectedFileInfo(
                test_virtual_dir().AppendASCII("prompted.txt")))));
    RunTestCasesWithActiveItem(&kSaveAsToVirtualDir, 1);
  }

  // "Save as" is not supported on Android.
  {
    SCOPED_TRACE(testing::Message() << "Save As to local directory");
    const DownloadTestCase kSaveAsToLocalDir = {
        SAVE_AS,
        download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
        DownloadFileType::NOT_DANGEROUS,
        "http://example.com/bar.txt",
        "text/plain",
        FILE_PATH_LITERAL(""),

        FILE_PATH_LITERAL("foo-x.txt"),
        DownloadItem::TARGET_DISPOSITION_PROMPT,

        EXPECT_CRDOWNLOAD};
    EXPECT_CALL(*delegate(), RequestConfirmation_(
                                 _, test_virtual_dir().AppendASCII("bar.txt"),
                                 DownloadConfirmationReason::SAVE_AS, _))
        .WillOnce(WithArg<3>(ScheduleCallback2(
            DownloadConfirmationResult::CONFIRMED,
            ui::SelectedFileInfo(
                GetPathInDownloadDir(FILE_PATH_LITERAL("foo-x.txt"))))));
    RunTestCasesWithActiveItem(&kSaveAsToLocalDir, 1);
  }

  {
    SCOPED_TRACE(testing::Message() << "Forced safe download");
    const DownloadTestCase kForcedSafe = {
        FORCED,
        download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
        DownloadFileType::NOT_DANGEROUS,
        "http://example.com/foo.txt",
        "",
        FILE_PATH_LITERAL("forced-foo.txt"),

        FILE_PATH_LITERAL("forced-foo.txt"),
        DownloadItem::TARGET_DISPOSITION_OVERWRITE,

        EXPECT_LOCAL_PATH};
    RunTestCasesWithActiveItem(&kForcedSafe, 1);
  }
}

#if BUILDFLAG(ENABLE_PLUGINS)
TEST_F(DownloadTargetDeterminerTest,
       DetermineIfHandledSafelyHelperSynchronous) {
  const char16_t kPluginName[] = u"PDF";
  const char kPdfMimeType[] = "application/pdf";
  const char kPdfFileType[] = "pdf";
  content::WebPluginInfo plugin_info;
  plugin_info.type = content::WebPluginInfo::PLUGIN_TYPE_PEPPER_OUT_OF_PROCESS;
  plugin_info.name = kPluginName;
  plugin_info.mime_types.emplace_back(kPdfMimeType, kPdfFileType,
                                      std::string());
  content::PluginService::GetInstance()->RegisterInternalPlugin(plugin_info,
                                                                false);

  const DownloadTestCase download_test_case = {
      AUTOMATIC,
      download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
      DownloadFileType::NOT_DANGEROUS,
      "http://example.com/foo.txt",
      kPdfMimeType,
      FILE_PATH_LITERAL(""),
      FILE_PATH_LITERAL("foo.txt"),
      DownloadItem::TARGET_DISPOSITION_OVERWRITE,
      EXPECT_CRDOWNLOAD};
  std::unique_ptr<download::MockDownloadItem> item =
      CreateActiveDownloadItem(1, download_test_case);

  EXPECT_TRUE(
      DownloadTargetDeterminer::DetermineIfHandledSafelyHelperSynchronous(
          item.get(), base::FilePath(), kPdfMimeType));
}
#endif

// Test that an inactive download will still get a virtual or local download
// path.
TEST_F(DownloadTargetDeterminerTest, InactiveDownload) {
  const DownloadTestCase kBaseTestCase = {
      AUTOMATIC,
      download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
      DownloadFileType::NOT_DANGEROUS,
      "http://example.com/foo.txt",
      "text/plain",
      FILE_PATH_LITERAL(""),
      FILE_PATH_LITERAL("foo.txt"),
      DownloadItem::TARGET_DISPOSITION_OVERWRITE,
      EXPECT_CRDOWNLOAD};

  const struct {
    TestCaseType type;
    DownloadItem::TargetDisposition disposition;
  } kTestCases[] = {{AUTOMATIC, DownloadItem::TARGET_DISPOSITION_OVERWRITE},
                    {SAVE_AS, DownloadItem::TARGET_DISPOSITION_PROMPT}};

  for (const auto& test_case : kTestCases) {
    DownloadTestCase download_test_case = kBaseTestCase;
    download_test_case.test_type = test_case.type;
    download_test_case.expected_disposition = test_case.disposition;
    std::unique_ptr<download::MockDownloadItem> item =
        CreateActiveDownloadItem(1, download_test_case);
    EXPECT_CALL(*item.get(), GetState())
        .WillRepeatedly(Return(download::DownloadItem::CANCELLED));

    EXPECT_CALL(*delegate(), RequestConfirmation_(_, _, _, _)).Times(0);
    EXPECT_CALL(*delegate(), NotifyExtensions_(_, _, _)).Times(0);
    EXPECT_CALL(*delegate(), ReserveVirtualPath_(_, _, _, _, _)).Times(0);
    EXPECT_CALL(*delegate(), DetermineLocalPath_(_, _, _)).Times(1);

    // Each test case has a non-empty target path. The test will fail if the
    // target determination doesn't result in that path.
    RunTestCase(download_test_case, base::FilePath(), item.get());
  }
}

// If the reserved path could not be verified, then the user should see a
// prompt.
TEST_F(DownloadTargetDeterminerTest, ReservationFailed_Confirmation) {
  DownloadTestCase download_test_case = {
      // 0: Automatic download. Since the reservation fails, the disposition of
      // the target is to prompt, but the returned path is used.
      AUTOMATIC,
      download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
      DownloadFileType::NOT_DANGEROUS,
      "http://example.com/foo.txt",
      "text/plain",
      FILE_PATH_LITERAL(""),

      FILE_PATH_LITERAL("bar.txt"),
      DownloadItem::TARGET_DISPOSITION_PROMPT,

      EXPECT_CRDOWNLOAD};

  struct TestCase {
    download::PathValidationResult result;
    DownloadConfirmationReason expected_confirmation_reason;
  } kTestCases[] = {{download::PathValidationResult::SUCCESS,
                     DownloadConfirmationReason::NONE},
                    {download::PathValidationResult::SUCCESS_RESOLVED_CONFLICT,
                     DownloadConfirmationReason::NONE},
                    {download::PathValidationResult::CONFLICT,
                     DownloadConfirmationReason::TARGET_CONFLICT},
                    {download::PathValidationResult::PATH_NOT_WRITABLE,
                     DownloadConfirmationReason::TARGET_PATH_NOT_WRITEABLE},
                    {download::PathValidationResult::NAME_TOO_LONG,
                     DownloadConfirmationReason::NAME_TOO_LONG}};

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(::testing::Message() << "download::PathValidationResult "
                                      << static_cast<int>(test_case.result));
    ON_CALL(*delegate(), ReserveVirtualPath_(_, _, _, _, _))
        .WillByDefault(WithArg<4>(ScheduleCallback2(
            test_case.result,
            GetPathInDownloadDir(FILE_PATH_LITERAL("bar.txt")))));
    if (test_case.expected_confirmation_reason !=
        DownloadConfirmationReason::NONE) {
      EXPECT_CALL(*delegate(),
                  RequestConfirmation_(
                      _, _, test_case.expected_confirmation_reason, _));
    } else {
      EXPECT_CALL(*delegate(), RequestConfirmation_(_, _, _, _)).Times(0);
    }

    download_test_case.expected_disposition =
        test_case.expected_confirmation_reason ==
                DownloadConfirmationReason::NONE
            ? download::DownloadItem::TARGET_DISPOSITION_OVERWRITE
            : download::DownloadItem::TARGET_DISPOSITION_PROMPT;
    std::unique_ptr<download::MockDownloadItem> item = CreateActiveDownloadItem(
        static_cast<int>(test_case.result), download_test_case);
    RunTestCase(download_test_case, base::FilePath(), item.get());
  }
}

// If the local path could not be determined, the download should be cancelled.
TEST_F(DownloadTargetDeterminerTest, LocalPathFailed) {
  const DownloadTestCase kLocalPathFailedCases[] = {
      {// 0: Automatic download.
       AUTOMATIC, download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
       DownloadFileType::NOT_DANGEROUS, "http://example.com/foo.txt",
       "text/plain", FILE_PATH_LITERAL(""),

       FILE_PATH_LITERAL(""), DownloadItem::TARGET_DISPOSITION_OVERWRITE,

       EXPECT_LOCAL_PATH},
  };

  // The default download directory is the virtual path.
  download_prefs()->SetDownloadPath(test_virtual_dir());
  // Simulate failed call to DetermineLocalPath.
  EXPECT_CALL(
      *delegate(),
      DetermineLocalPath_(
          _, GetPathInDownloadDir(FILE_PATH_LITERAL("virtual/foo.txt")), _))
      .WillOnce(
          WithArg<2>(ScheduleCallback2(base::FilePath(), base::FilePath())));
  RunTestCasesWithActiveItem(kLocalPathFailedCases,
                             std::size(kLocalPathFailedCases));
}

// Downloads that have a danger level of ALLOW_ON_USER_GESTURE should be marked
// as safe depending on whether there was a user gesture associated with the
// download and whether the referrer was visited prior to today.
TEST_F(DownloadTargetDeterminerTest, VisitedReferrer) {
  const DownloadTestCase kVisitedReferrerCases[] = {
      // http://visited.example.com/ is added to the history as a visit that
      // happened prior to today.
      {// 0: Safe download due to visiting referrer before.
       AUTOMATIC, download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
       DownloadFileType::NOT_DANGEROUS,
       "http://visited.example.com/foo.kindabad", "", FILE_PATH_LITERAL(""),

       FILE_PATH_LITERAL("foo.kindabad"),
       DownloadItem::TARGET_DISPOSITION_OVERWRITE,

       EXPECT_CRDOWNLOAD},

      {// 1: Dangerous due to not having visited referrer before.
       AUTOMATIC, download::DOWNLOAD_DANGER_TYPE_DANGEROUS_FILE,
       DownloadFileType::ALLOW_ON_USER_GESTURE,
       "http://not-visited.example.com/foo.kindabad", "", FILE_PATH_LITERAL(""),

       FILE_PATH_LITERAL("foo.kindabad"),
       DownloadItem::TARGET_DISPOSITION_OVERWRITE,

       EXPECT_UNCONFIRMED},

      {// 2: Safe because the user is being prompted.
       SAVE_AS, download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
       DownloadFileType::NOT_DANGEROUS,
       "http://not-visited.example.com/foo.kindabad", "", FILE_PATH_LITERAL(""),

       FILE_PATH_LITERAL("foo.kindabad"),
       DownloadItem::TARGET_DISPOSITION_PROMPT,

       EXPECT_CRDOWNLOAD},

      {// 3: Safe because of forced path.
       FORCED, download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
       DownloadFileType::NOT_DANGEROUS,
       "http://not-visited.example.com/foo.kindabad", "application/xml",
       FILE_PATH_LITERAL("foo.kindabad"),

       FILE_PATH_LITERAL("foo.kindabad"),
       DownloadItem::TARGET_DISPOSITION_OVERWRITE,

       EXPECT_LOCAL_PATH},
  };

  // This test assumes that the danger level of .kindabad files is
  // ALLOW_ON_USER_GESTURE.
  ASSERT_EQ(
      DownloadFileType::ALLOW_ON_USER_GESTURE,
      safe_browsing::FileTypePolicies::GetInstance()->GetFileDangerLevel(
          base::FilePath(FILE_PATH_LITERAL("foo.kindabad")), GURL{}, nullptr));

  GURL url("http://visited.example.com/visited-link.html");
  // The time of visit is picked to be several seconds prior to the most recent
  // midnight.
  base::Time time_of_visit(base::Time::Now().LocalMidnight() -
                           base::Seconds(10));
  history::HistoryService* history_service =
      HistoryServiceFactory::GetForProfile(profile(),
                                           ServiceAccessType::EXPLICIT_ACCESS);
  ASSERT_TRUE(history_service);
  history_service->AddPage(url, time_of_visit, history::SOURCE_BROWSED);

  RunTestCasesWithActiveItem(kVisitedReferrerCases,
                             std::size(kVisitedReferrerCases));
}

TEST_F(DownloadTargetDeterminerTest, TransitionType) {
  const DownloadTestCase kSafeFile = {
      AUTOMATIC,
      download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
      DownloadFileType::NOT_DANGEROUS,
      "http://example.com/foo.txt",
      "text/plain",
      FILE_PATH_LITERAL(""),

      FILE_PATH_LITERAL("foo.txt"),
      DownloadItem::TARGET_DISPOSITION_OVERWRITE,

      EXPECT_CRDOWNLOAD};

  const DownloadTestCase kAllowOnUserGesture = {
      AUTOMATIC,
      download::DOWNLOAD_DANGER_TYPE_DANGEROUS_FILE,
      DownloadFileType::ALLOW_ON_USER_GESTURE,
      "http://example.com/foo.kindabad",
      "application/octet-stream",
      FILE_PATH_LITERAL(""),

      FILE_PATH_LITERAL("foo.kindabad"),
      DownloadItem::TARGET_DISPOSITION_OVERWRITE,

      EXPECT_UNCONFIRMED};

  const DownloadTestCase kDangerousFile = {
      AUTOMATIC,
      download::DOWNLOAD_DANGER_TYPE_DANGEROUS_FILE,
      DownloadFileType::DANGEROUS,
      "http://example.com/foo.bad",
      "application/octet-stream",
      FILE_PATH_LITERAL(""),

      FILE_PATH_LITERAL("foo.bad"),
      DownloadItem::TARGET_DISPOSITION_OVERWRITE,

      EXPECT_UNCONFIRMED};

  const struct {
    ui::PageTransition page_transition;
    download::DownloadDangerType expected_danger_type;
    const raw_ref<const DownloadTestCase> template_download_test_case;
  } kTestCases[] = {
      {// Benign file type. Results in a danger type of NOT_DANGEROUS. Page
       // transition type is irrelevant.
       ui::PAGE_TRANSITION_LINK, download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
       ToRawRef(kSafeFile)},

      {// File type is ALLOW_ON_USER_GESTURE. PAGE_TRANSITION_LINK doesn't
       // cause file to be marked as safe.
       ui::PAGE_TRANSITION_LINK, download::DOWNLOAD_DANGER_TYPE_DANGEROUS_FILE,
       ToRawRef(kAllowOnUserGesture)},

      {// File type is ALLOW_ON_USER_GESTURE. PAGE_TRANSITION_TYPED doesn't
       // cause file to be marked as safe. TYPED can be used for certain
       // types of explicit page transitions that aren't necessarily
       // initiated by a user. Hence a resulting download may not be
       // intentional.
       ui::PAGE_TRANSITION_TYPED, download::DOWNLOAD_DANGER_TYPE_DANGEROUS_FILE,
       ToRawRef(kAllowOnUserGesture)},

      {// File type is ALLOW_ON_USER_GESTURE.
       // PAGE_TRANSITION_FROM_ADDRESS_BAR causes file to be marked as safe.
       static_cast<ui::PageTransition>(ui::PAGE_TRANSITION_TYPED |
                                       ui::PAGE_TRANSITION_FROM_ADDRESS_BAR),
       download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
       ToRawRef(kAllowOnUserGesture)},

      {// File type is ALLOW_ON_USER_GESTURE.
       // PAGE_TRANSITION_FROM_ADDRESS_BAR causes file to be marked as safe.
       static_cast<ui::PageTransition>(ui::PAGE_TRANSITION_GENERATED |
                                       ui::PAGE_TRANSITION_FROM_ADDRESS_BAR),
       download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
       ToRawRef(kAllowOnUserGesture)},

      {// File type is ALLOW_ON_USER_GESTURE.
       // PAGE_TRANSITION_FROM_ADDRESS_BAR causes file to be marked as safe.
       ui::PAGE_TRANSITION_FROM_ADDRESS_BAR,
       download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
       ToRawRef(kAllowOnUserGesture)},

      {// File type is DANGEROUS. PageTransition is irrelevant.
       static_cast<ui::PageTransition>(ui::PAGE_TRANSITION_TYPED |
                                       ui::PAGE_TRANSITION_FROM_ADDRESS_BAR),
       download::DOWNLOAD_DANGER_TYPE_DANGEROUS_FILE, ToRawRef(kDangerousFile)},
  };

  // Test assumptions:
  ASSERT_EQ(
      DownloadFileType::ALLOW_ON_USER_GESTURE,
      safe_browsing::FileTypePolicies::GetInstance()->GetFileDangerLevel(
          base::FilePath(FILE_PATH_LITERAL("foo.kindabad")), GURL{}, nullptr));
  ASSERT_EQ(DownloadFileType::DANGEROUS,
            safe_browsing::FileTypePolicies::GetInstance()->GetFileDangerLevel(
                base::FilePath(FILE_PATH_LITERAL("foo.bad")), GURL{}, nullptr));
  ASSERT_EQ(DownloadFileType::NOT_DANGEROUS,
            safe_browsing::FileTypePolicies::GetInstance()->GetFileDangerLevel(
                base::FilePath(FILE_PATH_LITERAL("foo.txt")), GURL{}, nullptr));

  for (const auto& test_case : kTestCases) {
    // The template download test case describes what to expect if the page
    // transition was LINK. If the expectation is that the page transition type
    // causes the download to be considered safe, then download_test_case needs
    // to be adjusted accordingly.
    DownloadTestCase download_test_case =
        *test_case.template_download_test_case;
    download_test_case.expected_danger_type = test_case.expected_danger_type;
    if (test_case.expected_danger_type ==
        download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS) {
      download_test_case.expected_danger_level =
          DownloadFileType::NOT_DANGEROUS;
      download_test_case.expected_intermediate = EXPECT_CRDOWNLOAD;
    }

    std::unique_ptr<download::MockDownloadItem> item =
        CreateActiveDownloadItem(1, download_test_case);
    EXPECT_CALL(*item, GetTransitionType())
        .WillRepeatedly(Return(test_case.page_transition));
    RunTestCase(download_test_case, base::FilePath(), item.get());
  }
}

// These test cases are run with "Prompt for download" user preference set to
// true.
TEST_F(DownloadTargetDeterminerTest, PromptAlways_SafeAutomatic) {
  const DownloadTestCase kSafeAutomatic = {
      // 0: Safe Automatic - Should prompt because of "Prompt for download"
      //    preference setting.
      AUTOMATIC,
      download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
      DownloadFileType::NOT_DANGEROUS,
      "http://example.com/automatic.txt",
      "text/plain",
      FILE_PATH_LITERAL(""),

      FILE_PATH_LITERAL("automatic.txt"),
      DownloadItem::TARGET_DISPOSITION_PROMPT,

      EXPECT_CRDOWNLOAD};

  SetPromptForDownload(true);
  EXPECT_CALL(*delegate(),
              RequestConfirmation_(
                  _, GetPathInDownloadDir(FILE_PATH_LITERAL("automatic.txt")),
                  DownloadConfirmationReason::PREFERENCE, _));
  RunTestCasesWithActiveItem(&kSafeAutomatic, 1);
}

TEST_F(DownloadTargetDeterminerTest, PromptAlways_SafeSaveAs) {
  const DownloadTestCase kSafeSaveAs = {
      // 1: Safe Save As - Should prompt because of "Save as" invocation.
      SAVE_AS,
      download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
      DownloadFileType::NOT_DANGEROUS,
      "http://example.com/save-as.txt",
      "text/plain",
      FILE_PATH_LITERAL(""),

      FILE_PATH_LITERAL("save-as.txt"),
      DownloadItem::TARGET_DISPOSITION_PROMPT,

      EXPECT_CRDOWNLOAD};

  SetPromptForDownload(true);
  EXPECT_CALL(*delegate(),
              RequestConfirmation_(
                  _, GetPathInDownloadDir(FILE_PATH_LITERAL("save-as.txt")),
                  DownloadConfirmationReason::SAVE_AS, _));
  RunTestCasesWithActiveItem(&kSafeSaveAs, 1);
}

TEST_F(DownloadTargetDeterminerTest, PromptAlways_SafeForced) {
  const DownloadTestCase kSafeForced = {
      // 2: Safe Forced - Shouldn't prompt.
      FORCED,
      download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
      DownloadFileType::NOT_DANGEROUS,
      "http://example.com/foo.txt",
      "text/plain",
      FILE_PATH_LITERAL("foo.txt"),

      FILE_PATH_LITERAL("foo.txt"),
      DownloadItem::TARGET_DISPOSITION_OVERWRITE,

      EXPECT_LOCAL_PATH};

  SetPromptForDownload(true);
  RunTestCasesWithActiveItem(&kSafeForced, 1);
}

TEST_F(DownloadTargetDeterminerTest, PromptAlways_AutoOpen) {
  const DownloadTestCase kAutoOpen = {
      // 3: Automatic - The filename extension is marked as one that we will
      //    open automatically. Shouldn't prompt.
      AUTOMATIC,
      download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
      DownloadFileType::NOT_DANGEROUS,
      "http://example.com/foo.dummy",
      "",
      FILE_PATH_LITERAL(""),

      FILE_PATH_LITERAL("foo.dummy"),
      DownloadItem::TARGET_DISPOSITION_OVERWRITE,

      EXPECT_CRDOWNLOAD};
  SetPromptForDownload(true);
  EnableAutoOpenByUserBasedOnExtension(
      base::FilePath(FILE_PATH_LITERAL("dummy.dummy")));
  RunTestCasesWithActiveItem(&kAutoOpen, 1);
}

// If an embedder responds to a RequestConfirmation with a new path and a
// CONTINUE_WITHOUT_CONFIRMATION, then we shouldn't consider the file as safe.
TEST_F(DownloadTargetDeterminerTest, ContinueWithoutConfirmation_SaveAs) {
  const DownloadTestCase kTestCase = {
      SAVE_AS,
      download::DOWNLOAD_DANGER_TYPE_DANGEROUS_FILE,
      DownloadFileType::ALLOW_ON_USER_GESTURE,
      "http://example.com/save-as.kindabad",
      "",
      FILE_PATH_LITERAL(""),
      FILE_PATH_LITERAL("foo.kindabad"),
      DownloadItem::TARGET_DISPOSITION_OVERWRITE,
      EXPECT_UNCONFIRMED};

  EXPECT_CALL(
      *delegate(),
      RequestConfirmation_(
          _, GetPathInDownloadDir(FILE_PATH_LITERAL("save-as.kindabad")),
          DownloadConfirmationReason::SAVE_AS, _))
      .WillOnce(WithArg<3>(ScheduleCallback2(
          DownloadConfirmationResult::CONTINUE_WITHOUT_CONFIRMATION,
          ui::SelectedFileInfo(
              GetPathInDownloadDir(FILE_PATH_LITERAL("foo.kindabad"))))));
  RunTestCasesWithActiveItem(&kTestCase, 1);
}

// Same as ContinueWithoutConfirmation_SaveAs, but the embedder response
// indicates that the user confirmed the path. Hence the danger level of the
// download and the disposition should be updated accordingly.
TEST_F(DownloadTargetDeterminerTest, ContinueWithConfirmation_SaveAs) {
  const DownloadTestCase kTestCase = {
      SAVE_AS,
      download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
      DownloadFileType::NOT_DANGEROUS,
      "http://example.com/save-as.kindabad",
      "",
      FILE_PATH_LITERAL(""),
      FILE_PATH_LITERAL("foo.kindabad"),
      DownloadItem::TARGET_DISPOSITION_PROMPT,
      EXPECT_CRDOWNLOAD};

  EXPECT_CALL(
      *delegate(),
      RequestConfirmation_(
          _, GetPathInDownloadDir(FILE_PATH_LITERAL("save-as.kindabad")),
          DownloadConfirmationReason::SAVE_AS, _))
      .WillOnce(WithArg<3>(ScheduleCallback2(
          DownloadConfirmationResult::CONFIRMED,
          ui::SelectedFileInfo(
              GetPathInDownloadDir(FILE_PATH_LITERAL("foo.kindabad"))))));
  RunTestCasesWithActiveItem(&kTestCase, 1);
}

#if BUILDFLAG(ENABLE_EXTENSIONS)
// These test cases are run with "Prompt for download" user preference set to
// true. For non-trusted extensions, download should cause prompting.
// Android doesn't support extensions.
TEST_F(DownloadTargetDeterminerTest, PromptAlways_NonTrustedExtension) {
  const DownloadTestCase kPromptingTestCases[] = {
      {// 0: Automatic Browser Extension download. - Shouldn't prompt for
       //    browser extension downloads even if "Prompt for download"
       //    preference is set.
       AUTOMATIC, download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
       DownloadFileType::NOT_DANGEROUS, "http://example.com/foo.kindabad",
       extensions::Extension::kMimeType, FILE_PATH_LITERAL(""),

       FILE_PATH_LITERAL("foo.crx"), DownloadItem::TARGET_DISPOSITION_PROMPT,

       EXPECT_CRDOWNLOAD},

      {// 1: Automatic User Script - Shouldn't prompt for user script downloads
       //    even if "Prompt for download" preference is set.
       AUTOMATIC, download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
       DownloadFileType::NOT_DANGEROUS, "http://example.com/foo.user.js", "",
       FILE_PATH_LITERAL(""),

       FILE_PATH_LITERAL("foo.user.js"),
       DownloadItem::TARGET_DISPOSITION_PROMPT,

       EXPECT_CRDOWNLOAD},
  };

  SetPromptForDownload(true);
  RunTestCasesWithActiveItem(kPromptingTestCases,
                             std::size(kPromptingTestCases));
}

// Trusted extension download should not cause prompting.
TEST_F(DownloadTargetDeterminerTest, PromptAlways_TrustedExtension) {
  const DownloadTestCase kPromptingTestCases[] = {
      {// 0: Automatic Browser Extension download. - Shouldn't prompt for
       //    browser extension downloads even if "Prompt for download"
       //    preference is set.
       AUTOMATIC, download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
       DownloadFileType::NOT_DANGEROUS, "http://example.com/foo.kindabad",
       extensions::Extension::kMimeType, FILE_PATH_LITERAL(""),

       FILE_PATH_LITERAL("foo.crx"), DownloadItem::TARGET_DISPOSITION_OVERWRITE,

       EXPECT_CRDOWNLOAD},

      {// 1: Automatic User Script - Shouldn't prompt for user script downloads
       //    even if "Prompt for download" preference is set.
       AUTOMATIC, download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
       DownloadFileType::NOT_DANGEROUS, "http://example.com/foo.user.js", "",
       FILE_PATH_LITERAL(""),

       FILE_PATH_LITERAL("foo.user.js"),
       DownloadItem::TARGET_DISPOSITION_OVERWRITE,

       EXPECT_CRDOWNLOAD},
  };

  auto allow_offstore_install =
      download_crx_util::OverrideOffstoreInstallAllowedForTesting(true);
  SetPromptForDownload(true);
  RunTestCasesWithActiveItem(kPromptingTestCases,
                             std::size(kPromptingTestCases));
}
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

// If the download path is managed, then we don't show any prompts.
// Note that if the download path is managed, then PromptForDownload() is false.
TEST_F(DownloadTargetDeterminerTest, ManagedPath) {
  const DownloadTestCase kManagedPathTestCases[] = {
      {// 0: Automatic Safe
       AUTOMATIC, download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
       DownloadFileType::NOT_DANGEROUS, "http://example.com/foo.txt",
       "text/plain", FILE_PATH_LITERAL(""),

       FILE_PATH_LITERAL("foo.txt"), DownloadItem::TARGET_DISPOSITION_OVERWRITE,

       EXPECT_CRDOWNLOAD},

      {// 1: Save_As Safe
       SAVE_AS, download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
       DownloadFileType::NOT_DANGEROUS, "http://example.com/foo.txt",
       "text/plain", FILE_PATH_LITERAL(""),

       FILE_PATH_LITERAL("foo.txt"), DownloadItem::TARGET_DISPOSITION_OVERWRITE,

       EXPECT_CRDOWNLOAD},
  };

  SetManagedDownloadPath(test_download_dir());
  ASSERT_TRUE(download_prefs()->IsDownloadPathManaged());
  RunTestCasesWithActiveItem(kManagedPathTestCases,
                             std::size(kManagedPathTestCases));
}

// Test basic blocking functionality via GetInsecureDownloadStatus.
TEST_F(DownloadTargetDeterminerTest, BlockDownloads) {
  const DownloadTestCase kBlockDownloadsTestCases[] = {
      {AUTOMATIC, download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
       DownloadFileType::NOT_DANGEROUS, "http://example.com/foo.txt", "",
       FILE_PATH_LITERAL(""), FILE_PATH_LITERAL(""),
       DownloadItem::TARGET_DISPOSITION_OVERWRITE, EXPECT_EMPTY},
  };

  ON_CALL(*delegate(), GetInsecureDownloadStatus_(_, _, _))
      .WillByDefault(WithArg<2>(ScheduleCallback(
          download::DownloadItem::InsecureDownloadStatus::SILENT_BLOCK)));
  RunTestCasesWithActiveItem(kBlockDownloadsTestCases,
                             std::size(kBlockDownloadsTestCases));
}

// Test basic functionality supporting extensions that want to override download
// filenames.
TEST_F(DownloadTargetDeterminerTest, NotifyExtensionsSafe) {
  const DownloadTestCase kNotifyExtensionsTestCases[] = {
      {// 0: Automatic Safe
       AUTOMATIC, download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
       DownloadFileType::NOT_DANGEROUS, "http://example.com/foo.txt",
       "text/plain", FILE_PATH_LITERAL(""),

       FILE_PATH_LITERAL("overridden/foo.txt"),
       DownloadItem::TARGET_DISPOSITION_OVERWRITE,

       EXPECT_CRDOWNLOAD},

      {// 1: Save_As Safe
       SAVE_AS, download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
       DownloadFileType::NOT_DANGEROUS, "http://example.com/foo.txt",
       "text/plain", FILE_PATH_LITERAL(""),

       FILE_PATH_LITERAL("overridden/foo.txt"),
       DownloadItem::TARGET_DISPOSITION_PROMPT,

       EXPECT_CRDOWNLOAD},

      {// 2: Automatic Dangerous
       AUTOMATIC, download::DOWNLOAD_DANGER_TYPE_DANGEROUS_FILE,
       DownloadFileType::ALLOW_ON_USER_GESTURE,
       "http://example.com/foo.kindabad", "", FILE_PATH_LITERAL(""),

       FILE_PATH_LITERAL("overridden/foo.kindabad"),
       DownloadItem::TARGET_DISPOSITION_OVERWRITE,

       EXPECT_UNCONFIRMED},

      {// 3: Forced Safe
       FORCED, download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
       DownloadFileType::NOT_DANGEROUS, "http://example.com/foo.txt", "",
       FILE_PATH_LITERAL("forced-foo.txt"),

       FILE_PATH_LITERAL("forced-foo.txt"),
       DownloadItem::TARGET_DISPOSITION_OVERWRITE,

       EXPECT_LOCAL_PATH},

      {// 4: Use a file extension that doesn't match the MIME type, but matches
       // the URL.
       AUTOMATIC, download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
       DownloadFileType::NOT_DANGEROUS, "http://example.com/foo.xyz",
       "text/plain", FILE_PATH_LITERAL(""),

       FILE_PATH_LITERAL("overridden/foo.xyz"),
       DownloadItem::TARGET_DISPOSITION_OVERWRITE,

       EXPECT_CRDOWNLOAD},
  };

  ON_CALL(*delegate(), NotifyExtensions_(_, _, _))
      .WillByDefault(Invoke(&NotifyExtensionsOverridePath));
  RunTestCasesWithActiveItem(kNotifyExtensionsTestCases,
                             std::size(kNotifyExtensionsTestCases));
}

// Test that filenames provided by extensions are passed into SafeBrowsing
// checks and dangerous download checks.
TEST_F(DownloadTargetDeterminerTest, NotifyExtensionsUnsafe) {
  const DownloadTestCase kNotHandledBySafeBrowsing = {
      AUTOMATIC,
      download::DOWNLOAD_DANGER_TYPE_DANGEROUS_FILE,
      DownloadFileType::ALLOW_ON_USER_GESTURE,
      "http://example.com/foo.kindabad.remove",
      "",
      FILE_PATH_LITERAL(""),

      FILE_PATH_LITERAL("overridden/foo.kindabad"),
      DownloadItem::TARGET_DISPOSITION_OVERWRITE,

      EXPECT_UNCONFIRMED};

  const DownloadTestCase kHandledBySafeBrowsing = {
      AUTOMATIC,
      download::DOWNLOAD_DANGER_TYPE_MAYBE_DANGEROUS_CONTENT,
      DownloadFileType::ALLOW_ON_USER_GESTURE,
      "http://example.com/foo.kindabad.remove",
      "",
      FILE_PATH_LITERAL(""),

      FILE_PATH_LITERAL("overridden/foo.kindabad"),
      DownloadItem::TARGET_DISPOSITION_OVERWRITE,

      EXPECT_UNCONFIRMED};

  ON_CALL(*delegate(), NotifyExtensions_(_, _, _))
      .WillByDefault(Invoke(&NotifyExtensionsOverridePath));
  RunTestCasesWithActiveItem(&kNotHandledBySafeBrowsing, 1);

  ON_CALL(*delegate(), CheckDownloadUrl_(_, _, _))
      .WillByDefault(WithArg<2>(ScheduleCallback(
          download::DOWNLOAD_DANGER_TYPE_MAYBE_DANGEROUS_CONTENT)));
  RunTestCasesWithActiveItem(&kHandledBySafeBrowsing, 1);
}

// Test that conflict actions set by extensions are passed correctly into
// ReserveVirtualPath.
TEST_F(DownloadTargetDeterminerTest, NotifyExtensionsConflict) {
  const DownloadTestCase kNotifyExtensionsTestCase = {
      AUTOMATIC,
      download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
      DownloadFileType::NOT_DANGEROUS,
      "http://example.com/foo.txt",
      "text/plain",
      FILE_PATH_LITERAL(""),

      FILE_PATH_LITERAL("overridden/foo.txt"),
      DownloadItem::TARGET_DISPOSITION_OVERWRITE,

      EXPECT_CRDOWNLOAD};

  const DownloadTestCase& test_case = kNotifyExtensionsTestCase;
  std::unique_ptr<download::MockDownloadItem> item =
      CreateActiveDownloadItem(0, test_case);
  base::FilePath overridden_path(FILE_PATH_LITERAL("overridden/foo.txt"));
  base::FilePath full_overridden_path =
      GetPathInDownloadDir(overridden_path.value());

  // First case: An extension sets the conflict_action to OVERWRITE.
  EXPECT_CALL(*delegate(), NotifyExtensions_(_, _, _))
      .WillOnce(WithArg<2>(ScheduleCallback2(
          overridden_path, DownloadPathReservationTracker::OVERWRITE)));
  EXPECT_CALL(*delegate(),
              ReserveVirtualPath_(_, full_overridden_path, true,
                                  DownloadPathReservationTracker::OVERWRITE, _))
      .WillOnce(WithArg<4>(ScheduleCallback2(
          download::PathValidationResult::SUCCESS, full_overridden_path)));

  RunTestCase(test_case, base::FilePath(), item.get());

  // Second case: An extension sets the conflict_action to PROMPT.
  EXPECT_CALL(*delegate(), NotifyExtensions_(_, _, _))
      .WillOnce(WithArg<2>(ScheduleCallback2(
          overridden_path, DownloadPathReservationTracker::PROMPT)));
  EXPECT_CALL(*delegate(),
              ReserveVirtualPath_(_, full_overridden_path, true,
                                  DownloadPathReservationTracker::PROMPT, _))
      .WillOnce(WithArg<4>(ScheduleCallback2(
          download::PathValidationResult::SUCCESS, full_overridden_path)));
  RunTestCase(test_case, base::FilePath(), item.get());
}

// Test that relative paths returned by extensions are always relative to the
// default downloads path.
TEST_F(DownloadTargetDeterminerTest, NotifyExtensionsDefaultPath) {
  const DownloadTestCase kNotifyExtensionsTestCase = {
      AUTOMATIC,
      download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
      DownloadFileType::NOT_DANGEROUS,
      "http://example.com/foo.txt",
      "text/plain",
      FILE_PATH_LITERAL(""),

      FILE_PATH_LITERAL("overridden/foo.txt"),
      DownloadItem::TARGET_DISPOSITION_OVERWRITE,

      EXPECT_CRDOWNLOAD};

  const DownloadTestCase& test_case = kNotifyExtensionsTestCase;
  std::unique_ptr<download::MockDownloadItem> item =
      CreateActiveDownloadItem(0, test_case);
  base::FilePath overridden_path(FILE_PATH_LITERAL("overridden/foo.txt"));
  base::FilePath full_overridden_path =
      GetPathInDownloadDir(overridden_path.value());

  download_prefs()->SetSaveFilePath(GetPathInDownloadDir(
      FILE_PATH_LITERAL("last_selected")));

  EXPECT_CALL(*delegate(), NotifyExtensions_(_, _, _))
      .WillOnce(WithArg<2>(ScheduleCallback2(
          overridden_path, DownloadPathReservationTracker::UNIQUIFY)));
  RunTestCase(test_case, base::FilePath(), item.get());
}

// Test that relative paths returned by extensions are always relative to the
// default downloads path.
TEST_F(DownloadTargetDeterminerTest, NotifyExtensionsLocalFile) {
  const DownloadTestCase kNotifyExtensionsTestCases[] = {
      {AUTOMATIC, download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
       DownloadFileType::NOT_DANGEROUS,
#if BUILDFLAG(IS_WIN)
       "file:///usr/local/xyz",
#else
       "file:///c:/usr/local/xyz",
#endif  // BUILDFLAG(IS_WIN)
       "text/plain", FILE_PATH_LITERAL(""),

       FILE_PATH_LITERAL("xyz.txt"), DownloadItem::TARGET_DISPOSITION_OVERWRITE,

       EXPECT_CRDOWNLOAD}};

  base::FilePath overridden_path(FILE_PATH_LITERAL("overridden/foo.txt"));
  EXPECT_CALL(*delegate(), NotifyExtensions_(_, _, _))
      .WillRepeatedly(WithArg<2>(ScheduleCallback2(
          overridden_path, DownloadPathReservationTracker::UNIQUIFY)));
  RunTestCasesWithActiveItem(kNotifyExtensionsTestCases,
                             std::size(kNotifyExtensionsTestCases));
}

TEST_F(DownloadTargetDeterminerTest, InitialVirtualPathUnsafe) {
  const base::FilePath::CharType* kInitialPath =
      FILE_PATH_LITERAL("some_path/bar.html");

  const DownloadTestCase kInitialPathTestCase = {
      // 0: Save As Save. The path generated based on the DownloadItem is safe,
      // but the initial path is unsafe. However, the download is not considered
      // dangerous since the user has been prompted.
      SAVE_AS,
      download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
      DownloadFileType::NOT_DANGEROUS,
      "http://example.com/foo.txt",
      "text/plain",
      FILE_PATH_LITERAL(""),

      kInitialPath,
      DownloadItem::TARGET_DISPOSITION_PROMPT,

      EXPECT_CRDOWNLOAD};

  const DownloadTestCase& test_case = kInitialPathTestCase;
  std::unique_ptr<download::MockDownloadItem> item =
      CreateActiveDownloadItem(1, test_case);
  EXPECT_CALL(*item, GetLastReason())
      .WillRepeatedly(
          Return(download::DOWNLOAD_INTERRUPT_REASON_NETWORK_FAILED));
  EXPECT_CALL(*item, GetTargetDisposition())
      .WillRepeatedly(Return(DownloadItem::TARGET_DISPOSITION_PROMPT));
  RunTestCase(test_case, GetPathInDownloadDir(kInitialPath), item.get());
}

// Prompting behavior for resumed downloads is based on the last interrupt
// reason. If the reason indicates that the target path may not be suitable for
// the download (ACCESS_DENIED, NO_SPACE, etc..), then the user should be
// prompted, and not otherwise. These test cases shouldn't result in prompting
// since the error is set to NETWORK_FAILED.
TEST_F(DownloadTargetDeterminerTest, ResumedNoPrompt) {
  // All test cases run with GetPathInDownloadDir(kInitialPath) as the inital
  // path.
  const base::FilePath::CharType* kInitialPath =
      FILE_PATH_LITERAL("some_path/bar.txt");

  const DownloadTestCase kResumedTestCases[] = {
      {// 0: Automatic Safe: Initial path is ignored since the user has not been
       // prompted before.
       AUTOMATIC, download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
       DownloadFileType::NOT_DANGEROUS, "http://example.com/foo.txt",
       "text/plain", FILE_PATH_LITERAL(""),

       FILE_PATH_LITERAL("foo.txt"), DownloadItem::TARGET_DISPOSITION_OVERWRITE,

       EXPECT_CRDOWNLOAD},

      {// 1: Save_As Safe: Initial path used.
       SAVE_AS, download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
       DownloadFileType::NOT_DANGEROUS, "http://example.com/foo.txt",
       "text/plain", FILE_PATH_LITERAL(""),

       kInitialPath, DownloadItem::TARGET_DISPOSITION_PROMPT,

       EXPECT_CRDOWNLOAD},

      {// 2: Automatic Dangerous: Initial path is ignored since the user hasn't
       // been prompted before.
       AUTOMATIC, download::DOWNLOAD_DANGER_TYPE_DANGEROUS_FILE,
       DownloadFileType::ALLOW_ON_USER_GESTURE,
       "http://example.com/foo.kindabad", "", FILE_PATH_LITERAL(""),

       FILE_PATH_LITERAL("foo.kindabad"),
       DownloadItem::TARGET_DISPOSITION_OVERWRITE,

       EXPECT_UNCONFIRMED},

      {// 3: Forced Safe: Initial path is ignored due to the forced path.
       FORCED, download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
       DownloadFileType::NOT_DANGEROUS, "http://example.com/foo.txt", "",
       FILE_PATH_LITERAL("forced-foo.txt"),

       FILE_PATH_LITERAL("forced-foo.txt"),
       DownloadItem::TARGET_DISPOSITION_OVERWRITE,

       EXPECT_LOCAL_PATH},
  };

  // The test assumes that .kindabad files have a danger level of
  // ALLOW_ON_USER_GESTURE.
  ASSERT_EQ(
      DownloadFileType::ALLOW_ON_USER_GESTURE,
      safe_browsing::FileTypePolicies::GetInstance()->GetFileDangerLevel(
          base::FilePath(FILE_PATH_LITERAL("foo.kindabad")), GURL{}, nullptr));
  for (size_t i = 0; i < std::size(kResumedTestCases); ++i) {
    SCOPED_TRACE(testing::Message() << "Running test case " << i);
    const DownloadTestCase& test_case = kResumedTestCases[i];
    std::unique_ptr<download::MockDownloadItem> item =
        CreateActiveDownloadItem(i, test_case);
    base::FilePath expected_path =
        GetPathInDownloadDir(test_case.expected_local_path);
    ON_CALL(*item.get(), GetLastReason())
        .WillByDefault(
            Return(download::DOWNLOAD_INTERRUPT_REASON_NETWORK_FAILED));
    // Extensions should be notified if a new path is being generated and there
    // is no forced path. In the test cases above, this is true for tests with
    // type == AUTOMATIC.
    EXPECT_CALL(*delegate(), NotifyExtensions_(_, _, _))
        .Times(test_case.test_type == AUTOMATIC ? 1 : 0);
    EXPECT_CALL(*delegate(),
                ReserveVirtualPath_(_, expected_path, false, _, _));
    EXPECT_CALL(*delegate(), RequestConfirmation_(_, expected_path, _, _))
        .Times(0);
    EXPECT_CALL(*delegate(), DetermineLocalPath_(_, expected_path, _));
    EXPECT_CALL(*delegate(), CheckDownloadUrl_(_, expected_path, _));
    RunTestCase(test_case, GetPathInDownloadDir(kInitialPath), item.get());
  }
}

// Test that a forced download doesn't prompt, even if the interrupt reason
// suggests that the target path may not be suitable for downloads.
TEST_F(DownloadTargetDeterminerTest, ResumedForcedDownload) {
  const base::FilePath::CharType* kInitialPath =
      FILE_PATH_LITERAL("some_path/bar.txt");
  const DownloadTestCase kResumedForcedDownload = {
      // 3: Forced Safe
      FORCED,
      download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
      DownloadFileType::NOT_DANGEROUS,
      "http://example.com/foo.txt",
      "",
      FILE_PATH_LITERAL("forced-foo.txt"),

      FILE_PATH_LITERAL("forced-foo.txt"),
      DownloadItem::TARGET_DISPOSITION_OVERWRITE,

      EXPECT_LOCAL_PATH};

  const DownloadTestCase& test_case = kResumedForcedDownload;
  base::FilePath expected_path =
      GetPathInDownloadDir(test_case.expected_local_path);
  std::unique_ptr<download::MockDownloadItem> item =
      CreateActiveDownloadItem(0, test_case);
  ON_CALL(*item.get(), GetLastReason())
      .WillByDefault(Return(download::DOWNLOAD_INTERRUPT_REASON_FILE_NO_SPACE));
  EXPECT_CALL(*delegate(), NotifyExtensions_(_, _, _))
      .Times(test_case.test_type == AUTOMATIC ? 1 : 0);
  EXPECT_CALL(*delegate(), ReserveVirtualPath_(_, expected_path, false, _, _));
  EXPECT_CALL(*delegate(), RequestConfirmation_(_, _, _, _)).Times(0);
  EXPECT_CALL(*delegate(), DetermineLocalPath_(_, expected_path, _));
  EXPECT_CALL(*delegate(), CheckDownloadUrl_(_, expected_path, _));
  RunTestCase(test_case, GetPathInDownloadDir(kInitialPath), item.get());
}

// Prompting behavior for resumed downloads is based on the last interrupt
// reason. If the reason indicates that the target path may not be suitable for
// the download (ACCESS_DENIED, NO_SPACE, etc..), then the user should be
// prompted, and not otherwise. These test cases result in prompting since the
// error is set to NO_SPACE.
TEST_F(DownloadTargetDeterminerTest, ResumedWithPrompt) {
  // All test cases run with GetPathInDownloadDir(kInitialPath) as the inital
  // path.
  const base::FilePath::CharType* kInitialPath =
      FILE_PATH_LITERAL("some_path/bar.txt");

  const DownloadTestCase kResumedTestCases[] = {
      {// 0: Automatic Safe
       AUTOMATIC, download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
       DownloadFileType::NOT_DANGEROUS, "http://example.com/foo.txt",
       "text/plain", FILE_PATH_LITERAL(""),

       FILE_PATH_LITERAL("foo.txt"), DownloadItem::TARGET_DISPOSITION_PROMPT,
       EXPECT_CRDOWNLOAD},

      {// 1: Save_As Safe
       SAVE_AS, download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
       DownloadFileType::NOT_DANGEROUS, "http://example.com/foo.txt",
       "text/plain", FILE_PATH_LITERAL(""),

       kInitialPath, DownloadItem::TARGET_DISPOSITION_PROMPT,

       EXPECT_CRDOWNLOAD},

      {
          // 2: Automatic Dangerous
          AUTOMATIC, download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
          DownloadFileType::NOT_DANGEROUS, "http://example.com/foo.kindabad",
          "", FILE_PATH_LITERAL(""),

          FILE_PATH_LITERAL("foo.kindabad"),
          DownloadItem::TARGET_DISPOSITION_PROMPT, EXPECT_CRDOWNLOAD,
      },

      {
          // 3: Automatic Dangerous
          AUTOMATIC, download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
          DownloadFileType::NOT_DANGEROUS, "http://example.com/foo.bad", "",
          FILE_PATH_LITERAL(""),

          FILE_PATH_LITERAL("foo.bad"), DownloadItem::TARGET_DISPOSITION_PROMPT,
          EXPECT_CRDOWNLOAD,
      },
  };

  ASSERT_EQ(
      DownloadFileType::ALLOW_ON_USER_GESTURE,
      safe_browsing::FileTypePolicies::GetInstance()->GetFileDangerLevel(
          base::FilePath(FILE_PATH_LITERAL("foo.kindabad")), GURL{}, nullptr));
  for (size_t i = 0; i < std::size(kResumedTestCases); ++i) {
    SCOPED_TRACE(testing::Message() << "Running test case " << i);
    download_prefs()->SetSaveFilePath(test_download_dir());
    const DownloadTestCase& test_case = kResumedTestCases[i];
    base::FilePath expected_path =
        GetPathInDownloadDir(test_case.expected_local_path);
    std::unique_ptr<download::MockDownloadItem> item =
        CreateActiveDownloadItem(i, test_case);
    ON_CALL(*item.get(), GetLastReason())
        .WillByDefault(
            Return(download::DOWNLOAD_INTERRUPT_REASON_FILE_NO_SPACE));
    EXPECT_CALL(*delegate(), NotifyExtensions_(_, _, _))
        .Times(test_case.test_type == AUTOMATIC ? 1 : 0);
    EXPECT_CALL(*delegate(),
                ReserveVirtualPath_(_, expected_path, false, _, _));
    EXPECT_CALL(*delegate(), RequestConfirmation_(_, expected_path, _, _));
    EXPECT_CALL(*delegate(), DetermineLocalPath_(_, expected_path, _));
    EXPECT_CALL(*delegate(), CheckDownloadUrl_(_, expected_path, _));
    RunTestCase(test_case, GetPathInDownloadDir(kInitialPath), item.get());
  }
}

// Test intermediate filename generation for resumed downloads.
TEST_F(DownloadTargetDeterminerTest, IntermediateNameForResumed) {
  // All test cases run with GetPathInDownloadDir(kInitialPath) as the inital
  // path.
  const base::FilePath::CharType kInitialPath[] =
      FILE_PATH_LITERAL("some_path/bar.txt");

  struct IntermediateNameTestCase {
    // General test case settings.
    DownloadTestCase general;

    // Value of DownloadItem::GetFullPath() during test run, relative
    // to test download path.
    const base::FilePath::CharType* initial_intermediate_path;

    // Expected intermediate path relatvie to the test download path. An exact
    // match is performed if this string is non-empty. Ignored otherwise.
    const base::FilePath::CharType* expected_intermediate_path;
  } kIntermediateNameTestCases[] = {
      {{// 0: Automatic Safe
        AUTOMATIC, download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
        DownloadFileType::NOT_DANGEROUS, "http://example.com/foo.txt",
        "text/plain", FILE_PATH_LITERAL(""),

        FILE_PATH_LITERAL("foo.txt"),
        DownloadItem::TARGET_DISPOSITION_OVERWRITE,

        EXPECT_CRDOWNLOAD},
       FILE_PATH_LITERAL("bar.txt.crdownload"),
       FILE_PATH_LITERAL("foo.txt.crdownload")},

      {{// 1: Save_As Safe
        SAVE_AS, download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
        DownloadFileType::NOT_DANGEROUS, "http://example.com/foo.txt",
        "text/plain", FILE_PATH_LITERAL(""),

        kInitialPath, DownloadItem::TARGET_DISPOSITION_PROMPT,

        EXPECT_CRDOWNLOAD},
       FILE_PATH_LITERAL("foo.txt.crdownload"),
       FILE_PATH_LITERAL("some_path/bar.txt.crdownload")},

      {{// 2: Automatic Dangerous
        AUTOMATIC, download::DOWNLOAD_DANGER_TYPE_DANGEROUS_FILE,
        DownloadFileType::ALLOW_ON_USER_GESTURE,
        "http://example.com/foo.kindabad", "", FILE_PATH_LITERAL(""),

        FILE_PATH_LITERAL("foo.kindabad"),
        DownloadItem::TARGET_DISPOSITION_OVERWRITE,

        EXPECT_UNCONFIRMED},
       FILE_PATH_LITERAL("Unconfirmed abcd.crdownload"),
       FILE_PATH_LITERAL("Unconfirmed abcd.crdownload")},

      {{// 3: Automatic Dangerous
        AUTOMATIC, download::DOWNLOAD_DANGER_TYPE_DANGEROUS_FILE,
        DownloadFileType::ALLOW_ON_USER_GESTURE,
        "http://example.com/foo.kindabad", "", FILE_PATH_LITERAL(""),

        FILE_PATH_LITERAL("foo.kindabad"),
        DownloadItem::TARGET_DISPOSITION_OVERWRITE,

        EXPECT_UNCONFIRMED},
       FILE_PATH_LITERAL("other_path/Unconfirmed abcd.crdownload"),
       // Rely on the EXPECT_UNCONFIRMED check in the general test settings. A
       // new intermediate path of the form "Unconfirmed <number>.crdownload"
       // should be generated for this case since the initial intermediate path
       // is in the wrong directory.
       FILE_PATH_LITERAL("")},

      {{// 3: Forced Safe: Initial path is ignored due to the forced path.
        FORCED, download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
        DownloadFileType::NOT_DANGEROUS, "http://example.com/foo.txt", "",
        FILE_PATH_LITERAL("forced-foo.txt"),

        FILE_PATH_LITERAL("forced-foo.txt"),
        DownloadItem::TARGET_DISPOSITION_OVERWRITE,

        EXPECT_LOCAL_PATH},
       FILE_PATH_LITERAL("forced-foo.txt"),
       FILE_PATH_LITERAL("forced-foo.txt")},
  };

  // The test assumes that .kindabad files have a danger level of
  // ALLOW_ON_USER_GESTURE.
  ASSERT_EQ(
      DownloadFileType::ALLOW_ON_USER_GESTURE,
      safe_browsing::FileTypePolicies::GetInstance()->GetFileDangerLevel(
          base::FilePath(FILE_PATH_LITERAL("foo.kindabad")), GURL{}, nullptr));

  for (size_t i = 0; i < std::size(kIntermediateNameTestCases); ++i) {
    SCOPED_TRACE(testing::Message() << "Running test case " << i);
    const IntermediateNameTestCase& test_case = kIntermediateNameTestCases[i];
    std::unique_ptr<download::MockDownloadItem> item =
        CreateActiveDownloadItem(i, test_case.general);

    ON_CALL(*item.get(), GetLastReason())
        .WillByDefault(
            Return(download::DOWNLOAD_INTERRUPT_REASON_NETWORK_FAILED));
    ON_CALL(*item.get(), GetFullPath())
        .WillByDefault(ReturnRefOfCopy(
            GetPathInDownloadDir(test_case.initial_intermediate_path)));
    ON_CALL(*item.get(), GetDangerType())
        .WillByDefault(Return(test_case.general.expected_danger_type));

    TargetInfoAndDangerLevel info = RunDownloadTargetDeterminer(
        GetPathInDownloadDir(kInitialPath), item.get());
    VerifyDownloadTarget(test_case.general, info);
    base::FilePath expected_intermediate_path =
        GetPathInDownloadDir(test_case.expected_intermediate_path);
    if (!expected_intermediate_path.empty())
      EXPECT_EQ(expected_intermediate_path, info.target_info.intermediate_path);
  }
}

// Test MIME type determination based on the target filename.
TEST_F(DownloadTargetDeterminerTest, MIMETypeDetermination) {
  // All test cases run with GetPathInDownloadDir(kInitialPath) as the inital
  // path.
  const base::FilePath::CharType kInitialPath[] =
      FILE_PATH_LITERAL("some_path/bar.txt");

  struct MIMETypeTestCase {
    // General test case settings.
    DownloadTestCase general;

    // Expected MIME type for test case.
    const char* expected_mime_type;
  } kMIMETypeTestCases[] = {
      {{// 0:
        AUTOMATIC, download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
        DownloadFileType::NOT_DANGEROUS, "http://example.com/foo.png",
        "image/png", FILE_PATH_LITERAL(""),

        FILE_PATH_LITERAL("foo.png"),
        DownloadItem::TARGET_DISPOSITION_OVERWRITE,

        EXPECT_CRDOWNLOAD},
       "image/png"},
      {{// 1: Empty MIME type in response.
        AUTOMATIC, download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
        DownloadFileType::NOT_DANGEROUS, "http://example.com/foo.png", "",
        FILE_PATH_LITERAL(""),

        FILE_PATH_LITERAL("foo.png"),
        DownloadItem::TARGET_DISPOSITION_OVERWRITE,

        EXPECT_CRDOWNLOAD},
       "image/png"},
      {{// 2: Forced path.
        FORCED, download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
        DownloadFileType::NOT_DANGEROUS, "http://example.com/foo.abc", "",
        FILE_PATH_LITERAL("foo.png"),

        FILE_PATH_LITERAL("foo.png"),
        DownloadItem::TARGET_DISPOSITION_OVERWRITE,

        EXPECT_CRDOWNLOAD},
       "image/png"},
      {{// 3: Unknown file type.
        AUTOMATIC, download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
        DownloadFileType::NOT_DANGEROUS, "http://example.com/foo.notarealext",
        "", FILE_PATH_LITERAL(""),

        FILE_PATH_LITERAL("foo.notarealext"),
        DownloadItem::TARGET_DISPOSITION_OVERWRITE,

        EXPECT_CRDOWNLOAD},
       ""},
      {{// 4: Unknown file type.
        AUTOMATIC, download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
        DownloadFileType::NOT_DANGEROUS, "http://example.com/foo.notarealext",
        "", FILE_PATH_LITERAL(""),

        FILE_PATH_LITERAL("foo.notarealext"),
        DownloadItem::TARGET_DISPOSITION_OVERWRITE,

        EXPECT_CRDOWNLOAD},
       ""},
      {{// 5: x-x509-user-cert mime-type.
        AUTOMATIC, download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
        DownloadFileType::NOT_DANGEROUS, "http://example.com/foo.notarealext",
        "application/x-x509-user-cert", FILE_PATH_LITERAL(""),

        FILE_PATH_LITERAL("user.crt"),
        DownloadItem::TARGET_DISPOSITION_OVERWRITE,

        EXPECT_CRDOWNLOAD},
       ""},
  };

  ON_CALL(
      *delegate(),
      GetFileMimeType_(GetPathInDownloadDir(FILE_PATH_LITERAL("foo.png")), _))
      .WillByDefault(WithArg<1>(ScheduleCallback("image/png")));

  for (size_t i = 0; i < std::size(kMIMETypeTestCases); ++i) {
    SCOPED_TRACE(testing::Message() << "Running test case " << i);
    const MIMETypeTestCase& test_case = kMIMETypeTestCases[i];
    std::unique_ptr<download::MockDownloadItem> item =
        CreateActiveDownloadItem(i, test_case.general);
    TargetInfoAndDangerLevel info = RunDownloadTargetDeterminer(
        GetPathInDownloadDir(kInitialPath), item.get());
    EXPECT_EQ(test_case.expected_mime_type, info.target_info.mime_type);
  }
}

// Test that the mime type given by the server may determine the target file
// extension.
// The file extension is generated through mime type when:
// 1. Content-Type header is not empty.
// 2. No suggested file name, which has higher priority than mime type to
// generate file name.
// 3. No force file name, which has higher priority.
// 4. The extension generated from the URL is considered safe by safe browsing,
// and it's not required to do security check by safe browsing. No matter the
// new extension is safe or unsafe, we never decrease the number of safe
// browsing checks.
TEST_F(DownloadTargetDeterminerTest, MimeTypeFileExtension) {
  // Safe browsing and empty mime type tests.
  struct MimeTypeFileExtensionTestCase {
    // General test case settings.
    DownloadTestCase general;

    // Return value of DownloadItem::GetSuggestedFilename().
    std::string suggested_file_name;

    // Return value of DownloadItem::GetOriginalMimeType().
    std::string original_mime_type;
  } kTestCases[] = {
      {{// 0: Unsafe file extension generated by URL should not be replaced
        // to a safe extension to bypass the safe browsing check.
        AUTOMATIC, download::DOWNLOAD_DANGER_TYPE_DANGEROUS_FILE,
        DownloadFileType::DANGEROUS, "http://example.com/foo.bad", "image/png",
        FILE_PATH_LITERAL(""), FILE_PATH_LITERAL("foo.bad"),
        DownloadItem::TARGET_DISPOSITION_OVERWRITE, EXPECT_UNCONFIRMED},
       ""},
      {{// 1: Generate file extension based on non-text sniffed mime types.
        AUTOMATIC, download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
        DownloadFileType::NOT_DANGEROUS, "http://example.com/foo.png",
        "image/gif", FILE_PATH_LITERAL(""), FILE_PATH_LITERAL("foo.gif"),
        DownloadItem::TARGET_DISPOSITION_OVERWRITE, EXPECT_CRDOWNLOAD},
       ""},
      {{// 2: Generate file extension from URL for text/plain sniffed mime type.
        AUTOMATIC, download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
        DownloadFileType::NOT_DANGEROUS, "http://example.com/foo.csv",
        "text/plain", FILE_PATH_LITERAL(""), FILE_PATH_LITERAL("foo.csv"),
        DownloadItem::TARGET_DISPOSITION_OVERWRITE, EXPECT_CRDOWNLOAD},
       ""},
      {{// 3: Sniffed mime type and original mime type are both text/plain.
        AUTOMATIC, download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
        DownloadFileType::NOT_DANGEROUS, "http://example.com/foo.xml",
        "text/plain" /*mime_type*/, FILE_PATH_LITERAL(""),
        FILE_PATH_LITERAL("foo.txt"),
        DownloadItem::TARGET_DISPOSITION_OVERWRITE, EXPECT_CRDOWNLOAD},
       "",
       "text/plain" /*original_mime_type*/},
      {{// 4: Sniffed mime type is text/plain, original mime type is not
        // text/plain. Use the URL file extension.
        AUTOMATIC, download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
        DownloadFileType::NOT_DANGEROUS, "http://example.com/foo.xml",
        "text/plain" /*mime_type*/, FILE_PATH_LITERAL(""),
        FILE_PATH_LITERAL("foo.xml"),
        DownloadItem::TARGET_DISPOSITION_OVERWRITE, EXPECT_CRDOWNLOAD},
       "",
       "image/png" /*original_mime_type*/},
      {{// 5: Forced file path. Mime type from Content-Type should not affect
        // file extension.
        AUTOMATIC, download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
        DownloadFileType::NOT_DANGEROUS, "http://example.com/foo.png",
        "image/gif", FILE_PATH_LITERAL("foo.txt") /* forced_file_path*/,
        FILE_PATH_LITERAL("foo.txt"),
        DownloadItem::TARGET_DISPOSITION_OVERWRITE, EXPECT_LOCAL_PATH},
       ""},
      {{// 6: Empty mime type.
        AUTOMATIC, download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
        DownloadFileType::NOT_DANGEROUS, "http://example.com/foo.png", "",
        FILE_PATH_LITERAL(""), FILE_PATH_LITERAL("foo.png"),
        DownloadItem::TARGET_DISPOSITION_OVERWRITE, EXPECT_CRDOWNLOAD},
       ""},
      {{// 7: Suggested file name. Mime type from Content-Type should not affect
        // file extension.
        AUTOMATIC, download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
        DownloadFileType::NOT_DANGEROUS, "http://example.com/foo.png",
        "image/gif", FILE_PATH_LITERAL(""), FILE_PATH_LITERAL("foo.txt"),
        DownloadItem::TARGET_DISPOSITION_OVERWRITE, EXPECT_CRDOWNLOAD},
       "foo.txt" /* suggested_file_name */}};

  for (size_t i = 0; i < std::size(kTestCases); ++i) {
    std::unique_ptr<download::MockDownloadItem> item =
        CreateActiveDownloadItem(i, kTestCases[i].general);
    ON_CALL(*item, GetSuggestedFilename())
        .WillByDefault(Return(kTestCases[i].suggested_file_name));
    ON_CALL(*item, GetOriginalMimeType())
        .WillByDefault(Return(kTestCases[i].original_mime_type));
    RunTestCase(kTestCases[i].general, base::FilePath(), item.get());
  }
}

// Test that a user validated download won't be treated as dangerous.
TEST_F(DownloadTargetDeterminerTest, ResumedWithUserValidatedDownload) {
  const base::FilePath::CharType kInitialPath[] =
      FILE_PATH_LITERAL("some_path/bar.txt");
  const base::FilePath::CharType* kIntermediatePath =
      FILE_PATH_LITERAL("foo.crx.crdownload");

  const DownloadTestCase kUserValidatedTestCase = {
      AUTOMATIC,
      download::DOWNLOAD_DANGER_TYPE_USER_VALIDATED,
      DownloadFileType::NOT_DANGEROUS,
      "http://example.com/foo.crx",
      "",
      FILE_PATH_LITERAL(""),
      FILE_PATH_LITERAL("foo.crx"),
      DownloadItem::TARGET_DISPOSITION_OVERWRITE,
      EXPECT_CRDOWNLOAD};

  const DownloadTestCase& test_case = kUserValidatedTestCase;
  std::unique_ptr<download::MockDownloadItem> item(
      CreateActiveDownloadItem(0, test_case));
  base::FilePath expected_path =
      GetPathInDownloadDir(test_case.expected_local_path);
  ON_CALL(*item.get(), GetDangerType())
      .WillByDefault(Return(download::DOWNLOAD_DANGER_TYPE_USER_VALIDATED));
  ON_CALL(*item.get(), GetFullPath())
      .WillByDefault(ReturnRefOfCopy(GetPathInDownloadDir(kIntermediatePath)));
  ON_CALL(*item.get(), GetLastReason())
      .WillByDefault(
          Return(download::DOWNLOAD_INTERRUPT_REASON_NETWORK_FAILED));
  EXPECT_CALL(*delegate(), NotifyExtensions_(_, _, _));
  EXPECT_CALL(*delegate(), ReserveVirtualPath_(_, expected_path, false, _, _));
  EXPECT_CALL(*delegate(), DetermineLocalPath_(_, expected_path, _));
  EXPECT_CALL(*delegate(), CheckDownloadUrl_(_, expected_path, _)).Times(0);
  EXPECT_CALL(*delegate(), RequestConfirmation_(_, _, _, _)).Times(0);
  RunTestCase(test_case, GetPathInDownloadDir(kInitialPath), item.get());
}

// Test that verifies transient download target determination.
TEST_F(DownloadTargetDeterminerTest, TransientDownload) {
  const base::FilePath::CharType kInitialPath[] =
      FILE_PATH_LITERAL("some_path/bar.txt");

  DownloadTestCase transient_test_case = {
      TRANSIENT,
      download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
      DownloadFileType::NOT_DANGEROUS,
      "http://example.com/foo",
      "",
      FILE_PATH_LITERAL("12345"), /* forced_file_path */
      FILE_PATH_LITERAL("12345"),
      DownloadItem::TARGET_DISPOSITION_OVERWRITE,
      EXPECT_LOCAL_PATH};

  std::unique_ptr<download::MockDownloadItem> item(
      CreateActiveDownloadItem(0, transient_test_case));
  base::FilePath expected_path =
      GetPathInDownloadDir(transient_test_case.expected_local_path);

  EXPECT_CALL(*delegate(), NotifyExtensions_(_, _, _)).Times(0);
  EXPECT_CALL(*delegate(), ReserveVirtualPath_(_, expected_path, false,
                                               ConflictAction::UNIQUIFY, _))
      .Times(1);
  EXPECT_CALL(*delegate(), DetermineLocalPath_(_, expected_path, _)).Times(1);
  EXPECT_CALL(*delegate(), CheckDownloadUrl_(_, expected_path, _)).Times(1);
  EXPECT_CALL(*delegate(), RequestConfirmation_(_, _, _, _)).Times(0);

  base::HistogramTester histogram_tester;
  RunTestCase(transient_test_case, GetPathInDownloadDir(kInitialPath),
              item.get());
  histogram_tester.ExpectBucketCount(
      kTransientPathGenerationHistogram,
      ToHistogramSample<DownloadPathGenerationEvent>(
          DownloadPathGenerationEvent::USE_FORCE_PATH),
      1);
  histogram_tester.ExpectBucketCount(
      kTransientPathValidationHistogram,
      ToHistogramSample<download::PathValidationResult>(
          download::PathValidationResult::SUCCESS),
      1);
  histogram_tester.ExpectTotalCount(kTransientPathGenerationHistogram, 1);
  histogram_tester.ExpectTotalCount(kTransientPathValidationHistogram, 1);
}

// Simulate resumption on transient download. The download item does not provide
// forced path in this case.
TEST_F(DownloadTargetDeterminerTest, TransientDownloadResumption) {
  const base::FilePath::CharType kInitialPath[] =
      FILE_PATH_LITERAL("some_path/bar.txt");
  base::FilePath expected_path = GetPathInDownloadDir(kInitialPath);

  DownloadTestCase transient_test_case = {
      TRANSIENT,
      download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
      DownloadFileType::NOT_DANGEROUS,
      "http://example.com/foo",
      "",
      FILE_PATH_LITERAL(""), /* forced_file_path */
      kInitialPath,
      DownloadItem::TARGET_DISPOSITION_OVERWRITE,
      EXPECT_LOCAL_PATH};

  // Simulate resumption that provides the full path and a failure reason.
  std::unique_ptr<download::MockDownloadItem> item(
      CreateActiveDownloadItem(0, transient_test_case));

  ON_CALL(*item.get(), GetFullPath())
      .WillByDefault(ReturnRefOfCopy(expected_path));
  ON_CALL(*item.get(), GetLastReason())
      .WillByDefault(
          Return(download::DOWNLOAD_INTERRUPT_REASON_NETWORK_FAILED));

  EXPECT_CALL(*delegate(), NotifyExtensions_(_, _, _)).Times(0);
  EXPECT_CALL(*delegate(), ReserveVirtualPath_(_, expected_path, false,
                                               ConflictAction::UNIQUIFY, _))
      .Times(1);
  EXPECT_CALL(*delegate(), DetermineLocalPath_(_, expected_path, _));
  EXPECT_CALL(*delegate(), CheckDownloadUrl_(_, expected_path, _)).Times(1);
  EXPECT_CALL(*delegate(), RequestConfirmation_(_, _, _, _)).Times(0);

  base::HistogramTester histogram_tester;
  RunTestCase(transient_test_case, GetPathInDownloadDir(kInitialPath),
              item.get());
  histogram_tester.ExpectBucketCount(
      kTransientPathGenerationHistogram,
      ToHistogramSample<DownloadPathGenerationEvent>(
          DownloadPathGenerationEvent::USE_EXISTING_VIRTUAL_PATH),
      1);
  histogram_tester.ExpectBucketCount(
      kTransientPathValidationHistogram,
      ToHistogramSample<download::PathValidationResult>(
          download::PathValidationResult::SUCCESS),
      1);
  histogram_tester.ExpectTotalCount(kTransientPathGenerationHistogram, 1);
  histogram_tester.ExpectTotalCount(kTransientPathValidationHistogram, 1);
}

#if BUILDFLAG(IS_WIN)
// Test that env variables will be removed from file name before prompting Save
// As dialog.
TEST_F(DownloadTargetDeterminerTest, TestSanitizeEnvVariable) {
  const DownloadTestCase kSaveEnvPathTestCases[] = {
      {// 0: File name contains env var delimits.
       SAVE_AS, download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
       DownloadFileType::NOT_DANGEROUS, "http://example.com/f%oo%.tx%xyz%t",
       "text/plain", FILE_PATH_LITERAL(""),

       FILE_PATH_LITERAL("f.txt"), DownloadItem::TARGET_DISPOSITION_PROMPT,

       EXPECT_CRDOWNLOAD},

      {// 1: File name contains dangerous extensions after removing env var.
       SAVE_AS, download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
       DownloadFileType::NOT_DANGEROUS, "http://example.com/foo.ln%xyz%k",
       "application/octet-stream", FILE_PATH_LITERAL(""),

       FILE_PATH_LITERAL("foo.download"),
       DownloadItem::TARGET_DISPOSITION_PROMPT,

       EXPECT_CRDOWNLOAD},
      {// 2: File name falling back to dangerous extensions after removing env var.
       SAVE_AS, download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
       DownloadFileType::NOT_DANGEROUS, "http://example.com/foo2.lnk.%%",
       "application/octet-stream", FILE_PATH_LITERAL(""),

       FILE_PATH_LITERAL("foo2.download"),
       DownloadItem::TARGET_DISPOSITION_PROMPT,

       EXPECT_CRDOWNLOAD},
      {// 3: File name is an env var.
       SAVE_AS, download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
       DownloadFileType::NOT_DANGEROUS, "http://example.com/%foo.txt%",
       "text/plain", FILE_PATH_LITERAL(""),

       FILE_PATH_LITERAL("download"), DownloadItem::TARGET_DISPOSITION_PROMPT,

       EXPECT_CRDOWNLOAD}};

  RunTestCasesWithActiveItem(kSaveEnvPathTestCases,
                             std::size(kSaveEnvPathTestCases));
}
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(ENABLE_PLUGINS)

void DummyGetPluginsCallback(
    base::OnceClosure closure,
    const std::vector<content::WebPluginInfo>& plugins) {
  std::move(closure).Run();
}

void ForceRefreshOfPlugins() {
#if !BUILDFLAG(IS_WIN)
  // Prevent creation of a utility process for loading plugins. Doing so breaks
  // unit_tests since /proc/self/exe can't be run as a utility process.
  content::RenderProcessHost::SetRunRendererInProcess(true);
#endif
  base::RunLoop run_loop;
  content::PluginService::GetInstance()->GetPlugins(
      base::BindOnce(&DummyGetPluginsCallback, run_loop.QuitClosure()));
  run_loop.Run();
#if !BUILDFLAG(IS_WIN)
  content::RenderProcessHost::SetRunRendererInProcess(false);
#endif
}

class MockPluginServiceFilter : public content::PluginServiceFilter {
 public:
  MOCK_METHOD1(MockPluginAvailable, bool(const base::FilePath&));

  bool IsPluginAvailable(content::BrowserContext* browser_context,
                         const content::WebPluginInfo& plugin) override {
    return MockPluginAvailable(plugin.path);
  }

  bool CanLoadPlugin(int render_process_id,
                     const base::FilePath& path) override {
    return true;
  }
};

class ScopedRegisterInternalPlugin {
 public:
  ScopedRegisterInternalPlugin(content::PluginService* plugin_service,
                               content::WebPluginInfo::PluginType type,
                               const base::FilePath& path,
                               const char* mime_type,
                               const char* extension)
      : plugin_service_(plugin_service),
        plugin_path_(path) {
    content::WebPluginMimeType plugin_mime_type(mime_type,
                                                extension,
                                                "Test file");
    content::WebPluginInfo plugin_info(std::u16string(), path, std::u16string(),
                                       std::u16string());
    plugin_info.mime_types.push_back(plugin_mime_type);
    plugin_info.type = type;

    plugin_service->RegisterInternalPlugin(plugin_info, true);
    plugin_service->RefreshPlugins();
    ForceRefreshOfPlugins();
  }

  ~ScopedRegisterInternalPlugin() {
    plugin_service_->UnregisterInternalPlugin(plugin_path_);
    plugin_service_->RefreshPlugins();
    ForceRefreshOfPlugins();
  }

  const base::FilePath& path() { return plugin_path_; }

 private:
  raw_ptr<content::PluginService> plugin_service_;
  base::FilePath plugin_path_;
};

// We use a slightly different test fixture for tests that touch plugins. SetUp
// needs to disable plugin discovery.
class DownloadTargetDeterminerTestWithPlugin
    : public DownloadTargetDeterminerTest {
 public:
  DownloadTargetDeterminerTestWithPlugin()
      : old_plugin_service_filter_(nullptr) {}

  void SetUp() override {
    DownloadTargetDeterminerTest::SetUp();
    content::PluginService* plugin_service =
        content::PluginService::GetInstance();
    plugin_service->Init();
    old_plugin_service_filter_ = plugin_service->GetFilter();
    plugin_service->SetFilter(&mock_plugin_filter_);
  }

  void TearDown() override {
    content::PluginService::GetInstance()->SetFilter(
        old_plugin_service_filter_);
    DownloadTargetDeterminerTest::TearDown();
  }

 protected:
  raw_ptr<content::PluginServiceFilter> old_plugin_service_filter_;
  testing::StrictMock<MockPluginServiceFilter> mock_plugin_filter_;
};

// Check if secure handling of filetypes is determined correctly for PPAPI
// plugins.
TEST_F(DownloadTargetDeterminerTestWithPlugin, CheckForSecureHandling_PPAPI) {
  // All test cases run with GetPathInDownloadDir(kInitialPath) as the inital
  // path.
  const base::FilePath::CharType kInitialPath[] =
      FILE_PATH_LITERAL("some_path/bar.txt");
  const char kTestMIMEType[] = "application/x-example-should-not-exist";

  DownloadTestCase kSecureHandlingTestCase = {
      AUTOMATIC,
      download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
      DownloadFileType::NOT_DANGEROUS,
      "http://example.com/foo.fakeext",
      "",
      FILE_PATH_LITERAL(""),

      FILE_PATH_LITERAL("foo.fakeext"),
      DownloadItem::TARGET_DISPOSITION_OVERWRITE,

      EXPECT_CRDOWNLOAD};

  content::PluginService* plugin_service =
      content::PluginService::GetInstance();

  // Verify our test assumptions.
  {
    ForceRefreshOfPlugins();
    std::vector<content::WebPluginInfo> info;
    ASSERT_FALSE(plugin_service->GetPluginInfoArray(GURL(), kTestMIMEType,
                                                    false, &info, nullptr));
    ASSERT_EQ(0u, info.size())
        << "Name: " << info[0].name << ", Path: " << info[0].path.value();
  }

  ON_CALL(*delegate(),
          GetFileMimeType_(
              GetPathInDownloadDir(FILE_PATH_LITERAL("foo.fakeext")), _))
      .WillByDefault(WithArg<1>(ScheduleCallback(kTestMIMEType)));
  std::unique_ptr<download::MockDownloadItem> item =
      CreateActiveDownloadItem(1, kSecureHandlingTestCase);
  TargetInfoAndDangerLevel info = RunDownloadTargetDeterminer(
      GetPathInDownloadDir(kInitialPath), item.get());
  EXPECT_FALSE(info.target_info.is_filetype_handled_safely);

  // Register a PPAPI plugin. This should count as handling the filetype
  // securely.
  ScopedRegisterInternalPlugin ppapi_plugin(
      plugin_service,
      content::WebPluginInfo::PLUGIN_TYPE_PEPPER_OUT_OF_PROCESS,
      test_download_dir().AppendASCII("ppapi"),
      kTestMIMEType,
      "fakeext");
  EXPECT_CALL(mock_plugin_filter_, MockPluginAvailable(ppapi_plugin.path()))
      .WillRepeatedly(Return(true));

  info = RunDownloadTargetDeterminer(GetPathInDownloadDir(kInitialPath),
                                     item.get());
  EXPECT_TRUE(info.target_info.is_filetype_handled_safely);

  // Try disabling the plugin. Handling should no longer be considered secure.
  EXPECT_CALL(mock_plugin_filter_, MockPluginAvailable(ppapi_plugin.path()))
      .WillRepeatedly(Return(false));
  info = RunDownloadTargetDeterminer(GetPathInDownloadDir(kInitialPath),
                                     item.get());
  EXPECT_FALSE(info.target_info.is_filetype_handled_safely);
}

// Check if secure handling of filetypes is determined correctly for
// BrowserPlugins.
TEST_F(DownloadTargetDeterminerTestWithPlugin,
       CheckForSecureHandling_BrowserPlugin) {
  // All test cases run with GetPathInDownloadDir(kInitialPath) as the inital
  // path.
  const base::FilePath::CharType kInitialPath[] =
      FILE_PATH_LITERAL("some_path/bar.txt");
  const char kTestMIMEType[] = "application/x-example-should-not-exist";

  DownloadTestCase kSecureHandlingTestCase = {
      AUTOMATIC,
      download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
      DownloadFileType::NOT_DANGEROUS,
      "http://example.com/foo.fakeext",
      "",
      FILE_PATH_LITERAL(""),

      FILE_PATH_LITERAL("foo.fakeext"),
      DownloadItem::TARGET_DISPOSITION_OVERWRITE,

      EXPECT_CRDOWNLOAD};

  content::PluginService* plugin_service =
      content::PluginService::GetInstance();

  // Verify our test assumptions.
  {
    ForceRefreshOfPlugins();
    std::vector<content::WebPluginInfo> info;
    ASSERT_FALSE(plugin_service->GetPluginInfoArray(GURL(), kTestMIMEType,
                                                    false, &info, nullptr));
    ASSERT_EQ(0u, info.size())
        << "Name: " << info[0].name << ", Path: " << info[0].path.value();
  }

  ON_CALL(*delegate(),
          GetFileMimeType_(
              GetPathInDownloadDir(FILE_PATH_LITERAL("foo.fakeext")), _))
      .WillByDefault(WithArg<1>(ScheduleCallback(kTestMIMEType)));
  std::unique_ptr<download::MockDownloadItem> item =
      CreateActiveDownloadItem(1, kSecureHandlingTestCase);
  TargetInfoAndDangerLevel info = RunDownloadTargetDeterminer(
      GetPathInDownloadDir(kInitialPath), item.get());
  EXPECT_FALSE(info.target_info.is_filetype_handled_safely);

  // Register a BrowserPlugin. This should count as handling the filetype
  // securely.
  ScopedRegisterInternalPlugin browser_plugin(
      plugin_service,
      content::WebPluginInfo::PLUGIN_TYPE_BROWSER_PLUGIN,
      test_download_dir().AppendASCII("browser_plugin"),
      kTestMIMEType,
      "fakeext");
  EXPECT_CALL(mock_plugin_filter_, MockPluginAvailable(browser_plugin.path()))
      .WillRepeatedly(Return(true));

  info = RunDownloadTargetDeterminer(GetPathInDownloadDir(kInitialPath),
                                     item.get());
  EXPECT_TRUE(info.target_info.is_filetype_handled_safely);

  // Try disabling the plugin. Handling should no longer be considered secure.
  EXPECT_CALL(mock_plugin_filter_, MockPluginAvailable(browser_plugin.path()))
      .WillRepeatedly(Return(false));
  info = RunDownloadTargetDeterminer(GetPathInDownloadDir(kInitialPath),
                                     item.get());
  EXPECT_FALSE(info.target_info.is_filetype_handled_safely);
}

#endif  // BUILDFLAG(ENABLE_PLUGINS)

#if BUILDFLAG(IS_ANDROID)
// If a content URI is returned when determining local path, virtual path
// is updated.
TEST_F(DownloadTargetDeterminerTest, DetermineLocalPathReturnsContentUri) {
  const DownloadTestCase kLocalPathContentUriCase = {
      AUTOMATIC,
      download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
      DownloadFileType::NOT_DANGEROUS,
      "http://example.com/foo.txt",
      "text/plain",
      FILE_PATH_LITERAL(""),
      FILE_PATH_LITERAL("content://media/123"),
      DownloadItem::TARGET_DISPOSITION_OVERWRITE,
      EXPECT_LOCAL_PATH};

  std::unique_ptr<download::MockDownloadItem> item =
      CreateActiveDownloadItem(0, kLocalPathContentUriCase);
  // The default download directory is the virtual path.
  download_prefs()->SetDownloadPath(test_virtual_dir());

  EXPECT_CALL(
      *delegate(),
      DetermineLocalPath_(
          _, GetPathInDownloadDir(FILE_PATH_LITERAL("virtual/foo.txt")), _))
      .WillOnce(WithArg<2>(ScheduleCallback2(
          base::FilePath("content://media/123"), base::FilePath("foor.txt"))));
  TargetInfoAndDangerLevel info =
      RunDownloadTargetDeterminer(base::FilePath(), item.get());

  EXPECT_EQ(info.target_info.display_name.value(), "foor.txt");
  EXPECT_EQ(info.target_info.target_path.value(), "content://media/123");
}
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Tests that DLP policies are checked before determining the download path.
class DownloadTargetDeterminerDlpTest : public DownloadTargetDeterminerTest {
 protected:
  DownloadTargetDeterminerDlpTest()
      : profile_(std::make_unique<TestingProfile>()),
        user_manager_(new ash::FakeChromeUserManager()),
        scoped_user_manager_(std::make_unique<user_manager::ScopedUserManager>(
            base::WrapUnique(user_manager_.get()))) {}

  class MockFilesController : public policy::DlpFilesControllerAsh {
   public:
    explicit MockFilesController(const policy::DlpRulesManager& rules_manager,
                                 Profile* profile)
        : DlpFilesControllerAsh(rules_manager, profile) {}
    ~MockFilesController() override = default;

    MOCK_METHOD(bool,
                ShouldPromptBeforeDownload,
                (const policy::DlpFileDestination&, const base::FilePath&),
                (override));
  };

  void SetUp() override {
    DownloadTargetDeterminerTest::SetUp();

    AccountId account_id =
        AccountId::FromUserEmailGaiaId("test@example.com", "12345");
    profile_->SetIsNewProfile(true);
    user_manager::User* user =
        user_manager_->AddUserWithAffiliationAndTypeAndProfile(
            account_id, /*is_affiliated=*/false,
            user_manager::UserType::kRegular, profile_.get());
    user_manager_->UserLoggedIn(account_id, user->username_hash(),
                                /*browser_restart=*/false,
                                /*is_child=*/false);
    user_manager_->SimulateUserProfileLoad(account_id);
  }

  void TearDown() override {
    mock_files_controller_.reset();
    scoped_user_manager_.reset();
    profile_.reset();

    DownloadTargetDeterminerTest::TearDown();
  }

  std::unique_ptr<KeyedService> SetDlpRulesManager(
      content::BrowserContext* context) {
    auto dlp_rules_manager =
        std::make_unique<testing::NiceMock<policy::MockDlpRulesManager>>(
            Profile::FromBrowserContext(context));
    rules_manager_ = dlp_rules_manager.get();
    return dlp_rules_manager;
  }

  void SetupRulesManager() {
    policy::DlpRulesManagerFactory::GetInstance()->SetTestingFactory(
        profile_.get(),
        base::BindRepeating(
            &DownloadTargetDeterminerDlpTest::SetDlpRulesManager,
            base::Unretained(this)));
    ASSERT_TRUE(policy::DlpRulesManagerFactory::GetForPrimaryProfile());

    ON_CALL(*rules_manager_, IsFilesPolicyEnabled)
        .WillByDefault(testing::Return(true));
    mock_files_controller_ =
        std::make_unique<MockFilesController>(*rules_manager_, profile_.get());
    ON_CALL(*rules_manager_, GetDlpFilesController)
        .WillByDefault(testing::Return(mock_files_controller_.get()));
  }

  std::unique_ptr<TestingProfile> profile_;
  raw_ptr<ash::FakeChromeUserManager, DanglingUntriaged> user_manager_;
  std::unique_ptr<user_manager::ScopedUserManager> scoped_user_manager_;
  raw_ptr<policy::MockDlpRulesManager, DanglingUntriaged> rules_manager_ =
      nullptr;
  std::unique_ptr<MockFilesController> mock_files_controller_ = nullptr;
};

// Download URL might be invalid. Dlp must not crash in that case
// (b/300605501).
TEST_F(DownloadTargetDeterminerDlpTest, InvalidUrl) {
  SetupRulesManager();

  const DownloadTestCase kManagedPathTestCase = {
      AUTOMATIC,
      download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
      DownloadFileType::NOT_DANGEROUS,
      "",
      "text/plain",
      FILE_PATH_LITERAL(""),

      FILE_PATH_LITERAL("download.txt"),
      DownloadItem::TARGET_DISPOSITION_PROMPT,

      EXPECT_CRDOWNLOAD};

  SetManagedDownloadPath(test_download_dir());
  ASSERT_TRUE(download_prefs()->IsDownloadPathManaged());
  EXPECT_CALL(*delegate(),
              RequestConfirmation_(
                  _, GetPathInDownloadDir(FILE_PATH_LITERAL("download.txt")),
                  DownloadConfirmationReason::DLP_BLOCKED, _));
  EXPECT_CALL(*mock_files_controller_, ShouldPromptBeforeDownload).Times(0);
  RunTestCasesWithActiveItem(&kManagedPathTestCase, 1);
}

// Even if the download path is managed, we should prompt if the download path
// is blocked by DLP.
TEST_F(DownloadTargetDeterminerDlpTest, ManagedPath_ShouldPrompt) {
  SetupRulesManager();

  const DownloadTestCase kManagedPathTestCase = {
      AUTOMATIC,
      download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
      DownloadFileType::NOT_DANGEROUS,
      "http://example.com/foo.txt",
      "text/plain",
      FILE_PATH_LITERAL(""),

      FILE_PATH_LITERAL("foo.txt"),
      DownloadItem::TARGET_DISPOSITION_PROMPT,

      EXPECT_CRDOWNLOAD};

  SetManagedDownloadPath(test_download_dir());
  ASSERT_TRUE(download_prefs()->IsDownloadPathManaged());
  EXPECT_CALL(*delegate(),
              RequestConfirmation_(
                  _, GetPathInDownloadDir(FILE_PATH_LITERAL("foo.txt")),
                  DownloadConfirmationReason::DLP_BLOCKED, _));
  EXPECT_CALL(*mock_files_controller_, ShouldPromptBeforeDownload)
      .WillOnce(testing::Return(true));
  RunTestCasesWithActiveItem(&kManagedPathTestCase, 1);
}

// Even if "Prompt for download" user preference is set to false, we should
// prompt if the download path is blocked by DLP.
TEST_F(DownloadTargetDeterminerDlpTest, PromptAlways_SafeAutomatic) {
  SetupRulesManager();

  const DownloadTestCase kSafeAutomatic = {
      AUTOMATIC,
      download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
      DownloadFileType::NOT_DANGEROUS,
      "http://example.com/automatic.txt",
      "text/plain",
      FILE_PATH_LITERAL(""),

      FILE_PATH_LITERAL("automatic.txt"),
      DownloadItem::TARGET_DISPOSITION_PROMPT,

      EXPECT_CRDOWNLOAD};

  SetPromptForDownload(false);
  EXPECT_CALL(*mock_files_controller_, ShouldPromptBeforeDownload)
      .WillOnce(testing::Return(true));
  EXPECT_CALL(*delegate(),
              RequestConfirmation_(
                  _, GetPathInDownloadDir(FILE_PATH_LITERAL("automatic.txt")),
                  DownloadConfirmationReason::DLP_BLOCKED, _));
  RunTestCasesWithActiveItem(&kSafeAutomatic, 1);
}
#endif

}  // namespace
