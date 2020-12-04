// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CAPTURE_MODE_VIDEO_FILE_HANDLER_H_
#define ASH_CAPTURE_MODE_VIDEO_FILE_HANDLER_H_

#include <string>
#include <vector>

#include "ash/ash_export.h"
#include "base/callback_forward.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/sequence_checker.h"
#include "base/sequenced_task_runner.h"
#include "base/thread_annotations.h"
#include "base/threading/sequence_bound.h"

namespace ash {

// Defines an object that takes care of creating and saving video contents to a
// file with a given |path_|. Video contents are received from the Recording
// Service as a sequence of byte chunks on the UI thread, however creating the
// file and appending those chunks to it must be done on a blocking thread,
// whose |blocking_task_runner| is given to |Create()|.
//
// Depending on the audio/video contents and their encoding, and the amount of
// muxed chunks buffering done in the recording service itself, there can be
// many chunks received every second. Therefore, it would be very expensive to
// write to the file system every time we receive a chunk, so this object also
// takes care of caching those chunks by std::move()'ing them into
// |buffered_chunks_| until the total size of the cached chunks |current_size_|
// is nearing the maximum allowed |max_buffered_bytes_|, at which point, the
// buffered chunks will be appended to the file, and |buffered_chunks_| will be
// cleared.
//
// A maximum |max_buffered_bytes_| is enforced on the total number of bytes of
// all the cached chunks in |buffered_chunks_| to keep things deterministic, and
// avoid video recording consuming too much memory and eventually OOM'ing the
// system.
//
// An instance of this object can only be accessed on the blocking pool
// sequenced task runner given to |Create()|, which will construct a
// |base::SequenceBound| wrapper around an instance that guarantees all
// operations, including construction and destruction happening in sequence on
// the provided blooking pool. This asserts that destruction of this object
// happens only after all chunks have been flushed to the file.
class ASH_EXPORT VideoFileHandler {
 public:
  VideoFileHandler(const VideoFileHandler&) = delete;
  VideoFileHandler& operator=(const VideoFileHandler&) = delete;

  // Creates a |base::SequenceBound| wrapping an instance of VideoFileHandler
  // that enforces all operations this object provides can only be done
  // asynchronously on the given |blocking_task_runner|. This should be called
  // on the UI thread. |max_buffered_bytes| should be chosen to be big enough to
  // allow caching of many video chunks before we need to write to disk.
  static base::SequenceBound<VideoFileHandler> Create(
      scoped_refptr<base::SequencedTaskRunner> blocking_task_runner,
      const base::FilePath& path,
      size_t max_buffered_bytes,
      size_t low_disk_space_threshold_bytes,
      base::OnceClosure on_low_disk_space_callback);

  // Must be done only once before all other operations to open/create the file
  // at |path_| via |file_| in preparation for future writes. The file remains
  // open until this object is destroyed. Returns true on success, and false
  // otherwise. |success_| is also updated with this result before returning.
  // Callers must check that result before doing further operations since if
  // this fails, all future IO will fail too.
  bool Initialize();

  // Returns whether all IO operations done so far were successful.
  bool GetSuccessStatus() const;

  // Depending of the number of bytes currently cached, the given |chunk| is
  // either cached or appended to the file at |path_| along with any previously
  // cached chunks.
  // |success_| will be updated and returned as the result.
  bool AppendChunk(std::string chunk);

  // Flushes the currently cached chunks in |buffered_chunks_| by appending them
  // to |file_| and clearing the buffer so further chunks can be cached.
  // |success_| will be updated and returned as the result.
  bool FlushBufferedChunks();

 private:
  friend class base::SequenceBound<VideoFileHandler>;

  VideoFileHandler(const base::FilePath& path,
                   size_t max_buffered_bytes,
                   size_t low_disk_space_threshold_bytes,
                   base::OnceClosure on_low_disk_space_callback);
  ~VideoFileHandler();

  // Appends the given |chunk| to |file_| at its current position, and updates
  // |success_| with the result.
  void AppendToFile(const std::string& chunk);

  // The video file path.
  const base::FilePath path_;

  // This is the maximum allowed number of bytes that the total size of all the
  // chunks cached in |buffered_chunks_| cannot exceed. This is to make sure
  // there's an upper bound to the memory consumed by video recording to avoid
  // OOM'in the system.
  const size_t max_buffered_bytes_;

  // The number of bytes if the free disk space goes below which, we will notify
  // the owner of this object of this condition via the
  // |on_low_disk_space_callback_|.
  const size_t low_disk_space_threshold_bytes_;

  // The opened file that exists at |path_|. All I/O operations on the file will
  // be done through this object, which must first be initialized by calling
  // |Initialize()|.
  base::File file_ GUARDED_BY_CONTEXT(sequence_checker_);

  // The buffer used to cache the video chunks before they're written to disk.
  // Accessed only on the blocking thread.
  std::vector<std::string> buffered_chunks_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // The current size in bytes of all the cached chunks in |buffered_chunks_|.
  // Accessed only on the blocking thread.
  size_t current_size_ GUARDED_BY_CONTEXT(sequence_checker_) = 0;

  // The status of all IO operations done so far. It remains as false, until the
  // file is successfully opened in |Initialize()|. After that it will be
  // repeatedly updated by each write operation.
  bool success_ GUARDED_BY_CONTEXT(sequence_checker_) = false;

  // Called to notify the owner of this object that the low disk space threshold
  // has been reached.
  base::OnceClosure on_low_disk_space_callback_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // Checker to guarantee certain operations are run on the
  // |blocking_task_runner| given to |Create()|.
  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace ash

#endif  // ASH_CAPTURE_MODE_VIDEO_FILE_HANDLER_H_
