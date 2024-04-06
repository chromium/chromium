// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/webrtc/webrtc_rtp_dump_writer.h"

#include <string.h>

#include "base/containers/extend.h"
#include "base/containers/span.h"
#include "base/containers/span_writer.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/numerics/byte_conversions.h"
#include "base/task/thread_pool.h"
#include "content/public/browser/browser_thread.h"
#include "third_party/zlib/zlib.h"

namespace {

static const size_t kMinimumGzipOutputBufferSize = 256;  // In bytes.

const char kRtpDumpFileHeaderFirstLine[] = "#!rtpplay1.0 0.0.0.0/0\n";
static const size_t kRtpDumpFileHeaderSize = 16;  // In bytes.

// A helper for writing the header of the dump file.
void WriteRtpDumpFileHeaderBigEndian(base::TimeTicks start,
                                     std::vector<uint8_t>* output) {
  size_t buffer_start_pos = output->size();
  output->resize(output->size() + kRtpDumpFileHeaderSize);

  base::SpanWriter writer(
      base::span<uint8_t>(*output).subspan(buffer_start_pos));

  base::TimeDelta delta = start - base::TimeTicks();
  uint32_t start_sec = delta.InSeconds();
  writer.WriteU32BigEndian(start_sec);

  uint32_t start_usec =
      delta.InMilliseconds() * base::Time::kMicrosecondsPerMillisecond;
  writer.WriteU32BigEndian(start_usec);

  // Network source, always 0.
  writer.WriteU32BigEndian(uint32_t{0});
  // UDP port, always 0.
  writer.WriteU16BigEndian(uint16_t{0});
  // 2 bytes padding.
  writer.WriteU16BigEndian(uint16_t{0});

  CHECK_EQ(writer.remaining(), 0u);
}

// The header size for each packet dump.
static const size_t kPacketDumpHeaderSize = 8;  // In bytes.

// A helper for writing the header for each packet dump.
// |start| is the time when the recording is started.
// |dump_length| is the length of the packet dump including this header.
// |packet_length| is the length of the RTP packet header.
void WritePacketDumpHeaderBigEndian(const base::TimeTicks& start,
                                    uint16_t dump_length,
                                    uint16_t packet_length,
                                    std::vector<uint8_t>* output) {
  size_t buffer_start_pos = output->size();
  output->resize(output->size() + kPacketDumpHeaderSize);

  auto buffer = base::span(*output).subspan(buffer_start_pos);
  base::SpanWriter writer(buffer);
  writer.WriteU16BigEndian(dump_length);
  writer.WriteU16BigEndian(packet_length);
  uint32_t elapsed =
      static_cast<uint32_t>((base::TimeTicks::Now() - start).InMilliseconds());
  writer.WriteU32BigEndian(elapsed);
  CHECK_EQ(writer.remaining(), 0u);
}

}  // namespace

// This class runs on the backround task runner, compresses and writes the
// dump buffer to disk.
class WebRtcRtpDumpWriter::FileWorker {
 public:
  explicit FileWorker(const base::FilePath& dump_path) : dump_path_(dump_path) {
    DETACH_FROM_SEQUENCE(sequence_checker_);

    memset(&stream_, 0, sizeof(stream_));
    int result = deflateInit2(&stream_,
                              Z_DEFAULT_COMPRESSION,
                              Z_DEFLATED,
                              // windowBits = 15 is default, 16 is added to
                              // produce a gzip header + trailer.
                              15 + 16,
                              8,  // memLevel = 8 is default.
                              Z_DEFAULT_STRATEGY);
    DCHECK_EQ(Z_OK, result);
  }

  FileWorker(const FileWorker&) = delete;
  FileWorker& operator=(const FileWorker&) = delete;

