// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/webrtc/webrtc_event_log_manager_common.h"

#include <memory>
#include <numeric>
#include <optional>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/notreached.h"
#include "base/rand_util.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/media/webrtc/webrtc_event_log_manager_unittest_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/zlib/google/compression_utils.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/test/base/testing_profile.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/user_names.h"
#include "content/public/test/browser_task_environment.h"
#endif

namespace webrtc_event_logging {

namespace {
constexpr LogCompressor::Result OK = LogCompressor::Result::OK;
constexpr LogCompressor::Result DISALLOWED = LogCompressor::Result::DISALLOWED;
constexpr LogCompressor::Result ERROR_ENCOUNTERED =
    LogCompressor::Result::ERROR_ENCOUNTERED;
}  // namespace

// Tests for GzipLogCompressor.
// Note that these tests may not use GzippedSize(), or they would be assuming
// what they set out to prove. (Subsequent tests may use it, though.)
class GzipLogCompressorTest : public ::testing::Test {
 public:
  ~GzipLogCompressorTest() override = default;

  void Init(
      std::unique_ptr<CompressedSizeEstimator::Factory> estimator_factory) {
    DCHECK(!compressor_factory_);
    DCHECK(estimator_factory);
    compressor_factory_ = std::make_unique<GzipLogCompressorFactory>(
        std::move(estimator_factory));
  }

  std::string Decompress(const std::string& input) {
    std::string output;
    EXPECT_TRUE(compression::GzipUncompress(input, &output));
    return output;
  }

