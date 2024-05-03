// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CHROMEBOX_FOR_MEETINGS_HOTLOG2_LOCAL_DATA_SOURCE_H_
#define CHROME_BROWSER_ASH_CHROMEBOX_FOR_MEETINGS_HOTLOG2_LOCAL_DATA_SOURCE_H_

#include <deque>

#include "base/memory/weak_ptr.h"
#include "chromeos/ash/services/chromebox_for_meetings/public/mojom/meet_devices_data_aggregator.mojom.h"
#include "components/feedback/redaction_tool/redaction_tool.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "third_party/re2/src/re2/re2.h"

namespace ash::cfm {

// Maximum lines that can be in the internal buffer before we start
// purging older records. In the working case, we should never hit
// this limit, but we may reach it if we're unable to enqueue logs
// via Fetch() for whatever reason (eg a network outage).
inline constexpr int kMaxInternalBufferSize = 50000;  // ~7Mb

class LocalDataSource : public mojom::DataSource {
 public:
  LocalDataSource(base::TimeDelta poll_rate,
                  bool data_needs_redacting,
                  bool is_incremental);
  LocalDataSource(const LocalDataSource&) = delete;
  LocalDataSource& operator=(const LocalDataSource&) = delete;
  ~LocalDataSource() override;

  // mojom::DataSource implementation
  void Fetch(FetchCallback callback) override;
  void AddWatchDog(mojom::DataFilterPtr filter,
                   mojo::PendingRemote<mojom::DataWatchDog> pending_watch_dog,
                   AddWatchDogCallback callback) override;
  void Flush() override;

  void StartCollectingData();

 protected:
  void FillDataBuffer();
  // Make this virtual so unittests can override it
  virtual void SerializeDataBuffer(std::vector<std::string>& buffer);

  // Returns a unique identifier for logging purposes only.
  virtual const std::string& GetDisplayName() = 0;

  // Returns an array of the "next" data. For commands or other stately
  // data sources, this will likely just be the current state. For sources
  // that are incremental, like log files, this might be the next batch of
  // lines in the file (or otherwise). This data will be added to the internal
  // buffer for temporary storage until the next call to Fetch().
  virtual std::vector<std::string> GetNextData() = 0;

 private:
  bool IsDataBufferOverMaxLimit();
  void RedactDataBuffer(std::vector<std::string>& buffer);
  void AddTimestamps(std::vector<std::string>& data);
  bool IsWatchDogFilterValid(mojom::DataFilterPtr& filter);
  void FireChangeWatchdogCallbacks(const std::string& data);
  void CheckRegexWatchdogsAndFireCallbacks(const std::string& data);

  base::RepeatingTimer poll_timer_;
  base::TimeDelta poll_rate_;

  // True if we should pass the data through the redactor tool
  // before uploading, False otherwise.
  bool data_needs_redacting_;

  // Set to True when the data source yields incremental data, like a
  // log file, and False when the source simply yields current state.
  const bool is_incremental_;

  // Contains a chain of the most recent data. Will be moved into
  // pending_upload_buffer_ below upon a call to Fetch().
  std::deque<std::string> data_buffer_;

  // Contains a chain of data that are queued for upload. Will be
  // cleared upon a call to Flush();
  std::vector<std::string> pending_upload_buffer_;

  // Redaction tool for PII redaction
  redaction::RedactionTool redactor_;

  // Contains the most recent unique data from GetNextData(). Only used
  // for non-incremental sources to avoid spamming the same data.
  std::vector<std::string> last_unique_data_;

  // Contains a set of watchdogs to be fired when the output contains
  // any change since the previous output.
  mojo::RemoteSet<mojom::DataWatchDog> change_based_watchdogs_;

  // Contains a collection of watchdogs to be fired when the output matches
  // one or more regex patterns. The patterns are the keys, which map to a
  // set of remotes. Supports multiple watchdogs per pattern.
  std::map<const std::string, mojo::RemoteSet<mojom::DataWatchDog>>
      regex_based_watchdogs_;

  // Regex objects are expensive to create, so cache them here and
  // reuse for repeated usages and duplicate watchdogs.
  std::map<const std::string, std::unique_ptr<RE2>> regex_cache_;

  // Must be the last class member.
  base::WeakPtrFactory<LocalDataSource> weak_ptr_factory_{this};
};

}  // namespace ash::cfm

#endif  // CHROME_BROWSER_ASH_CHROMEBOX_FOR_MEETINGS_HOTLOG2_LOCAL_DATA_SOURCE_H_
