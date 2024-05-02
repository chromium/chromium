// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/chromebox_for_meetings/hotlog2/local_data_source.h"

#include "base/logging.h"
#include "base/test/bind.h"
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

void FetchCallbackFnWithRegex(const std::vector<std::string>& expected_data,
                              const std::vector<std::string>& actual_data) {
  EXPECT_EQ(expected_data.size(), actual_data.size());
  for (size_t i = 0; i < expected_data.size(); i++) {
    EXPECT_TRUE(RE2::FullMatch(actual_data[i], expected_data[i]));
  }
}

void AddWatchDogCallbackFn(bool expected_success, bool actual_success) {
  EXPECT_EQ(expected_success, actual_success);
}

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

 protected:
  const std::string& GetDisplayName() override { return kDataSourceName; }
  std::vector<std::string> GetNextData() override { return {next_data_}; }

 private:
  std::string next_data_;
};

// Define tests
TEST(HotlogLocalDataSourceTest, TestFetchWithBasicUsage) {
  auto source =
      LocalDataSourcePeer(kPollFrequency, kDoNotRedactData, kIsIncremental);

  // No data in buffer yet, so expecting empty data
  source.RunFetchWithExpectedData({});

  source.FillDataBufferForTesting({"a", "b", "c"});
  source.RunFetchWithExpectedData({"a", "b", "c"});
}

TEST(HotlogLocalDataSourceTest, TestNonIncrementalSource) {
  auto source =
      LocalDataSourcePeer(kPollFrequency, kDoNotRedactData, kIsNotIncremental);

  source.FillDataBufferForTesting({"aa", "aa", "b", "b", "bb", "aa"});

  // Verify we only get the unique data, and that it has a timestamp
  const std::string ts_regex = "[0-9]{4}-[0-9]{2}-[0-9]{2}T[0-9:\\.]+Z ";
  std::vector<std::string> expected_data = {"aa", "b", "bb", "aa"};
  for (size_t i = 0; i < expected_data.size(); i++) {
    expected_data[i] = ts_regex + expected_data[i];
  }

  // Using special regex callback to account for timestamps
  auto callback = base::BindOnce(&FetchCallbackFnWithRegex, expected_data);
  source.Fetch(std::move(callback));
}

TEST(HotlogLocalDataSourceTest, TestFlushWithVariousFetches) {
  auto source =
      LocalDataSourcePeer(kPollFrequency, kDoNotRedactData, kIsIncremental);

  source.FillDataBufferForTesting({"a", "b", "c"});

  // First fetch will contain the data
  source.RunFetchWithExpectedData({"a", "b", "c"});

  // Second fetch should contain the same data (no flush)
  source.RunFetchWithExpectedData({"a", "b", "c"});

  // Third fetch should contain nothing (after flush)
  source.Flush();
  source.RunFetchWithExpectedData({});

  // Fill more data
  source.FillDataBufferForTesting({"d", "e", "f"});
  source.RunFetchWithExpectedData({"d", "e", "f"});

  // Filling data buffer should have no bearing on what
  // Fetch returns if we haven't Flush'ed yet
  source.FillDataBufferForTesting({"a", "b", "c"});
  source.RunFetchWithExpectedData({"d", "e", "f"});

  // Next Fetch() after Flushing() should yield the data
  // that built up in the previous block.
  source.Flush();
  source.RunFetchWithExpectedData({"a", "b", "c"});
}

TEST(HotlogLocalDataSourceTest, TestBufferSizeIsCapped) {
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

TEST(HotlogLocalDataSourceTest, TestRedactionWorksAsExpected) {
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

TEST(HotlogWatchdogTest, TestVariousInvalidWatchdogs) {
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

}  // namespace
}  // namespace ash::cfm