  std::unique_ptr<GzipLogCompressorFactory> compressor_factory_;
};

TEST_F(GzipLogCompressorTest,
       GzipLogCompressorFactoryCreatesCompressorIfMinimalSizeOrAbove) {
  Init(std::make_unique<PerfectGzipEstimator::Factory>());
  const size_t min_size = compressor_factory_->MinSizeBytes();
  auto compressor = compressor_factory_->Create(min_size);
  EXPECT_TRUE(compressor);
}

TEST_F(GzipLogCompressorTest,
       GzipLogCompressorFactoryDoesNotCreateCompressorIfBelowMinimalSize) {
  Init(std::make_unique<PerfectGzipEstimator::Factory>());
  const size_t min_size = compressor_factory_->MinSizeBytes();
  ASSERT_GE(min_size, 1u);
  auto compressor = compressor_factory_->Create(min_size - 1);
  EXPECT_FALSE(compressor);
}

TEST_F(GzipLogCompressorTest, EmptyStreamReasonableMaxSize) {
  Init(std::make_unique<PerfectGzipEstimator::Factory>());

  auto compressor = compressor_factory_->Create(kMaxRemoteLogFileSizeBytes);
  ASSERT_TRUE(compressor);

  std::string header;
  compressor->CreateHeader(&header);

  std::string footer;
  ASSERT_TRUE(compressor->CreateFooter(&footer));

  const std::string simulated_file = header + footer;
  EXPECT_EQ(Decompress(simulated_file), std::string());
}

TEST_F(GzipLogCompressorTest, EmptyStreamMinimalSize) {
  Init(std::make_unique<PerfectGzipEstimator::Factory>());

  const size_t min_size = compressor_factory_->MinSizeBytes();
  auto compressor = compressor_factory_->Create(min_size);
  ASSERT_TRUE(compressor);

  std::string header;
  compressor->CreateHeader(&header);

  std::string footer;
  ASSERT_TRUE(compressor->CreateFooter(&footer));

  const std::string simulated_file = header + footer;
  EXPECT_EQ(Decompress(simulated_file), std::string());
}

TEST_F(GzipLogCompressorTest, SingleCallToCompress) {
  Init(std::make_unique<PerfectGzipEstimator::Factory>());

  auto compressor = compressor_factory_->Create(kMaxRemoteLogFileSizeBytes);
  ASSERT_TRUE(compressor);

  std::string header;
  compressor->CreateHeader(&header);

  const std::string input = "Some random text.";
  std::string log;
  ASSERT_EQ(compressor->Compress(input, &log), OK);

  std::string footer;
  ASSERT_TRUE(compressor->CreateFooter(&footer));

  const std::string simulated_file = header + log + footer;
  EXPECT_EQ(Decompress(simulated_file), input);
}

TEST_F(GzipLogCompressorTest, MultipleCallsToCompress) {
  Init(std::make_unique<PerfectGzipEstimator::Factory>());

  auto compressor = compressor_factory_->Create(kMaxRemoteLogFileSizeBytes);
  ASSERT_TRUE(compressor);

  std::string header;
  compressor->CreateHeader(&header);

  const std::vector<std::string> inputs = {
      "Some random text.",
      "This text is also random. I give you my word for it. 100% random.",
      "nejnnc pqmnx0981 mnl<D@ikjed90~~,z."};

  std::vector<std::string> logs(inputs.size());
  for (size_t i = 0; i < inputs.size(); i++) {
    ASSERT_EQ(compressor->Compress(inputs[i], &logs[i]), OK);
  }

  std::string footer;
  ASSERT_TRUE(compressor->CreateFooter(&footer));

  const auto input = std::accumulate(begin(inputs), end(inputs), std::string());
  const auto log = std::accumulate(begin(logs), end(logs), std::string());

  const std::string simulated_file = header + log + footer;
  EXPECT_EQ(Decompress(simulated_file), input);
}

TEST_F(GzipLogCompressorTest, UnlimitedBudgetSanity) {
  Init(std::make_unique<PerfectGzipEstimator::Factory>());

  auto compressor = compressor_factory_->Create(std::optional<size_t>());
  ASSERT_TRUE(compressor);

  std::string header;
  compressor->CreateHeader(&header);

  const std::string input = "Some random text.";
  std::string log;
  ASSERT_EQ(compressor->Compress(input, &log), OK);

  std::string footer;
  ASSERT_TRUE(compressor->CreateFooter(&footer));

  const std::string simulated_file = header + log + footer;
  EXPECT_EQ(Decompress(simulated_file), input);
}

// Test once with a big input, to provide coverage over inputs that could
// exceed the size of some local buffers in the UUT.
TEST_F(GzipLogCompressorTest, CompressionBigInput) {
  Init(std::make_unique<PerfectGzipEstimator::Factory>());

  auto compressor = compressor_factory_->Create(kMaxRemoteLogFileSizeBytes);
  ASSERT_TRUE(compressor);

  std::string header;
  compressor->CreateHeader(&header);

  constexpr size_t kRealisticSizeBytes = 1000 * 1000;
  const std::string input = base::RandBytesAsString(kRealisticSizeBytes);
  std::string log;
  ASSERT_EQ(compressor->Compress(input, &log), OK);

  std::string footer;
  ASSERT_TRUE(compressor->CreateFooter(&footer));

  const std::string simulated_file = header + log + footer;
  EXPECT_EQ(Decompress(simulated_file), input);
}

TEST_F(GzipLogCompressorTest, BudgetExceededByFirstCompressYieldsEmptyFile) {
  Init(std::make_unique<PerfectGzipEstimator::Factory>());

  const std::string input = "This won't fit.";

  auto compressor = compressor_factory_->Create(GzippedSize(input) - 1);
  ASSERT_TRUE(compressor);

  std::string header;
  compressor->CreateHeader(&header);

  // Focal point #1 - Compress() returns DISALLOWED.
  std::string log;
  EXPECT_EQ(compressor->Compress(input, &log), DISALLOWED);

  // Focal point #2 - CreateFooter() still succeeds;
  std::string footer;
  EXPECT_TRUE(compressor->CreateFooter(&footer));

  // Focal point #3 - the resulting log is parsable, and contains only those
  // logs for which Compress() was successful.
  // Note that |log| is not supposed to be written to the file, because
  // Compress() has disallowed it.
  const std::string simulated_file = header + footer;
  EXPECT_EQ(Decompress(simulated_file), std::string());
}

TEST_F(GzipLogCompressorTest,
       BudgetExceededByNonFirstCompressYieldsPartialFile) {
  Init(std::make_unique<PerfectGzipEstimator::Factory>());

  const std::string short_input = "short";
  const std::string long_input = "A somewhat longer input string. @$%^&*()!!2";

  // Allocate enough budget that |short_input| would be produced, and not yet
  // exhaust the budget, but |long_input| won't fit.
  auto compressor = compressor_factory_->Create(GzippedSize(short_input) + 1);
  ASSERT_TRUE(compressor);

  std::string header;
  compressor->CreateHeader(&header);

  std::string short_log;
  ASSERT_EQ(compressor->Compress(short_input, &short_log), OK);

  // Focal point #1 - Compress() returns DISALLOWED.
  std::string long_log;
  EXPECT_EQ(compressor->Compress(long_input, &long_log), DISALLOWED);
  EXPECT_TRUE(long_log.empty());

  // Focal point #2 - CreateFooter() still succeeds;
  std::string footer;
  EXPECT_TRUE(compressor->CreateFooter(&footer));

  // Focal point #3 - the resulting log is parsable, and contains only those
  // logs for which Compress() was successful.
  // Note that |long_log| is not supposed to be written to the file, because
  // Compress() has disallowed it.
  const std::string simulated_file = header + short_log + footer;
  EXPECT_EQ(Decompress(simulated_file), short_input);
}

TEST_F(GzipLogCompressorTest,
       ExceedingBudgetDueToOverlyOptimisticEstimationYieldsError) {
  // Use an estimator that will always be overly optimistic.
  Init(std::make_unique<NullEstimator::Factory>());

  // Set a budget that will easily be exceeded.
  auto compressor = compressor_factory_->Create(kGzipOverheadBytes + 5);
  ASSERT_TRUE(compressor);

  std::string header;
  compressor->CreateHeader(&header);

  // Prepare to compress an input that is guaranteed to exceed the budget.
  const std::string input = "A string that would not fit in five bytes.";

  // The estimation allowed the compression, but then the compressed output
  // ended up being over-budget.
  std::string compressed;
  EXPECT_EQ(compressor->Compress(input, &compressed), ERROR_ENCOUNTERED);
  EXPECT_TRUE(compressed.empty());
}

// Tests relevant to all LogFileWriter subclasses.
class LogFileWriterTest
    : public ::testing::Test,
      public ::testing::WithParamInterface<WebRtcEventLogCompression> {
 public:
  LogFileWriterTest() { EXPECT_TRUE(temp_dir_.CreateUniqueTempDir()); }

  ~LogFileWriterTest() override {}

  void Init(WebRtcEventLogCompression compression) {
    DCHECK(!compression_.has_value()) << "Must only be called once.";
    compression_ = compression;
    log_file_writer_factory_ = CreateLogFileWriterFactory(compression);
    path_ = temp_dir_.GetPath()
                .Append(FILE_PATH_LITERAL("arbitrary_filename"))
                .AddExtension(log_file_writer_factory_->Extension());
  }

  std::unique_ptr<LogFileWriter> CreateWriter(std::optional<size_t> max_size) {
    return log_file_writer_factory_->Create(path_, max_size);
  }

  void ExpectFileContents(const base::FilePath& file_path,
                          const std::string& expected_contents) {
    DCHECK(compression_.has_value()) << "Must call Init().";

    std::string file_contents;
    ASSERT_TRUE(base::ReadFileToString(file_path, &file_contents));

    switch (compression_.value()) {
      case WebRtcEventLogCompression::NONE: {
        EXPECT_EQ(file_contents, expected_contents);
        break;
      }
      case WebRtcEventLogCompression::GZIP_PERFECT_ESTIMATION:
      case WebRtcEventLogCompression::GZIP_NULL_ESTIMATION: {
        std::string uncompressed;
        ASSERT_TRUE(compression::GzipUncompress(file_contents, &uncompressed));
        EXPECT_EQ(uncompressed, expected_contents);
        break;
      }
      default: {
        NOTREACHED_IN_MIGRATION();
      }
    }
  }

  base::test::TaskEnvironment task_environment_;
  std::optional<WebRtcEventLogCompression> compression_;  // Set in Init().
  base::ScopedTempDir temp_dir_;
  base::FilePath path_;
  std::unique_ptr<LogFileWriter::Factory> log_file_writer_factory_;
};

TEST_P(LogFileWriterTest, FactoryCreatesLogFileWriter) {
  Init(GetParam());
  EXPECT_TRUE(CreateWriter(log_file_writer_factory_->MinFileSizeBytes()));
}

#if BUILDFLAG(IS_POSIX)
TEST_P(LogFileWriterTest, FactoryReturnsEmptyUniquePtrIfCantCreateFile) {
  Init(GetParam());
  RemoveWritePermissions(temp_dir_.GetPath());
  auto writer = CreateWriter(kMaxRemoteLogFileSizeBytes);
  EXPECT_FALSE(writer);
}
#endif  // BUILDFLAG(IS_POSIX)

TEST_P(LogFileWriterTest, CloseSucceedsWhenNoErrorsOccurred) {
  Init(GetParam());

  auto writer = CreateWriter(kMaxRemoteLogFileSizeBytes);
  ASSERT_TRUE(writer);

  EXPECT_TRUE(writer->Close());
}

// Other tests check check the case of compression where the estimation is
// close to the file's capacity, reaches or exceeds it.
TEST_P(LogFileWriterTest, CallToWriteSuccedsWhenCapacityFarOff) {
  Init(GetParam());

  auto writer = CreateWriter(kMaxRemoteLogFileSizeBytes);
  ASSERT_TRUE(writer);

  const std::string log = "log";
  EXPECT_TRUE(writer->Write(log));

  ASSERT_TRUE(writer->Close());
  ExpectFileContents(path_, log);
}

TEST_P(LogFileWriterTest, CallToWriteWithEmptyStringSucceeds) {
  Init(GetParam());

  auto writer = CreateWriter(kMaxRemoteLogFileSizeBytes);
  ASSERT_TRUE(writer);

  const std::string log;
  EXPECT_TRUE(writer->Write(log));

  ASSERT_TRUE(writer->Close());
  ExpectFileContents(path_, log);
}

TEST_P(LogFileWriterTest, UnlimitedBudgetSanity) {
  Init(GetParam());

  auto writer = CreateWriter(std::optional<size_t>());
  ASSERT_TRUE(writer);

  const std::string log = "log";
  EXPECT_TRUE(writer->Write(log));

  ASSERT_TRUE(writer->Close());
  ExpectFileContents(path_, log);
}

TEST_P(LogFileWriterTest, DeleteRemovesUnclosedFile) {
  Init(GetParam());

  auto writer = CreateWriter(kMaxRemoteLogFileSizeBytes);
  ASSERT_TRUE(writer);

  writer->Delete();
  EXPECT_FALSE(base::PathExists(path_));
}

TEST_P(LogFileWriterTest, DeleteRemovesClosedFile) {
  Init(GetParam());

  auto writer = CreateWriter(kMaxRemoteLogFileSizeBytes);
  ASSERT_TRUE(writer);

  EXPECT_TRUE(writer->Close());

  writer->Delete();
  EXPECT_FALSE(base::PathExists(path_));
}

#if !BUILDFLAG(IS_WIN)  // Deleting the open file does not work on Windows.
TEST_P(LogFileWriterTest, WriteDoesNotCrashIfFileRemovedExternally) {
  Init(GetParam());

  auto writer = CreateWriter(kMaxRemoteLogFileSizeBytes);
  ASSERT_TRUE(writer);

  ASSERT_TRUE(base::DeleteFile(path_));
  ASSERT_FALSE(base::PathExists(path_));  // Sanity on the test itself.

  // It's up to the OS whether this will succeed or fail, but it must not crash.
  writer->Write("log");
}

TEST_P(LogFileWriterTest, CloseDoesNotCrashIfFileRemovedExternally) {
  Init(GetParam());

  auto writer = CreateWriter(kMaxRemoteLogFileSizeBytes);
  ASSERT_TRUE(writer);

  ASSERT_TRUE(base::DeleteFile(path_));
  ASSERT_FALSE(base::PathExists(path_));  // Sanity on the test itself.

  // It's up to the OS whether this will succeed or fail, but it must not crash.
  writer->Close();
}

TEST_P(LogFileWriterTest, DeleteDoesNotCrashIfFileRemovedExternally) {
  Init(GetParam());

  auto writer = CreateWriter(kMaxRemoteLogFileSizeBytes);
  ASSERT_TRUE(writer);

  ASSERT_TRUE(base::DeleteFile(path_));
  ASSERT_FALSE(base::PathExists(path_));  // Sanity on the test itself.

  // It's up to the OS whether this will succeed or fail, but it must not crash.
  writer->Delete();
}
#endif  // !BUILDFLAG(IS_WIN)

TEST_P(LogFileWriterTest, PathReturnsTheCorrectPath) {
  Init(GetParam());

  auto writer = CreateWriter(kMaxRemoteLogFileSizeBytes);
  ASSERT_TRUE(writer);

  ASSERT_EQ(writer->path(), path_);
}

INSTANTIATE_TEST_SUITE_P(
    Compression,
    LogFileWriterTest,
    ::testing::Values(WebRtcEventLogCompression::NONE,
                      WebRtcEventLogCompression::GZIP_PERFECT_ESTIMATION));

// Tests for UncompressedLogFileWriterTest only.
class UncompressedLogFileWriterTest : public LogFileWriterTest {
 public:
  ~UncompressedLogFileWriterTest() override = default;
};

TEST_F(UncompressedLogFileWriterTest,
       MaxSizeReachedReturnsFalseWhenMaxNotReached) {
  Init(WebRtcEventLogCompression::NONE);

  auto writer = CreateWriter(kMaxRemoteLogFileSizeBytes);
  ASSERT_TRUE(writer);

  const std::string log = "log";
  ASSERT_TRUE(writer->Write(log));

  EXPECT_FALSE(writer->MaxSizeReached());
}

TEST_F(UncompressedLogFileWriterTest, MaxSizeReachedReturnsTrueWhenMaxReached) {
  Init(WebRtcEventLogCompression::NONE);

  const std::string log = "log";

  auto writer = CreateWriter(log.size());
  ASSERT_TRUE(writer);

  ASSERT_TRUE(writer->Write(log));  // (CallToWriteSuccedsWhenCapacityReached)

  EXPECT_TRUE(writer->MaxSizeReached());
}

TEST_F(UncompressedLogFileWriterTest, CallToWriteSuccedsWhenCapacityReached) {
  Init(WebRtcEventLogCompression::NONE);

  const std::string log = "log";

  auto writer = CreateWriter(log.size());
  ASSERT_TRUE(writer);

  EXPECT_TRUE(writer->Write(log));

  ASSERT_TRUE(writer->Close());
  ExpectFileContents(path_, log);
}

TEST_F(UncompressedLogFileWriterTest, CallToWriteFailsWhenCapacityExceeded) {
  Init(WebRtcEventLogCompression::NONE);

  const std::string log = "log";

  auto writer = CreateWriter(log.size() - 1);
  ASSERT_TRUE(writer);

  EXPECT_FALSE(writer->Write(log));

  ASSERT_TRUE(writer->Close());
  ExpectFileContents(path_, std::string());
}

TEST_F(UncompressedLogFileWriterTest, WriteCompleteMessagesOnly) {
  Init(WebRtcEventLogCompression::NONE);

  const std::string log1 = "01234";
  const std::string log2 = "56789";

  auto writer = CreateWriter(log1.size() + log2.size() - 1);
  ASSERT_TRUE(writer);

  EXPECT_TRUE(writer->Write(log1));

  EXPECT_FALSE(writer->Write(log2));

  ASSERT_TRUE(writer->Close());
  ExpectFileContents(path_, log1);
}

// Tests for GzippedLogFileWriterTest only.
class GzippedLogFileWriterTest : public LogFileWriterTest {
 public:
  ~GzippedLogFileWriterTest() override = default;
};

TEST_F(GzippedLogFileWriterTest, FactoryDeletesFileIfMaxSizeBelowMin) {
  Init(WebRtcEventLogCompression::GZIP_NULL_ESTIMATION);

  const size_t min_size = log_file_writer_factory_->MinFileSizeBytes();
  ASSERT_GE(min_size, 1u);

  auto writer = CreateWriter(min_size - 1);
  ASSERT_FALSE(writer);

  EXPECT_FALSE(base::PathExists(path_));
}

TEST_F(GzippedLogFileWriterTest, MaxSizeReachedReturnsFalseWhenMaxNotReached) {
  Init(WebRtcEventLogCompression::GZIP_NULL_ESTIMATION);

  auto writer = CreateWriter(kMaxRemoteLogFileSizeBytes);
  ASSERT_TRUE(writer);

  const std::string log = "log";
  ASSERT_TRUE(writer->Write(log));
  EXPECT_FALSE(writer->MaxSizeReached());
}

TEST_F(GzippedLogFileWriterTest, MaxSizeReachedReturnsTrueWhenMaxReached) {
  // By using a 0 estimation, we allow the compressor to keep going to
  // the point of budget saturation.
  Init(WebRtcEventLogCompression::GZIP_NULL_ESTIMATION);

  const std::string log = "log";

  auto writer = CreateWriter(GzippedSize(log));
  ASSERT_TRUE(writer);

  ASSERT_TRUE(writer->Write(log));  // (CallToWriteSuccedsWhenCapacityReached)
  EXPECT_TRUE(writer->MaxSizeReached());
}

TEST_F(GzippedLogFileWriterTest, CallToWriteSuccedsWhenCapacityReached) {
  Init(WebRtcEventLogCompression::GZIP_PERFECT_ESTIMATION);

  const std::string log = "log";

  auto writer = CreateWriter(GzippedSize(log));
  ASSERT_TRUE(writer);

  EXPECT_TRUE(writer->Write(log));

  ASSERT_TRUE(writer->Close());
  ExpectFileContents(path_, log);
}

// Also tests the scenario WriteCompleteMessagesOnly.
TEST_F(GzippedLogFileWriterTest,
       CallToWriteFailsWhenCapacityWouldBeExceededButEstimationPreventedWrite) {
  Init(WebRtcEventLogCompression::GZIP_PERFECT_ESTIMATION);

  const std::string log1 = "abcde";
  const std::string log2 = "fghij";
  const std::vector<std::string> logs = {log1, log2};

  // Find out the size necessary for compressing log1 and log2 in two calls.
  const size_t compressed_len = GzippedSize(logs);  // Vector version.

  auto writer = CreateWriter(compressed_len - 1);
  ASSERT_TRUE(writer);

  ASSERT_TRUE(writer->Write(log1));

  EXPECT_FALSE(writer->Write(log2));

  // The second write was succesfully prevented; no error should have occurred,
  // and it should be possible to produce a meaningful gzipped log file.
  EXPECT_TRUE(writer->Close());

  ExpectFileContents(path_, log1);  // Only the in-budget part was written.
}

// This tests the case when the estimation fails to warn us of a pending
// over-budget write, which leaves us unable to produce a valid compression
// footer for the truncated file. This forces us to discard the file.
TEST_F(GzippedLogFileWriterTest,
       CallToWriteFailsWhenCapacityExceededDespiteEstimationAllowingIt) {
  // By using a 0 estimation, we allow the compressor to keep going to
  // the point of budget saturation.
  Init(WebRtcEventLogCompression::GZIP_NULL_ESTIMATION);

  const std::string log = "log";

  auto writer = CreateWriter(GzippedSize(log) - 1);
  ASSERT_TRUE(writer);

  EXPECT_FALSE(writer->Write(log));

  EXPECT_FALSE(writer->Close());
  EXPECT_FALSE(base::PathExists(path_));  // Errored files deleted by Close().
}

#if BUILDFLAG(IS_CHROMEOS_ASH)

struct DoesProfileDefaultToLoggingEnabledForUserTypeTestCase {
  user_manager::UserType user_type;
  bool defaults_to_logging_enabled;
};

class DoesProfileDefaultToLoggingEnabledForUserTypeParametrizedTest
    : public ::testing::TestWithParam<
          DoesProfileDefaultToLoggingEnabledForUserTypeTestCase> {
 protected:
  content::BrowserTaskEnvironment task_environment_;
};

TEST_P(DoesProfileDefaultToLoggingEnabledForUserTypeParametrizedTest,
       WebRtcPolicyDefaultTest) {
  DoesProfileDefaultToLoggingEnabledForUserTypeTestCase test_case = GetParam();

  TestingProfile::Builder profile_builder;
  profile_builder.OverridePolicyConnectorIsManagedForTesting(true);
  std::unique_ptr<TestingProfile> testing_profile = profile_builder.Build();
  auto fake_user_manager_ = std::make_unique<ash::FakeChromeUserManager>();
  // We use a standard Gaia account by default:
  AccountId account_id = AccountId::FromUserEmailGaiaId("name", "id");

  switch (test_case.user_type) {
    case user_manager::UserType::kRegular:
      fake_user_manager_->AddUserWithAffiliationAndTypeAndProfile(
          account_id, false, test_case.user_type, testing_profile.get());
      break;
    case user_manager::UserType::kGuest:
      account_id = user_manager::GuestAccountId();
      fake_user_manager_->AddGuestUser();
      break;
    case user_manager::UserType::kPublicAccount:
      fake_user_manager_->AddPublicAccountUser(account_id);
      break;
    case user_manager::UserType::kKioskApp:
      fake_user_manager_->AddKioskAppUser(account_id);
      break;
    case user_manager::UserType::kChild:
      fake_user_manager_->AddChildUser(account_id);
      break;
    default:
      FAIL() << "Invalid test setup. Unexpected user type.";
  }

  fake_user_manager_->LoginUser(account_id);
  std::unique_ptr<user_manager::ScopedUserManager> scoped_user_manager_ =
      std::make_unique<user_manager::ScopedUserManager>(
          std::move(fake_user_manager_));

  EXPECT_EQ(DoesProfileDefaultToLoggingEnabled(testing_profile.get()),
            test_case.defaults_to_logging_enabled);
}

INSTANTIATE_TEST_SUITE_P(
    WebRtcPolicyDefaultTests,
    DoesProfileDefaultToLoggingEnabledForUserTypeParametrizedTest,
    testing::ValuesIn(
        std::vector<DoesProfileDefaultToLoggingEnabledForUserTypeTestCase>{
            {user_manager::UserType::kRegular, true},
            {user_manager::UserType::kGuest, false},
            {user_manager::UserType::kPublicAccount, false},
            {user_manager::UserType::kKioskApp, false},
            {user_manager::UserType::kChild, false},
        }));

#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

}  // namespace webrtc_event_logging
