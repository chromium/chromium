// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CHROMEBOX_FOR_MEETINGS_HOTLOG2_LOCAL_DATA_SOURCE_H_
#define CHROME_BROWSER_ASH_CHROMEBOX_FOR_MEETINGS_HOTLOG2_LOCAL_DATA_SOURCE_H_

#include <deque>

#include "base/memory/weak_ptr.h"
#include "chromeos/ash/services/chromebox_for_meetings/public/mojom/meet_devices_data_aggregator.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace ash::cfm {

class LocalDataSource : public mojom::DataSource {
 public:
  explicit LocalDataSource(base::TimeDelta poll_rate);
  LocalDataSource(const LocalDataSource&) = delete;
  LocalDataSource& operator=(const LocalDataSource&) = delete;
  ~LocalDataSource() override;

 protected:
  // mojom::DataSource implementation
  void Fetch(FetchCallback callback) override;
  void AddWatchDog(mojom::DataFilterPtr filter,
                   mojo::PendingRemote<mojom::DataWatchDog> pending_watch_dog,
                   AddWatchDogCallback callback) override;
  void Flush() override;

  void StartPollTimer();

  // Returns a unique identifier for logging purposes only.
  virtual const std::string& GetDisplayName() = 0;

  // Returns an array of the "next" data. For commands or other stately
  // data sources, this will likely just be the current state. For sources
  // that are incremental, like log files, this might be the next batch of
  // lines in the file (or otherwise). This data will be added to the internal
  // buffer for temporary storage until the next call to Fetch().
  virtual std::vector<std::string> GetNextData() = 0;

 private:
  void FillDataBuffer();
  bool IsDataBufferAtMaxLimit();

  base::RepeatingTimer poll_timer_;
  base::TimeDelta poll_rate_;

  // Contains a chain of the most recent data. Will be moved into
  // pending_upload_buffer_ below upon a call to Fetch().
  std::deque<std::string> data_buffer_;

  // Contains a chain of data that are queued for upload. Will be
  // cleared upon a call to Flush();
  std::vector<std::string> pending_upload_buffer_;

  // Must be the last class member.
  base::WeakPtrFactory<LocalDataSource> weak_ptr_factory_{this};
};

}  // namespace ash::cfm

#endif  // CHROME_BROWSER_ASH_CHROMEBOX_FOR_MEETINGS_HOTLOG2_LOCAL_DATA_SOURCE_H_
