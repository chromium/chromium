// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/chromebox_for_meetings/artemis/local_data_source.h"

#include "base/logging.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/re2/src/re2/re2.h"

namespace ash::cfm {
namespace {

// Local convenience aliases
using mojom::DataFilter::FilterType::CHANGE;
using mojom::DataFilter::FilterType::REGEX;

constexpr std::string kDataSourceName = "fake_display_name";

// These are all arguments passed to the |LocalDataSource| object.
// Add them as named variables for clarity.
constexpr base::TimeDelta kPollFrequency = base::Seconds(0);
constexpr bool kRedactData = true;
constexpr bool kDoNotRedactData = false;
constexpr bool kIsIncremental = true;
constexpr bool kIsNotIncremental = false;

// Define callbacks to use for verification
void FetchCallbackFn(const std::vector<std::string>& expected_data,
                     const std::vector<std::string>& actual_data) {
  EXPECT_EQ(expected_data, actual_data);
}

[[maybe_unused]] void FetchCallbackFnWithRegex(
    const std::vector<std::string>& expected_data,
    const std::vector<std::string>& actual_data) {
  EXPECT_EQ(expected_data.size(), actual_data.size());
  for (size_t i = 0; i < expected_data.size(); i++) {
    EXPECT_TRUE(RE2::FullMatch(actual_data[i], expected_data[i]));
  }
}

void AddWatchDogCallbackFn(bool expected_success, bool actual_success) {
  EXPECT_EQ(expected_success, actual_success);
}

// Create a new DataWatchDog implementation that will help us control
// and check if callbacks have fired.
class LocalWatchDogPeer : public mojom::DataWatchDog {
 public:
  LocalWatchDogPeer(mojo::PendingReceiver<mojom::DataWatchDog> pending_receiver,
                    mojom::DataFilterPtr filter)
      : receiver_(this, std::move(pending_receiver)) {}
  LocalWatchDogPeer(const LocalWatchDogPeer&) = delete;
  LocalWatchDogPeer& operator=(const LocalWatchDogPeer&) = delete;

  bool DidCallbackFire() { return !callback_data_.empty(); }
  const std::string& GetCallbackData() { return callback_data_; }
  void Reset() { callback_data_.clear(); }

 protected:
  // mojom::DataWatchDog:
  void OnNotify(const std::string& data) override { callback_data_ = data; }

 private:
  mojo::Receiver<mojom::DataWatchDog> receiver_;
  std::string callback_data_;
};

// Subclass LocalDataSource so we can provide implementations
// for pure-virtuals and control data creation.
class LocalDataSourcePeer : public LocalDataSource {
 public:
  LocalDataSourcePeer(base::TimeDelta poll_rate,
                      bool data_needs_redacting,
                      bool is_incremental)
      : LocalDataSource(poll_rate, data_needs_redacting, is_incremental) {
    // The data that will be returned on the next call to GetNextData().
    next_data_ = "";

    // Note that we are explicitly omitting the call to StartPollTimer()
    // here to keep things simple. FillDataBuffer() will be called manually
    // instead of on a timer cadence.
  }

  LocalDataSourcePeer(const LocalDataSourcePeer&) = delete;
  LocalDataSourcePeer& operator=(const LocalDataSourcePeer&) = delete;

  // Give public access to FillDataBuffer method. Testers should call
  // FillDataBufferForTesting(<next_data>) to simulate data collection
  // into the internal buffer. Internally, FillDataBuffer() will call
  // GetNextData(), overridden below, which will append the provided
  // <next_data> to the buffer.
  void FillDataBufferForTesting(const std::string& new_data) {
    next_data_ = new_data;
    FillDataBuffer();
  }

  // Convenience overload that accepts multiple pieces of data.
  // NOTE: to avoid an ambiguous call with the function above,
  // this function must either be called with 3 or more pieces
  // of data, or with an explicitly initialized std::vector().
  void FillDataBufferForTesting(const std::vector<std::string>& new_data) {
    for (const auto& data : new_data) {
      FillDataBufferForTesting(data);
    }
  }

  // Give public access to BuildLogEntryFromLogLine() so we can
  // test the regex matching capabilities.
  void BuildLogEntryFromLogLineForTesting(
      proto::LogEntry& entry,
      const std::string& line,
      const uint64_t default_timestamp,
      const proto::LogSeverity& default_severity) {
    BuildLogEntryFromLogLine(entry, line, default_timestamp, default_severity);
  }

