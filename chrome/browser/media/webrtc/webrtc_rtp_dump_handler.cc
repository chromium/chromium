// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/webrtc/webrtc_rtp_dump_handler.h"

#include <utility>

#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "chrome/browser/media/webrtc/webrtc_rtp_dump_writer.h"

namespace {

static const size_t kMaxOngoingRtpDumpsAllowed = 5;

// The browser process wide total number of ongoing (i.e. started and not
// released) RTP dumps. Incoming and outgoing in one WebRtcDumpHandler are
// counted as one dump.
// Must be accessed on the browser IO thread.
static size_t g_ongoing_rtp_dumps = 0;

void FireGenericDoneCallback(WebRtcRtpDumpHandler::GenericDoneCallback callback,
                             bool success,
                             const std::string& error_message) {
  DCHECK(!callback.is_null());

  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), success, error_message));
}

bool DumpTypeContainsIncoming(RtpDumpType type) {
  return type == RTP_DUMP_INCOMING || type == RTP_DUMP_BOTH;
}

bool DumpTypeContainsOutgoing(RtpDumpType type) {
  return type == RTP_DUMP_OUTGOING || type == RTP_DUMP_BOTH;
}

}  // namespace

WebRtcRtpDumpHandler::WebRtcRtpDumpHandler(const base::FilePath& dump_dir)
    : dump_dir_(dump_dir),
      incoming_state_(STATE_NONE),
      outgoing_state_(STATE_NONE) {}

WebRtcRtpDumpHandler::~WebRtcRtpDumpHandler() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_);

  // Reset dump writer first to stop writing.
  if (dump_writer_) {
    --g_ongoing_rtp_dumps;
    dump_writer_.reset();
  }

  if (incoming_state_ != STATE_NONE && !incoming_dump_path_.empty()) {
    base::ThreadPool::PostTask(
        FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
        base::GetDeleteFileCallback(incoming_dump_path_));
  }

  if (outgoing_state_ != STATE_NONE && !outgoing_dump_path_.empty()) {
    base::ThreadPool::PostTask(
        FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
        base::GetDeleteFileCallback(outgoing_dump_path_));
  }
}

bool WebRtcRtpDumpHandler::StartDump(RtpDumpType type,
                                     std::string* error_message) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_);

  if (!dump_writer_ && g_ongoing_rtp_dumps >= kMaxOngoingRtpDumpsAllowed) {
    *error_message = "Max RTP dump limit reached.";
    DVLOG(2) << *error_message;
    return false;
  }

  // Returns an error if any type of dump specified by the caller cannot be
  // started.
  if ((DumpTypeContainsIncoming(type) && incoming_state_ != STATE_NONE) ||
      (DumpTypeContainsOutgoing(type) && outgoing_state_ != STATE_NONE)) {
    *error_message =
        "RTP dump already started for type " + base::NumberToString(type);
    return false;
  }

  if (DumpTypeContainsIncoming(type))
    incoming_state_ = STATE_STARTED;

  if (DumpTypeContainsOutgoing(type))
    outgoing_state_ = STATE_STARTED;

  DVLOG(2) << "Start RTP dumping: type = " << type;

  if (!dump_writer_) {
    ++g_ongoing_rtp_dumps;

    static const char kRecvDumpFilePrefix[] = "rtpdump_recv_";
    static const char kSendDumpFilePrefix[] = "rtpdump_send_";
    static const size_t kMaxDumpSize = 5 * 1024 * 1024;  // 5MB

    std::string dump_id =
        base::NumberToString(base::Time::Now().InSecondsFSinceUnixEpoch());
    incoming_dump_path_ =
        dump_dir_.AppendASCII(std::string(kRecvDumpFilePrefix) + dump_id)
            .AddExtension(FILE_PATH_LITERAL(".gz"));

    outgoing_dump_path_ =
        dump_dir_.AppendASCII(std::string(kSendDumpFilePrefix) + dump_id)
            .AddExtension(FILE_PATH_LITERAL(".gz"));

    // WebRtcRtpDumpWriter does not support changing the dump path after it's
    // created. So we assign both incoming and outgoing dump path even if only
    // one type of dumping has been started.
    // For "Unretained(this)", see comments StopDump.
    dump_writer_ = std::make_unique<WebRtcRtpDumpWriter>(
        incoming_dump_path_, outgoing_dump_path_, kMaxDumpSize,
        base::BindRepeating(&WebRtcRtpDumpHandler::OnMaxDumpSizeReached,
                            base::Unretained(this)));
  }

  return true;
}

void WebRtcRtpDumpHandler::StopDump(RtpDumpType type,
                                    GenericDoneCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_);

  // Returns an error if any type of dump specified by the caller cannot be
  // stopped.
  if ((DumpTypeContainsIncoming(type) && incoming_state_ != STATE_STARTED) ||
      (DumpTypeContainsOutgoing(type) && outgoing_state_ != STATE_STARTED)) {
    if (!callback.is_null()) {
      FireGenericDoneCallback(
          std::move(callback), false,
          "RTP dump not started or already stopped for type " +
              base::NumberToString(type));
    }
    return;
  }

  DVLOG(2) << "Stopping RTP dumping: type = " << type;

  if (DumpTypeContainsIncoming(type))
    incoming_state_ = STATE_STOPPING;

  if (DumpTypeContainsOutgoing(type))
    outgoing_state_ = STATE_STOPPING;

  // Using "Unretained(this)" because the this object owns the writer and the
  // writer is guaranteed to cancel the callback before it goes away. Same for
  // the other posted tasks bound to the writer.
  dump_writer_->EndDump(
      type,
      base::BindOnce(&WebRtcRtpDumpHandler::OnDumpEnded, base::Unretained(this),
                     callback.is_null()
                         ? base::NullCallback()
                         : base::BindOnce(&FireGenericDoneCallback,
                                          std::move(callback), true, ""),
                     type));
}

