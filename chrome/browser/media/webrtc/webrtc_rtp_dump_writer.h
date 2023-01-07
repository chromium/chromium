// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_WEBRTC_WEBRTC_RTP_DUMP_WRITER_H_
#define CHROME_BROWSER_MEDIA_WEBRTC_WEBRTC_RTP_DUMP_WRITER_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "chrome/browser/media/webrtc/rtp_dump_type.h"

// This class is responsible for creating the compressed RTP header dump file:
// - Adds the RTP headers to an in-memory buffer.
// - When the in-memory buffer is full, compresses it, and writes it to the
//   disk.
// - Notifies the caller when the on-disk file size reaches the max limit.
// - The uncompressed dump follows the standard RTPPlay format
//   (http://www.cs.columbia.edu/irt/software/rtptools/).
// - The caller is always responsible for cleaning up the dump file in all
//   cases.
// - WebRtcRtpDumpWriter does not stop writing to the dump after the max size
//   limit is reached. The caller must stop calling WriteRtpPacket instead.
//
// This object must run on the IO thread.
class WebRtcRtpDumpWriter {
 public:
  typedef base::OnceCallback<void(bool incoming_succeeded,
                                  bool outgoing_succeeded)>
      EndDumpCallback;

  // |incoming_dump_path| and |outgoing_dump_path| are the file paths of the
  // compressed dump files for incoming and outgoing packets respectively.
  // |max_dump_size| is the max size of the compressed dump file in bytes.
  // |max_dump_size_reached_callback| will be called when the on-disk file size
  // reaches |max_dump_size|.
  WebRtcRtpDumpWriter(const base::FilePath& incoming_dump_path,
                      const base::FilePath& outgoing_dump_path,
                      size_t max_dump_size,
                      base::RepeatingClosure max_dump_size_reached_callback);

  WebRtcRtpDumpWriter(const WebRtcRtpDumpWriter&) = delete;
  WebRtcRtpDumpWriter& operator=(const WebRtcRtpDumpWriter&) = delete;

  virtual ~WebRtcRtpDumpWriter();

  // Adds a RTP packet to the dump. The caller must make sure it's a valid RTP
  // packet. No validation is done by this method.
  virtual void WriteRtpPacket(const uint8_t* packet_header,
                              size_t header_length,
                              size_t packet_length,
                              bool incoming);

  // Flushes the in-memory buffer to the disk and ends the dump. The caller must
  // make sure the dump has not already been ended.
  // |finished_callback| will be called to indicate whether the dump is valid.
  // If this object is destroyed before the operation is finished, the callback
  // will be canceled and the dump files will be deleted.
  virtual void EndDump(RtpDumpType type, EndDumpCallback finished_callback);

  size_t max_dump_size() const;

  const scoped_refptr<base::SequencedTaskRunner>& background_task_runner()
      const {
    return background_task_runner_;
  }

 private:
  enum FlushResult {
    // Flushing has succeeded and the dump size is under the max limit.
    FLUSH_RESULT_SUCCESS,
    // Nothing has been written to disk and the dump is empty.
    FLUSH_RESULT_NO_DATA,
    // Flushing has failed for other reasons.
    FLUSH_RESULT_FAILURE
  };

  class FileWorker;

  typedef base::OnceCallback<void(bool)> FlushDoneCallback;

  // Used by EndDump to cache the input and intermediate results.
  struct EndDumpContext {
    EndDumpContext(RtpDumpType type, EndDumpCallback callback);
    EndDumpContext(EndDumpContext&& other);
    ~EndDumpContext();

    RtpDumpType type;
    bool incoming_succeeded;
    bool outgoing_succeeded;
    EndDumpCallback callback;
  };

  // Flushes the in-memory buffer to disk. If |incoming| is true, the incoming
  // buffer will be flushed; otherwise, the outgoing buffer will be flushed.
  // The dump file will be ended if |end_stream| is true. |callback| will be
  // called when flushing is done.
  void FlushBuffer(bool incoming, bool end_stream, FlushDoneCallback callback);

  // Called when FlushBuffer finishes. Checks the max dump size limit and
  // maybe calls the |max_dump_size_reached_callback_|. Also calls |callback|
  // with the flush result.
  void OnFlushDone(FlushDoneCallback callback,
                   const std::unique_ptr<FlushResult>& result,
                   const std::unique_ptr<size_t>& bytes_written);

  // Called when one type of dump has been ended. It continues to end the other
  // dump if needed based on |context| and |incoming|, or calls the callback in
  // |context| if no more dump needs to be ended.
  void OnDumpEnded(EndDumpContext context, bool incoming, bool success);

  // The max limit on the total size of incoming and outgoing dumps on disk.
  const size_t max_dump_size_;

  // The callback to call when the max size limit is reached.
  const base::RepeatingClosure max_dump_size_reached_callback_;

  // The in-memory buffers for the uncompressed dumps.
  std::vector<uint8_t> incoming_buffer_;
  std::vector<uint8_t> outgoing_buffer_;

  // The time when the first packet is dumped.
  base::TimeTicks start_time_;

  // The total on-disk size of the compressed incoming and outgoing dumps.
  size_t total_dump_size_on_disk_;

  // File workers must be called and deleted on the backround task runner.
  scoped_refptr<base::SequencedTaskRunner> background_task_runner_;
  std::unique_ptr<FileWorker> incoming_file_thread_worker_;
  std::unique_ptr<FileWorker> outgoing_file_thread_worker_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<WebRtcRtpDumpWriter> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_MEDIA_WEBRTC_WEBRTC_RTP_DUMP_WRITER_H_
