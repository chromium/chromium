// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_FILEAPI_TEST_FAKE_RECENT_SOURCE_H_
#define CHROME_BROWSER_ASH_FILEAPI_TEST_FAKE_RECENT_SOURCE_H_

#include <map>
#include <memory>
#include <vector>

#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/ash/fileapi/recent_source.h"

namespace ash {

class RecentFile;

// A file producer class that delivers specified files with the given lag.
class FileProducer {
 public:
  FileProducer(const base::TimeDelta& lag, std::vector<RecentFile> files);

  ~FileProducer();

  // The method which delivers the files after the lag has elapsed.
  void GetFiles(RecentSource::GetRecentFilesCallback callback);

 private:
  // The method called by OneShotTimer when this producer is ready to deliver
  // files to the caller of GetFiles.
  void OnFilesReady(RecentSource::GetRecentFilesCallback callback);

  const base::TimeDelta lag_;

  // Timers used to trigger the response. Every time GetFiles is called, a new
  // timer is allocated.
  std::vector<std::unique_ptr<base::OneShotTimer>> timers_;

  // The files to be delivered after the specified lag passed.
  std::vector<RecentFile> files_;
};

// Fake implementation of RecentSource that returns a canned set of files.
//
// All member functions must be called on the UI thread.
class FakeRecentSource : public RecentSource {
 public:
  // Creates a recent source for kTesting volume type.
  FakeRecentSource();
  // Creates a recent source with the specified volume type.
  FakeRecentSource(
      extensions::api::file_manager_private::VolumeType volume_type);

  FakeRecentSource(const FakeRecentSource&) = delete;
  FakeRecentSource& operator=(const FakeRecentSource&) = delete;

  ~FakeRecentSource() override;

  // RecentSource overrides. When this method is called it triggers GetFiles
  // calls on all known file producers. When all file producers deliver the
  // results the callback is called with all delivered files.
  void GetRecentFiles(const Params& params,
                      GetRecentFilesCallback callback) override;

  std::vector<RecentFile> Stop(int32_t call_id) override;

  // Adds a new producer; these must be added before GetRecentFiles is called.
  void AddProducer(std::unique_ptr<FileProducer> producer);

 private:
  // The callback called by file producers.
  void OnProducerReady(const int32_t call_id, std::vector<RecentFile> files);

  // Called when this source received files from all file producers.
  void OnAllProducersDone(const int32_t call_id);

  // Filters files in the file accumulator down to those matching the criteria
  // specified in the params_.
  std::vector<RecentFile> GetMatchingFiles(
      const std::vector<RecentFile>& accumulator,
      const Params& params);

  struct CallContext {
    CallContext(GetRecentFilesCallback callback, const Params& params);
    CallContext(CallContext&& context);
    ~CallContext();

    // Callback; set when GetRecentFiles is called.
    GetRecentFilesCallback callback;

    // Copy of parameters. Set when GetRecentFiles is called.
    Params params;

    // Accumulates files returned by producers.
    std::vector<RecentFile> accumulator;

    // The number of active file producers.
    size_t active_producer_count = 0;
  };

  // A map from call_id to callback.
  std::map<int32_t, CallContext> context_map_;

  // The list of producers.
  std::vector<std::unique_ptr<FileProducer>> producers_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_FILEAPI_TEST_FAKE_RECENT_SOURCE_H_
