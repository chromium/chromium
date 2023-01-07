// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_WEBRTC_WEBRTC_RTP_DUMP_HANDLER_H_
#define CHROME_BROWSER_MEDIA_WEBRTC_WEBRTC_RTP_DUMP_HANDLER_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "chrome/browser/media/webrtc/rtp_dump_type.h"

class WebRtcRtpDumpWriter;

// WebRtcRtpDumpHandler handles operations regarding the WebRTC RTP dump:
// - Starts or stops the RTP dumping on behalf of the client.
// - Stops the RTP dumping when the max dump file size is reached.
// - Writes the dump file.
// - Provides the dump file to the client code to be uploaded when
//   ReleaseRtpDump is called.
// - Cleans up the dump file if not transferred to the client before the object
//   is destroyed.
//
// Must be created/used/destroyed on the browser IO thread.
class WebRtcRtpDumpHandler {
 public:
  typedef base::OnceCallback<void(bool, const std::string&)>
      GenericDoneCallback;

  struct ReleasedDumps {
    ReleasedDumps(const base::FilePath& incoming_dump,
                  const base::FilePath& outgoing_dump)
        : incoming_dump_path(incoming_dump),
          outgoing_dump_path(outgoing_dump) {}

    const base::FilePath incoming_dump_path;
    const base::FilePath outgoing_dump_path;
  };

  // The caller must make sure |dump_dir| exists. RTP dump files are saved under
  // |dump_dir| as "rtpdump_$DIRECTION_$TIMESTAMP.gz", where $DIRECTION is
  // 'send' for outgoing dump or 'recv' for incoming dump. $TIMESTAMP is the
  // dump started time converted to a double number in microsecond precision,
  // which should guarantee the uniqueness across tabs and dump streams in
  // practice.
  explicit WebRtcRtpDumpHandler(const base::FilePath& dump_dir);

  WebRtcRtpDumpHandler(const WebRtcRtpDumpHandler&) = delete;
  WebRtcRtpDumpHandler& operator=(const WebRtcRtpDumpHandler&) = delete;

  ~WebRtcRtpDumpHandler();

  // Starts the specified type of dumping. Incoming/outgoing dumping can be
  // started separately. Returns true if called in a valid state, i.e. the
  // specified type of dump has not been started.
  bool StartDump(RtpDumpType type, std::string* error_message);

  // Stops the specified type of dumping. Incoming/outgoing dumping can be
  // stopped separately. Returns asynchronously through |callback|, where
  // |success| is true if StopDump is called in a valid state. The callback is
  // called when the writer finishes writing the dumps.
  void StopDump(RtpDumpType type, GenericDoneCallback callback);

  // Returns true if it's valid to call ReleaseDumps, i.e. no dumping is ongoing
  // or being stopped.
  bool ReadyToRelease() const;

  // Releases all the dumps and resets the state.
  // It should only be called when both incoming and outgoing dumping has been
  // stopped, i.e. ReadyToRelease() returns true. Returns the dump file paths.
  //
  // The caller will own the dump file after the method returns. If ReleaseDump
  // is not called before this object goes away, the dump file will be deleted
  // by this object.
  ReleasedDumps ReleaseDumps();

  // Adds an RTP packet to the dump. The caller must make sure it's a valid RTP
  // packet.
  void OnRtpPacket(const uint8_t* packet_header,
                   size_t header_length,
                   size_t packet_length,
                   bool incoming);

  // Stops all ongoing dumps and call |callback| when finished.
  void StopOngoingDumps(base::OnceClosure callback);

 private:
  friend class WebRtcRtpDumpHandlerTest;

  // State transitions:
  // initial --> STATE_NONE
  // StartDump --> STATE_STARTED
  // StopDump --> STATE_STOPPED
  // ReleaseDump --> STATE_RELEASING
  // ReleaseDump done --> STATE_NONE
  enum State {
    STATE_NONE,
    STATE_STARTED,
    STATE_STOPPING,
    STATE_STOPPED,
  };

  // For unit test to inject a fake writer.
  void SetDumpWriterForTesting(std::unique_ptr<WebRtcRtpDumpWriter> writer);

  // Callback from the dump writer when the max dump size is reached.
  void OnMaxDumpSizeReached();

  // Callback from the dump writer when ending dumps finishes. Calls |callback|
  // when finished.
  void OnDumpEnded(base::OnceClosure callback,
                   RtpDumpType ended_type,
                   bool incoming_succeeded,
                   bool outgoing_succeeded);

  SEQUENCE_CHECKER(main_sequence_);

  // The absolute path to the directory containing the incoming/outgoing dumps.
  const base::FilePath dump_dir_;

  // The dump file paths.
  base::FilePath incoming_dump_path_;
  base::FilePath outgoing_dump_path_;

  // The states of the incoming and outgoing dump.
  State incoming_state_;
  State outgoing_state_;

  // The object used to create and write the dump file.
  std::unique_ptr<WebRtcRtpDumpWriter> dump_writer_;

  base::WeakPtrFactory<WebRtcRtpDumpHandler> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_MEDIA_WEBRTC_WEBRTC_RTP_DUMP_HANDLER_H_