  ~FileWorker() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    // Makes sure all allocations are freed.
    deflateEnd(&stream_);
  }

  // Compresses the data in |buffer| and write to the dump file. If |end_stream|
  // is true, the compression stream will be ended and the dump file cannot be
  // written to any more.
  void CompressAndWriteToFileOnFileThread(
      std::unique_ptr<std::vector<uint8_t>> buffer,
      bool end_stream,
      FlushResult* result,
      size_t* bytes_written) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    // This is called either when the in-memory buffer is full or the dump
    // should be ended.
    DCHECK(!buffer->empty() || end_stream);

    *result = FLUSH_RESULT_SUCCESS;
    *bytes_written = 0;

    // There may be nothing to compress/write if there is no RTP packet since
    // the last flush.
    if (!buffer->empty()) {
      *bytes_written = CompressAndWriteBufferToFile(buffer.get(), result);
    } else if (!base::PathExists(dump_path_)) {
      // If the dump does not exist, it means there is no RTP packet recorded.
      // Return FLUSH_RESULT_NO_DATA to indicate no dump file created.
      *result = FLUSH_RESULT_NO_DATA;
    }

    if (end_stream && !EndDumpFile())
      *result = FLUSH_RESULT_FAILURE;
  }

 private:
  // Helper for CompressAndWriteToFileOnFileThread to compress and write one
  // dump.
  size_t CompressAndWriteBufferToFile(std::vector<uint8_t>* buffer,
                                      FlushResult* result) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    DCHECK(buffer->size());

    *result = FLUSH_RESULT_SUCCESS;

    std::vector<uint8_t> compressed_buffer;
    if (!Compress(buffer, &compressed_buffer)) {
      DVLOG(2) << "Compressing buffer failed.";
      *result = FLUSH_RESULT_FAILURE;
      return 0;
    }

    bool success = false;

    if (base::PathExists(dump_path_)) {
      success = base::AppendToFile(dump_path_, compressed_buffer);
    } else {
      success = base::WriteFile(dump_path_, compressed_buffer);
    }

    if (!success) {
      DVLOG(2) << "Writing file failed: " << dump_path_.value();
      *result = FLUSH_RESULT_FAILURE;
      return 0;
    }
    return compressed_buffer.size();
  }

  // Compresses |input| into |output|.
  bool Compress(std::vector<uint8_t>* input, std::vector<uint8_t>* output) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    int result = Z_OK;

    output->resize(std::max(kMinimumGzipOutputBufferSize, input->size()));

    stream_.next_in = &(*input)[0];
    stream_.avail_in = input->size();
    stream_.next_out = &(*output)[0];
    stream_.avail_out = output->size();

    result = deflate(&stream_, Z_SYNC_FLUSH);
    DCHECK_EQ(Z_OK, result);
    DCHECK_EQ(0U, stream_.avail_in);

    output->resize(output->size() - stream_.avail_out);

    stream_.next_in = nullptr;
    stream_.next_out = nullptr;
    stream_.avail_out = 0;
    return true;
  }

  // Ends the compression stream and completes the dump file.
  bool EndDumpFile() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    std::vector<uint8_t> output_buffer;
    output_buffer.resize(kMinimumGzipOutputBufferSize);

    stream_.next_in = nullptr;
    stream_.avail_in = 0;
    stream_.next_out = &output_buffer[0];
    stream_.avail_out = output_buffer.size();

    int result = deflate(&stream_, Z_FINISH);
    DCHECK_EQ(Z_STREAM_END, result);

    result = deflateEnd(&stream_);
    DCHECK_EQ(Z_OK, result);

    output_buffer.resize(output_buffer.size() - stream_.avail_out);

    memset(&stream_, 0, sizeof(z_stream));

    DCHECK(!output_buffer.empty());
    return base::AppendToFile(dump_path_, output_buffer);
  }

  const base::FilePath dump_path_;

  z_stream stream_;

  SEQUENCE_CHECKER(sequence_checker_);
};

WebRtcRtpDumpWriter::WebRtcRtpDumpWriter(
    const base::FilePath& incoming_dump_path,
    const base::FilePath& outgoing_dump_path,
    size_t max_dump_size,
    base::RepeatingClosure max_dump_size_reached_callback)
    : max_dump_size_(max_dump_size),
      max_dump_size_reached_callback_(
          std::move(max_dump_size_reached_callback)),
      total_dump_size_on_disk_(0),
      background_task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT})),
      incoming_file_thread_worker_(new FileWorker(incoming_dump_path)),
      outgoing_file_thread_worker_(new FileWorker(outgoing_dump_path)) {}

WebRtcRtpDumpWriter::~WebRtcRtpDumpWriter() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  bool success = background_task_runner_->DeleteSoon(
      FROM_HERE, incoming_file_thread_worker_.release());
  DCHECK(success);

  success = background_task_runner_->DeleteSoon(
      FROM_HERE, outgoing_file_thread_worker_.release());
  DCHECK(success);
}

void WebRtcRtpDumpWriter::WriteRtpPacket(const uint8_t* packet_header,
                                         size_t header_length,
                                         size_t packet_length,
                                         bool incoming) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  static const size_t kMaxInMemoryBufferSize = 65536;

  std::vector<uint8_t>* dest_buffer =
      incoming ? &incoming_buffer_ : &outgoing_buffer_;

  // We use the capacity of the buffer to indicate if the buffer has been
  // initialized and if the dump file header has been created.
  if (!dest_buffer->capacity()) {
    dest_buffer->reserve(std::min(kMaxInMemoryBufferSize, max_dump_size_));

    start_time_ = base::TimeTicks::Now();

    // Writes the dump file header.
    base::Extend(
        *dest_buffer,
        base::as_bytes(base::span_from_cstring(kRtpDumpFileHeaderFirstLine)));
    WriteRtpDumpFileHeaderBigEndian(start_time_, dest_buffer);
  }

  size_t packet_dump_length = kPacketDumpHeaderSize + header_length;

  // Flushes the buffer to disk if the buffer is full.
  if (dest_buffer->size() + packet_dump_length > dest_buffer->capacity())
    FlushBuffer(incoming, false, FlushDoneCallback());

  WritePacketDumpHeaderBigEndian(
      start_time_, packet_dump_length, packet_length, dest_buffer);

  // Writes the actual RTP packet header.
  base::Extend(*dest_buffer,
               // TODO(crbug.com/40284755): WriteRtpPacket should receive a
               // span, not a pointer+length pair.
               UNSAFE_BUFFERS(base::span(packet_header, header_length)));
}