  // Add a wrapper around Fetch invocations for convenience. Use the callback
  // function defined above for data verification.
  void RunFetchWithExpectedData(const std::vector<std::string>& expected_data) {
    // Redirect callback to test function to verify that the data
    // received by the callback matches what we requested.
    auto callback = base::BindOnce(&FetchCallbackFn, expected_data);
    Fetch(std::move(callback));
  }

  void RunAddWatchDogWithExpectedResult(mojom::DataFilterPtr filter,
                                        bool expected_result) {
    // Redirect callback to test function to verify that the data
    // received by the callback matches what we requested.
    auto callback = base::BindOnce(&AddWatchDogCallbackFn, expected_result);
    mojo::PendingRemote<mojom::DataWatchDog> unused_remote;
    AddWatchDog(std::move(filter), std::move(unused_remote),
                std::move(callback));
  }

  void SetUpTestingWatchDog(mojom::DataFilterPtr filter) {
    mojo::PendingReceiver<mojom::DataWatchDog> receiver;
    auto remote = receiver.InitWithNewPipeAndPassRemote();
    auto watchdog = std::make_unique<LocalWatchDogPeer>(std::move(receiver),
                                                        filter.Clone());

    // Use DoNothing() for these callbacks and assume success.
    // We cover testing these callbacks elsewhere.
    AddWatchDog(std::move(filter), std::move(remote), base::DoNothing());
    watchdog_ = std::move(watchdog);
  }

  void AssertWatchDogCallbackFiredWithData(const std::string& expected_data) {
    const std::string& actual_data = watchdog_->GetCallbackData();
    EXPECT_EQ(expected_data, actual_data);
    watchdog_->Reset();
  }

  bool DidWatchDogCallbackFire() {
    bool result = watchdog_->DidCallbackFire();
    watchdog_->Reset();
    return result;
  }

 protected:
  const std::string& GetDisplayName() override { return kDataSourceName; }
  std::vector<std::string> GetNextData() override { return {next_data_}; }

  // Override this and avoid serialization as it greatly complicates testing
  void SerializeDataBuffer(std::vector<std::string>& buffer) override {}

 private:
  std::string next_data_;

