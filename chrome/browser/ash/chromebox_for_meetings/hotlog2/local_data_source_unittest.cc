// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/chromebox_for_meetings/hotlog2/local_data_source.h"

#include "base/logging.h"
#include "base/test/bind.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::cfm {
namespace {

constexpr std::string kDataSourceName = "fake_display_name";

// These are all arguments passed to the |LocalDataSource| object.
// Add them as named variables for clarity.
constexpr base::TimeDelta kPollFrequency = base::Seconds(0);
constexpr bool kRedactData = true;
constexpr bool kDoNotRedactData = false;

// Define callbacks to use for verification
void FetchCallbackFn(const std::vector<std::string>& expected_data,
                     const std::vector<std::string>& actual_data) {
  EXPECT_EQ(expected_data, actual_data);
}

void AddWatchDogCallbackFn(bool expected_success, bool actual_success) {
  EXPECT_EQ(expected_success, actual_success);
}

// Subclass LocalDataSource so we can provide implementations
// for pure-virtuals and control data creation.
class LocalDataSourcePeer : public LocalDataSource {
 public:
  LocalDataSourcePeer(base::TimeDelta poll_rate, bool data_needs_redacting)
      : LocalDataSource(poll_rate, data_needs_redacting), fake_data_index_(0) {
    // Arbitrary data. Each call to FillDataBufferForTesting() will load the
    // internal buffer with the next value (per the overridden GetNextData()
    // call) and increment the index for the next invocation. This replaces
    // the usual procedure of calling FillDataBuffer() via a timer, and it
    // allows testers to manually control the data flow. Testers can then
    // use Fetch() to verify that the correct data is returned. Note that
    // this data should first be initialized via SetFakeData() in all tests.
    fake_data_ = {};

    // Note that we are explicitly omitting the call to StartPollTimer()
    // here to keep things simple. FillDataBuffer() will be called manually
    // instead of on a timer cadence.
  }

  LocalDataSourcePeer(const LocalDataSourcePeer&) = delete;
  LocalDataSourcePeer& operator=(const LocalDataSourcePeer&) = delete;

  // Give public access to FillDataBuffer method. Note that FillDataBuffer()
  // will call GetNextData() internally, which has been overridden below.
  void FillDataBufferForTesting() { FillDataBuffer(); }

  // Provide a means for tests to initialize the return data
  void SetFakeData(const std::vector<std::string>& new_data) {
    fake_data_ = new_data;
  }

  // Add a wrapper around Fetch invocations for convenience. Use the callback
  // function defined above for data verification.
  void RunFetchWithExpectedData(const std::vector<std::string>& expected_data) {
    // Redirect callback to test function to verify that the data
    // received by the callback matches what we requested.
    auto callback = base::BindOnce(&FetchCallbackFn, expected_data);
    Fetch(std::move(callback));
  }

 protected:
  const std::string& GetDisplayName() override { return kDataSourceName; }

  std::vector<std::string> GetNextData() override {
    size_t tmp_index = fake_data_index_;

    fake_data_index_++;
    if (fake_data_index_ >= fake_data_.size()) {
      fake_data_index_ = 0;
    }

    // Return next piece of data in data buffer, looping to the
    // beginning when you reach the end of the fake data
    return {fake_data_[tmp_index]};
  }

 private:
  std::vector<std::string> fake_data_;
  unsigned int fake_data_index_;
};

// Define tests
TEST(HotlogLocalDataSourceTest, TestFetchWithBasicUsage) {
  auto source = LocalDataSourcePeer(kPollFrequency, kDoNotRedactData);
  source.SetFakeData({"a", "b", "c", "d", "e", "f"});

  // No data in buffer yet, so expecting empty data
  source.RunFetchWithExpectedData({});

  source.FillDataBufferForTesting();
  source.FillDataBufferForTesting();
  source.FillDataBufferForTesting();

  source.RunFetchWithExpectedData({"a", "b", "c"});
}

TEST(HotlogLocalDataSourceTest, TestFlushWithVariousFetches) {
  auto source = LocalDataSourcePeer(kPollFrequency, kDoNotRedactData);
  source.SetFakeData({"a", "b", "c", "d", "e", "f"});

  source.FillDataBufferForTesting();
  source.FillDataBufferForTesting();

  // First fetch will contain the data
  source.RunFetchWithExpectedData({"a", "b"});

  // Second fetch should contain the same data (no flush)
  source.RunFetchWithExpectedData({"a", "b"});

  // Third fetch should contain nothing (after flush)
  source.Flush();
  source.RunFetchWithExpectedData({});

  // Fill more data
  source.FillDataBufferForTesting();
  source.FillDataBufferForTesting();
  source.FillDataBufferForTesting();
  source.RunFetchWithExpectedData({"c", "d", "e"});

  // Filling data buffer should have no bearing on what
  // Fetch returns if we haven't Flush'ed yet
  source.FillDataBufferForTesting();
  source.FillDataBufferForTesting();
  source.RunFetchWithExpectedData({"c", "d", "e"});

  // Next Fetch() after Flushing() should yield the data
  // that built up in the previous block.
  source.Flush();
  source.RunFetchWithExpectedData({"f", "a"});
}

TEST(HotlogLocalDataSourceTest, TestBufferSizeIsCapped) {
  auto source = LocalDataSourcePeer(kPollFrequency, kDoNotRedactData);
  source.SetFakeData({"a", "b", "c", "d", "e", "f"});

  // Fill buffer to the max limit
  for (int i = 0; i < kMaxInternalBufferSize; i++) {
    source.FillDataBufferForTesting();
  }

  // Now fill beyond that limit
  source.FillDataBufferForTesting();
  source.FillDataBufferForTesting();
  source.FillDataBufferForTesting();

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

TEST(HotlogLocalDataSourceTest, TestWatchdogCallbackAlwaysFalse) {
  auto source = LocalDataSourcePeer(kPollFrequency, kDoNotRedactData);

  // TODO(b/326440932): watchdog support
  bool expected_result = false; /* success */
  auto unused_filter = mojom::DataFilter::New();
  mojo::PendingRemote<mojom::DataWatchDog> unused_remote;
  auto callback = base::BindOnce(&AddWatchDogCallbackFn, expected_result);

  source.AddWatchDog(std::move(unused_filter), std::move(unused_remote),
                     std::move(callback));
}

TEST(HotlogLocalDataSourceTest, TestRedactionWorksAsExpected) {
  auto source = LocalDataSourcePeer(kPollFrequency, kRedactData);

  source.SetFakeData({"192.168.0.1", "fake@email.com"});
  source.FillDataBufferForTesting();
  source.FillDataBufferForTesting();
  source.RunFetchWithExpectedData({"(192.168.0.0/16: 1)", "(email: 1)"});

  // Make a new source with redaction disabled
  auto new_source = LocalDataSourcePeer(kPollFrequency, kDoNotRedactData);

  new_source.SetFakeData({"192.168.0.1", "fake@email.com"});
  new_source.FillDataBufferForTesting();
  new_source.FillDataBufferForTesting();
  new_source.RunFetchWithExpectedData({"192.168.0.1", "fake@email.com"});
}

}  // namespace
}  // namespace ash::cfm