bool WebRtcRtpDumpHandler::ReadyToRelease() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_);

  return incoming_state_ != STATE_STARTED &&
         incoming_state_ != STATE_STOPPING &&
         outgoing_state_ != STATE_STARTED && outgoing_state_ != STATE_STOPPING;
}

WebRtcRtpDumpHandler::ReleasedDumps WebRtcRtpDumpHandler::ReleaseDumps() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_);
  DCHECK(ReadyToRelease());

  base::FilePath incoming_dump, outgoing_dump;

  if (incoming_state_ == STATE_STOPPED) {
    DVLOG(2) << "Incoming RTP dumps released: " << incoming_dump_path_.value();

    incoming_state_ = STATE_NONE;
    incoming_dump = incoming_dump_path_;
  }

  if (outgoing_state_ == STATE_STOPPED) {
    DVLOG(2) << "Outgoing RTP dumps released: " << outgoing_dump_path_.value();

    outgoing_state_ = STATE_NONE;
    outgoing_dump = outgoing_dump_path_;
  }
  return ReleasedDumps(incoming_dump, outgoing_dump);
}

void WebRtcRtpDumpHandler::OnRtpPacket(const uint8_t* packet_header,
                                       size_t header_length,
                                       size_t packet_length,
                                       bool incoming) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_);

  if ((incoming && incoming_state_ != STATE_STARTED) ||
      (!incoming && outgoing_state_ != STATE_STARTED)) {
    return;
  }

  dump_writer_->WriteRtpPacket(
      packet_header, header_length, packet_length, incoming);
}

void WebRtcRtpDumpHandler::StopOngoingDumps(base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_);
  DCHECK(!callback.is_null());

  // No ongoing dumps, return directly.
  if ((incoming_state_ == STATE_NONE || incoming_state_ == STATE_STOPPED) &&
      (outgoing_state_ == STATE_NONE || outgoing_state_ == STATE_STOPPED)) {
    std::move(callback).Run();
    return;
  }

  // If the background task runner is working on stopping the dumps, wait for it
  // to complete and then check the states again.
  if (incoming_state_ == STATE_STOPPING || outgoing_state_ == STATE_STOPPING) {
    dump_writer_->background_task_runner()->PostTaskAndReply(
        FROM_HERE, base::DoNothing(),
        base::BindOnce(&WebRtcRtpDumpHandler::StopOngoingDumps,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
    return;
  }

  // Either incoming or outgoing dump must be ongoing.
  RtpDumpType type =
      (incoming_state_ == STATE_STARTED)
          ? (outgoing_state_ == STATE_STARTED ? RTP_DUMP_BOTH
                                              : RTP_DUMP_INCOMING)
          : RTP_DUMP_OUTGOING;

  if (incoming_state_ == STATE_STARTED)
    incoming_state_ = STATE_STOPPING;

  if (outgoing_state_ == STATE_STARTED)
    outgoing_state_ = STATE_STOPPING;

  DVLOG(2) << "Stopping ongoing dumps: type = " << type;

  dump_writer_->EndDump(
      type, base::BindOnce(&WebRtcRtpDumpHandler::OnDumpEnded,
                           base::Unretained(this), std::move(callback), type));
}

void WebRtcRtpDumpHandler::SetDumpWriterForTesting(
    std::unique_ptr<WebRtcRtpDumpWriter> writer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_);

  dump_writer_ = std::move(writer);
  ++g_ongoing_rtp_dumps;

  incoming_dump_path_ = dump_dir_.AppendASCII("recv");
  outgoing_dump_path_ = dump_dir_.AppendASCII("send");
}

void WebRtcRtpDumpHandler::OnMaxDumpSizeReached() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_);

  RtpDumpType type =
      (incoming_state_ == STATE_STARTED)
          ? (outgoing_state_ == STATE_STARTED ? RTP_DUMP_BOTH
                                              : RTP_DUMP_INCOMING)
          : RTP_DUMP_OUTGOING;
  StopDump(type, GenericDoneCallback());
}

void WebRtcRtpDumpHandler::OnDumpEnded(base::OnceClosure callback,
                                       RtpDumpType ended_type,
                                       bool incoming_success,
                                       bool outgoing_success) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_);

  if (DumpTypeContainsIncoming(ended_type)) {
    DCHECK_EQ(STATE_STOPPING, incoming_state_);
    incoming_state_ = STATE_STOPPED;

    if (!incoming_success) {
      base::ThreadPool::PostTask(
          FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
          base::GetDeleteFileCallback(incoming_dump_path_));

      DVLOG(2) << "Deleted invalid incoming dump "
               << incoming_dump_path_.value();
      incoming_dump_path_.clear();
    }
  }

  if (DumpTypeContainsOutgoing(ended_type)) {
    DCHECK_EQ(STATE_STOPPING, outgoing_state_);
    outgoing_state_ = STATE_STOPPED;

    if (!outgoing_success) {
      base::ThreadPool::PostTask(
          FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
          base::GetDeleteFileCallback(outgoing_dump_path_));

      DVLOG(2) << "Deleted invalid outgoing dump "
               << outgoing_dump_path_.value();
      outgoing_dump_path_.clear();
    }
  }

  // Release the writer when it's no longer needed.
  if (incoming_state_ != STATE_STOPPING && outgoing_state_ != STATE_STOPPING &&
      incoming_state_ != STATE_STARTED && outgoing_state_ != STATE_STARTED) {
    dump_writer_.reset();
    --g_ongoing_rtp_dumps;
  }

  // This object might be deleted after running the callback.
  if (!callback.is_null())
    std::move(callback).Run();
}
