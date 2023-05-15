// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/offline_pages/offline_page_mhtml_archiver.h"

#include <stdint.h>
#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/common/chrome_paths.h"
#include "components/offline_pages/core/client_namespace_constants.h"
#include "components/offline_pages/core/model/offline_page_model_utils.h"
#include "components/offline_pages/core/offline_clock.h"
#include "components/offline_pages/core/test_scoped_offline_clock.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace offline_pages {

namespace {

const char kTestURL[] = "http://example.com/hello.mhtml";
const char kNonExistentURL[] = "http://example.com/non_existent.mhtml";
// Size of chrome/test/data/offline_pages/hello.mhtml
const int64_t kTestFileSize = 471LL;
const std::u16string kTestTitle = u"a title";
// SHA256 Hash of chrome/test/data/offline_pages/hello.mhtml
const std::string kTestDigest(
    "\x43\x60\x62\x02\x06\x15\x0f\x3e\x77\x99\x3d\xed\xdc\xd4\xe2\x0d\xbe\xbd"
    "\x77\x1a\xfb\x32\x00\x51\x7e\x63\x7d\x3b\x2e\x46\x63\xf6",
    32);

constexpr base::TimeDelta kTimeToSaveMhtml = base::Milliseconds(1000);
constexpr base::TimeDelta kTimeToComputeDigest = base::Milliseconds(10);

class TestMHTMLArchiver : public OfflinePageMHTMLArchiver {
 public:
  enum class TestScenario {
    SUCCESS,
    NOT_ABLE_TO_ARCHIVE,
    WEB_CONTENTS_MISSING,
  };

  TestMHTMLArchiver(const GURL& url,
                    const TestScenario test_scenario,
                    TestScopedOfflineClock* clock);

  TestMHTMLArchiver(const TestMHTMLArchiver&) = delete;
  TestMHTMLArchiver& operator=(const TestMHTMLArchiver&) = delete;

  ~TestMHTMLArchiver() override;

 private:
  void GenerateMHTML(const base::FilePath& archives_dir,
                     content::WebContents* web_contents,
                     const CreateArchiveParams& create_archive_params) override;

  const GURL url_;
  const TestScenario test_scenario_;
  // Not owned.
  raw_ptr<TestScopedOfflineClock> clock_;
};

TestMHTMLArchiver::TestMHTMLArchiver(const GURL& url,
                                     const TestScenario test_scenario,
                                     TestScopedOfflineClock* clock)
    : url_(url), test_scenario_(test_scenario), clock_(clock) {}

TestMHTMLArchiver::~TestMHTMLArchiver() {
}

void TestMHTMLArchiver::GenerateMHTML(
    const base::FilePath& archives_dir,
    content::WebContents* web_contents,
    const CreateArchiveParams& create_archive_params) {
  if (test_scenario_ == TestScenario::WEB_CONTENTS_MISSING) {
    ReportFailure(ArchiverResult::ERROR_CONTENT_UNAVAILABLE);
    return;
  }

  if (test_scenario_ == TestScenario::NOT_ABLE_TO_ARCHIVE) {
    ReportFailure(ArchiverResult::ERROR_ARCHIVE_CREATION_FAILED);
    return;
  }

  EXPECT_EQ(kDownloadNamespace, create_archive_params.name_space);
  base::FilePath archive_file_path =
      archives_dir.AppendASCII(url_.ExtractFileName());
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&TestMHTMLArchiver::OnGenerateMHTMLDone,
                     base::Unretained(this), url_, archive_file_path,
                     kTestTitle, create_archive_params.name_space,
                     OfflineTimeNow(),
                     content::MHTMLGenerationResult(kTestFileSize, nullptr)));

  clock_->Advance(kTimeToSaveMhtml);
}

}  // namespace

class OfflinePageMHTMLArchiverTest : public testing::Test {
 public:
  OfflinePageMHTMLArchiverTest();

  OfflinePageMHTMLArchiverTest(const OfflinePageMHTMLArchiverTest&) = delete;
  OfflinePageMHTMLArchiverTest& operator=(const OfflinePageMHTMLArchiverTest&) =
      delete;

  ~OfflinePageMHTMLArchiverTest() override;

  void SetUp() override;

  // Creates an archiver for testing scenario and uses it to create an archive.
  void CreateArchive(const GURL& url, TestMHTMLArchiver::TestScenario scenario);

  // Test tooling methods.
  void PumpLoop();
  void WaitForAsyncOperation();

  base::FilePath GetTestFilePath(const GURL& url) const {
    return archive_dir_path_.AppendASCII(url.ExtractFileName());
  }
  base::HistogramTester* histogram_tester() { return &histogram_tester_; }

  OfflinePageArchiver::ArchiverResult last_result() const {
    return last_result_;
  }
  const base::FilePath& last_file_path() const { return last_file_path_; }
  int64_t last_file_size() const { return last_file_size_; }
  const std::string& last_digest() const { return last_digest_; }

  OfflinePageArchiver::CreateArchiveCallback callback() {
    return base::BindOnce(&OfflinePageMHTMLArchiverTest::OnCreateArchiveDone,
                          base::Unretained(this));
  }