void WebRtcRtpDumpWriter::EndDump(RtpDumpType type,
                                  EndDumpCallback finished_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(type == RTP_DUMP_OUTGOING || incoming_file_thread_worker_ != nullptr);
  DCHECK(type == RTP_DUMP_INCOMING || outgoing_file_thread_worker_ != nullptr);

  bool incoming = (type == RTP_DUMP_BOTH || type == RTP_DUMP_INCOMING);
  EndDumpContext context(type, std::move(finished_callback));

  // End the incoming dump first if required. OnDumpEnded will continue to end
  // the outgoing dump if necessary.
  FlushBuffer(incoming, true,
              base::BindOnce(&WebRtcRtpDumpWriter::OnDumpEnded,
                             weak_ptr_factory_.GetWeakPtr(), std::move(context),
                             incoming));
}

size_t WebRtcRtpDumpWriter::max_dump_size() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return max_dump_size_;
}

WebRtcRtpDumpWriter::EndDumpContext::EndDumpContext(RtpDumpType type,
                                                    EndDumpCallback callback)
    : type(type),
      incoming_succeeded(false),
      outgoing_succeeded(false),
      callback(std::move(callback)) {}

WebRtcRtpDumpWriter::EndDumpContext::EndDumpContext(EndDumpContext&& other) =
    default;

WebRtcRtpDumpWriter::EndDumpContext::~EndDumpContext() = default;

void WebRtcRtpDumpWriter::FlushBuffer(bool incoming,
                                      bool end_stream,
                                      FlushDoneCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::unique_ptr<std::vector<uint8_t>> new_buffer(new std::vector<uint8_t>());

  if (incoming) {
    new_buffer->reserve(incoming_buffer_.capacity());
    new_buffer->swap(incoming_buffer_);
  } else {
    new_buffer->reserve(outgoing_buffer_.capacity());
    new_buffer->swap(outgoing_buffer_);
  }

  std::unique_ptr<FlushResult> result(new FlushResult(FLUSH_RESULT_FAILURE));

  std::unique_ptr<size_t> bytes_written(new size_t(0));

  FileWorker* worker = incoming ? incoming_file_thread_worker_.get()
                                : outgoing_file_thread_worker_.get();

  // Using "Unretained(worker)" because |worker| is owner by this object and it
  // guaranteed to be deleted on the backround task runner before this object
  // goes away.
  base::OnceClosure task = base::BindOnce(
      &FileWorker::CompressAndWriteToFileOnFileThread, base::Unretained(worker),
      std::move(new_buffer), end_stream, result.get(), bytes_written.get());

  // OnFlushDone is necessary to avoid running the callback after this
  // object is gone.
  base::OnceClosure reply = base::BindOnce(
      &WebRtcRtpDumpWriter::OnFlushDone, weak_ptr_factory_.GetWeakPtr(),
      std::move(callback), std::move(result), std::move(bytes_written));

  // Define the task and reply outside the method call so that getting and
  // passing the scoped_ptr does not depend on the argument evaluation order.
  background_task_runner_->PostTaskAndReply(FROM_HERE, std::move(task),
                                            std::move(reply));

  if (end_stream) {
    bool success = background_task_runner_->DeleteSoon(
        FROM_HERE, incoming ? incoming_file_thread_worker_.release()
                            : outgoing_file_thread_worker_.release());
    DCHECK(success);
  }
}

void WebRtcRtpDumpWriter::OnFlushDone(
    FlushDoneCallback callback,
    const std::unique_ptr<FlushResult>& result,
    const std::unique_ptr<size_t>& bytes_written) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  total_dump_size_on_disk_ += *bytes_written;

  if (total_dump_size_on_disk_ >= max_dump_size_ &&
      !max_dump_size_reached_callback_.is_null()) {
    max_dump_size_reached_callback_.Run();
  }

  // Returns success for FLUSH_RESULT_MAX_SIZE_REACHED since the dump is still
  // valid.
  if (!callback.is_null()) {
    std::move(callback).Run(*result != FLUSH_RESULT_FAILURE &&
                            *result != FLUSH_RESULT_NO_DATA);
  }
}

void WebRtcRtpDumpWriter::OnDumpEnded(EndDumpContext context,
                                      bool incoming,
                                      bool success) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  DVLOG(2) << "Dump ended, incoming = " << incoming
           << ", succeeded = " << success;

  if (incoming)
    context.incoming_succeeded = success;
  else
    context.outgoing_succeeded = success;

  // End the outgoing dump if needed.
  if (incoming && context.type == RTP_DUMP_BOTH) {
    FlushBuffer(false, true,
                base::BindOnce(&WebRtcRtpDumpWriter::OnDumpEnded,
                               weak_ptr_factory_.GetWeakPtr(),
                               std::move(context), false));
    return;
  }

  // This object might be deleted after running the callback.
  std::move(context).callback.Run(context.incoming_succeeded,
                                  context.outgoing_succeeded);
}