  // Support only one watchdog for now, for easier testing
  std::unique_ptr<LocalWatchDogPeer> watchdog_;
};

// Define tests
TEST(ArtemisLocalDataSourceTest, TestFetchWithBasicUsage) {
  auto source =
      LocalDataSourcePeer(kPollFrequency, kDoNotRedactData, kIsIncremental);

  // No data in buffer yet, so expecting empty data
  source.RunFetchWithExpectedData({});

  source.FillDataBufferForTesting({"a", "b", "c"});
  source.RunFetchWithExpectedData({"a", "b", "c"});
}

TEST(ArtemisLocalDataSourceTest, TestNonIncrementalSource) {
  auto source =
      LocalDataSourcePeer(kPollFrequency, kDoNotRedactData, kIsNotIncremental);

  source.FillDataBufferForTesting({"aa", "aa", "b", "b", "bb", "aa"});

  // Verify we only get the unique data
  source.RunFetchWithExpectedData({"aa", "b", "bb", "aa"});
}

TEST(ArtemisLocalDataSourceTest, TestBufferSizeIsCapped) {
  auto source =
      LocalDataSourcePeer(kPollFrequency, kDoNotRedactData, kIsIncremental);

  std::vector<std::string> fake_data = {"a", "b", "c", "d", "e", "f"};

  // Fill buffer to the max limit
  for (int i = 0; i < kMaxInternalBufferSize; i++) {
    int index = i % fake_data.size();
    source.FillDataBufferForTesting(fake_data[index]);
  }

  // Now fill beyond that limit
  source.FillDataBufferForTesting({"a", "b", "c"});

  // Verify that returned data is capped at the limit. Also verify that
  // the beginning of the Fetch buffer is as expected, ie with older
  // data purged.
  auto callback =
      base::BindLambdaForTesting([&](const std::vector<std::string>& data) {
        EXPECT_EQ((int)data.size(), kMaxInternalBufferSize);
        EXPECT_EQ(data[0], "d");
      });
  source.Fetch(std::move(callback));
}

TEST(ArtemisLocalDataSourceTest, TestRedactionWorksAsExpected) {
  auto source = LocalDataSourcePeer(kPollFrequency, kRedactData,
                                    kIsIncremental);

  std::vector<std::string> fake_data = {"192.168.0.1", "fake@email.com"};

  source.FillDataBufferForTesting(fake_data);
  source.RunFetchWithExpectedData({"(192.168.0.0/16: 1)", "(email: 1)"});

  // Make a new source with redaction disabled
  auto new_source =
      LocalDataSourcePeer(kPollFrequency, kDoNotRedactData, kIsIncremental);

  new_source.FillDataBufferForTesting(fake_data);
  new_source.RunFetchWithExpectedData(fake_data);
}

TEST(ArtemisLocalDataSourceTest, TestTimestampAndSeverityParser) {
  // Test non-incremental sources first
  auto source =
      LocalDataSourcePeer(kPollFrequency, kRedactData, kIsNotIncremental);

  const std::string timestamp_str = "2000-01-01T22:34:56.789987Z";
  const std::string severity_str = "ERROR";
  const uint64_t timestamp = 946766096789987;  // us since epoch
  const proto::LogSeverity severity = proto::LOG_SEVERITY_ERROR;

  const std::string default_timestamp_str = "1970-01-01T00:00:00.000000Z";
  const std::string default_severity_str = "INFO";
  const uint64_t default_timestamp = 0;  // us since epoch
  const proto::LogSeverity default_severity = proto::LOG_SEVERITY_INFO;

  const std::string& text_payload = "fake log";

  proto::LogEntry entry;
  std::string log_line;

  // Test fully-formed log line
  log_line = timestamp_str + " " + severity_str + " " + text_payload;
  source.BuildLogEntryFromLogLineForTesting(entry, log_line, default_timestamp,
                                            default_severity);
  EXPECT_EQ(entry.timestamp_micros(), timestamp);
  EXPECT_EQ(entry.severity(), severity);
  EXPECT_EQ(entry.text_payload(), text_payload);

  // Test log line with missing severity
  log_line = timestamp_str + " " + text_payload;
  source.BuildLogEntryFromLogLineForTesting(entry, log_line, default_timestamp,
                                            default_severity);
  EXPECT_EQ(entry.timestamp_micros(), timestamp);
  EXPECT_EQ(entry.severity(), default_severity);
  EXPECT_EQ(entry.text_payload(), text_payload);

  // Test log line with missing timestamp
  log_line = severity_str + " " + text_payload;
  source.BuildLogEntryFromLogLineForTesting(entry, log_line, default_timestamp,
                                            default_severity);
  EXPECT_EQ(entry.timestamp_micros(), default_timestamp);
  EXPECT_EQ(entry.severity(), severity);
  EXPECT_EQ(entry.text_payload(), text_payload);

  // Test log line with text payload only
  log_line = text_payload;
  source.BuildLogEntryFromLogLineForTesting(entry, log_line, default_timestamp,
                                            default_severity);
  EXPECT_EQ(entry.timestamp_micros(), default_timestamp);
  EXPECT_EQ(entry.severity(), default_severity);
  EXPECT_EQ(entry.text_payload(), text_payload);

  // Instantiate a new incremental source and verify that timestamps
  // are "carried forward" for logs that contain newlines.
  auto source_incr =
      LocalDataSourcePeer(kPollFrequency, kRedactData, kIsIncremental);

  // Initial log line is normal, expect the provided timestamp.
  log_line = timestamp_str + " " + text_payload;
  source_incr.BuildLogEntryFromLogLineForTesting(
      entry, log_line, default_timestamp, default_severity);
  EXPECT_EQ(entry.timestamp_micros(), timestamp);
  EXPECT_EQ(entry.text_payload(), text_payload);

  // Next log line contains no timestamp, so it should inherit the
  // previously seen timestamp above, plus one microsecond.
  log_line = text_payload;
  source_incr.BuildLogEntryFromLogLineForTesting(
      entry, log_line, default_timestamp, default_severity);
  EXPECT_EQ(entry.timestamp_micros(), timestamp + 1);
  EXPECT_EQ(entry.text_payload(), text_payload);

  // Try one more line with no timestamp to verify incrementation.
  log_line = text_payload;
  source_incr.BuildLogEntryFromLogLineForTesting(
      entry, log_line, default_timestamp, default_severity);
  EXPECT_EQ(entry.timestamp_micros(), timestamp + 2);
  EXPECT_EQ(entry.text_payload(), text_payload);

  // Verify that a new line with a timestamp works as expected again.
  log_line = timestamp_str + " " + text_payload;
  source_incr.BuildLogEntryFromLogLineForTesting(
      entry, log_line, default_timestamp, default_severity);
  EXPECT_EQ(entry.timestamp_micros(), timestamp);
  EXPECT_EQ(entry.text_payload(), text_payload);
}

TEST(ArtemisWatchdogTest, TestVariousInvalidWatchdogs) {
  // All of these tests should fail
  bool expected_result = false;
  auto source =
      LocalDataSourcePeer(kPollFrequency, kDoNotRedactData, kIsNotIncremental);

  // CHANGE filter with supplied pattern
  auto filter = mojom::DataFilter::New(CHANGE, "pattern");
  source.RunAddWatchDogWithExpectedResult(std::move(filter), expected_result);

  // REGEX filter with empty pattern
  filter = mojom::DataFilter::New(REGEX, "");
  source.RunAddWatchDogWithExpectedResult(std::move(filter), expected_result);

  // REGEX filter with too-permissive pattern
  filter = mojom::DataFilter::New(REGEX, "*");
  source.RunAddWatchDogWithExpectedResult(std::move(filter), expected_result);

  // REGEX filter with invalid pattern
  filter = mojom::DataFilter::New(REGEX, "[a-z");
  source.RunAddWatchDogWithExpectedResult(std::move(filter), expected_result);

  // CHANGE filter for incremental source
  auto incr_source =
      LocalDataSourcePeer(kPollFrequency, kDoNotRedactData, kIsIncremental);
  filter = mojom::DataFilter::New(CHANGE, "");
  incr_source.RunAddWatchDogWithExpectedResult(std::move(filter),
                                               expected_result);
}

TEST(ArtemisWatchdogTest, TestChangeWatchdogsFireCorrectly) {
  base::test::TaskEnvironment task_environment;

  // Need non-incremental source for CHANGE watchdogs
  auto source =
      LocalDataSourcePeer(kPollFrequency, kDoNotRedactData, kIsNotIncremental);

  // Pre-fill the buffer with some data, then add the watchdog
  source.FillDataBufferForTesting("first");
  source.SetUpTestingWatchDog(mojom::DataFilter::New(CHANGE, std::nullopt));
  task_environment.RunUntilIdle();

  // Adding a CHANGE watchdog should trigger it immediately with
  // the last recorded data
  source.AssertWatchDogCallbackFiredWithData("first");

  source.FillDataBufferForTesting("second");
  task_environment.RunUntilIdle();

  // We went from "first" to "second", so the callback should have fired
  source.AssertWatchDogCallbackFiredWithData("second");

  // Add the same data
  source.FillDataBufferForTesting("second");
  task_environment.RunUntilIdle();

  // "second" to "second" again, no callback
  EXPECT_FALSE(source.DidWatchDogCallbackFire());

  // Add new data
  source.FillDataBufferForTesting("third");
  task_environment.RunUntilIdle();

  // "second" to "third", callback fired
  source.AssertWatchDogCallbackFiredWithData("third");
}

TEST(ArtemisWatchdogTest, TestRegexWatchdogsFireCorrectly) {
  base::test::TaskEnvironment task_environment;

  auto source =
      LocalDataSourcePeer(kPollFrequency, kDoNotRedactData, kIsIncremental);

  // Pre-fill the buffer with some data, then add the watchdog
  source.FillDataBufferForTesting("ABC");
  source.SetUpTestingWatchDog(mojom::DataFilter::New(REGEX, "[A-Z]+"));
  task_environment.RunUntilIdle();

  // Unlike CHANGE watchdogs, REGEX watchdogs do not fire on initial
  // add, even if there is a match on old data.
  EXPECT_FALSE(source.DidWatchDogCallbackFire());

  source.FillDataBufferForTesting("ABC");
  task_environment.RunUntilIdle();

  // ABC matches the regex, expect callback
  source.AssertWatchDogCallbackFiredWithData("ABC");

  source.FillDataBufferForTesting("123ABC");
  task_environment.RunUntilIdle();

  // Regexes are partial matches, so 123ABC still matches
  source.AssertWatchDogCallbackFiredWithData("123ABC");

  source.FillDataBufferForTesting("123");
  task_environment.RunUntilIdle();

  // No match; data is all numbers
  EXPECT_FALSE(source.DidWatchDogCallbackFire());

  source.FillDataBufferForTesting("");
  task_environment.RunUntilIdle();

  // No match; data is empty
  EXPECT_FALSE(source.DidWatchDogCallbackFire());
}

}  // namespace
}  // namespace ash::cfm