 private:
  void OnCreateArchiveDone(OfflinePageArchiver::ArchiverResult result,
                           const GURL& url,
                           const base::FilePath& file_path,
                           const std::u16string& title,
                           int64_t file_size,
                           const std::string& digest);

  content::BrowserTaskEnvironment task_environment_;
  base::FilePath archive_dir_path_;
  base::HistogramTester histogram_tester_;

  OfflinePageArchiver::ArchiverResult last_result_;
  GURL last_url_;
  base::FilePath last_file_path_;
  int64_t last_file_size_;
  std::string last_digest_;
  bool async_operation_completed_ = false;
  base::OnceClosure async_operation_completed_callback_;

  TestScopedOfflineClock clock_;
};

OfflinePageMHTMLArchiverTest::OfflinePageMHTMLArchiverTest()
    : task_environment_(content::BrowserTaskEnvironment::REAL_IO_THREAD),
      last_result_(OfflinePageArchiver::ArchiverResult::ERROR_DEVICE_FULL),
      last_file_size_(0L) {}

OfflinePageMHTMLArchiverTest::~OfflinePageMHTMLArchiverTest() {
}

void OfflinePageMHTMLArchiverTest::SetUp() {
  base::FilePath test_data_dir_path;
  ASSERT_TRUE(
      base::PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir_path));
  archive_dir_path_ = test_data_dir_path.AppendASCII("offline_pages");
  clock_.SetNow(base::Time::Now());
}

void OfflinePageMHTMLArchiverTest::CreateArchive(
    const GURL& url,
    TestMHTMLArchiver::TestScenario scenario) {
  TestMHTMLArchiver archiver(url, scenario, &clock_);
  archiver.CreateArchive(
      archive_dir_path_,
      OfflinePageArchiver::CreateArchiveParams(kDownloadNamespace), nullptr,
      callback());
  PumpLoop();
  clock_.Advance(kTimeToComputeDigest);
  WaitForAsyncOperation();
}

void OfflinePageMHTMLArchiverTest::OnCreateArchiveDone(
    OfflinePageArchiver::ArchiverResult result,
    const GURL& url,
    const base::FilePath& file_path,
    const std::u16string& title,
    int64_t file_size,
    const std::string& digest) {
  DCHECK(!async_operation_completed_);
  async_operation_completed_ = true;
  last_url_ = url;
  last_result_ = result;
  last_file_path_ = file_path;
  last_file_size_ = file_size;
  last_digest_ = digest;
  if (!async_operation_completed_callback_.is_null())
    std::move(async_operation_completed_callback_).Run();
}

void OfflinePageMHTMLArchiverTest::PumpLoop() {
  base::RunLoop().RunUntilIdle();
}

void OfflinePageMHTMLArchiverTest::WaitForAsyncOperation() {
  // No need to wait if async operation is not needed.
  if (async_operation_completed_)
    return;
  base::RunLoop run_loop;
  async_operation_completed_callback_ = run_loop.QuitClosure();
  run_loop.Run();
}

// Tests that creation of an archiver fails when web contents is missing.
TEST_F(OfflinePageMHTMLArchiverTest, WebContentsMissing) {
  GURL page_url = GURL(kTestURL);
  CreateArchive(page_url,
                TestMHTMLArchiver::TestScenario::WEB_CONTENTS_MISSING);

  EXPECT_EQ(OfflinePageArchiver::ArchiverResult::ERROR_CONTENT_UNAVAILABLE,
            last_result());
  EXPECT_EQ(base::FilePath(), last_file_path());
}

// Tests for archiver failing save an archive.
TEST_F(OfflinePageMHTMLArchiverTest, NotAbleToGenerateArchive) {
  GURL page_url = GURL(kTestURL);
  CreateArchive(page_url, TestMHTMLArchiver::TestScenario::NOT_ABLE_TO_ARCHIVE);

  EXPECT_EQ(OfflinePageArchiver::ArchiverResult::ERROR_ARCHIVE_CREATION_FAILED,
            last_result());
  EXPECT_EQ(base::FilePath(), last_file_path());
  EXPECT_EQ(0LL, last_file_size());
}

// Tests for failing to compute digest for archive file.
TEST_F(OfflinePageMHTMLArchiverTest, DigestError) {
  GURL page_url = GURL(kNonExistentURL);
  CreateArchive(page_url, TestMHTMLArchiver::TestScenario::SUCCESS);

  EXPECT_EQ(
      OfflinePageArchiver::ArchiverResult::ERROR_DIGEST_CALCULATION_FAILED,
      last_result());
  EXPECT_EQ(base::FilePath(), last_file_path());
  EXPECT_EQ(0LL, last_file_size());
}

// Tests for successful creation of the offline page archive.
TEST_F(OfflinePageMHTMLArchiverTest, SuccessfullyCreateOfflineArchive) {
  GURL page_url = GURL(kTestURL);
  CreateArchive(page_url, TestMHTMLArchiver::TestScenario::SUCCESS);

  EXPECT_EQ(OfflinePageArchiver::ArchiverResult::SUCCESSFULLY_CREATED,
            last_result());
  EXPECT_EQ(GetTestFilePath(page_url), last_file_path());
  EXPECT_EQ(kTestFileSize, last_file_size());
  EXPECT_EQ(kTestDigest, last_digest());
}

}  // namespace offline_pages
