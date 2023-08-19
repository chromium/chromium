// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_WEBRTC_AUDIO_DEBUG_RECORDINGS_HANDLER_H_
#define CHROME_BROWSER_MEDIA_WEBRTC_AUDIO_DEBUG_RECORDINGS_HANDLER_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/time/time.h"

namespace content {
class BrowserContext;
class RenderProcessHost;
}

namespace media {
class AudioDebugRecordingSession;
}

// AudioDebugRecordingsHandler provides an interface to start and stop
// AudioDebugRecordings, including WebRTC AEC dumps. Lives on the UI thread.
class AudioDebugRecordingsHandler
    : public base::RefCountedThreadSafe<AudioDebugRecordingsHandler> {
 public:
  typedef base::OnceCallback<void(bool, const std::string&)>
      GenericDoneCallback;
  typedef base::OnceCallback<void(const std::string&)> RecordingErrorCallback;
  typedef base::OnceCallback<void(const std::string&, bool, bool)>
      RecordingDoneCallback;

  // Key used to attach the handler to the RenderProcessHost
  static const char kAudioDebugRecordingsHandlerKey[];

  explicit AudioDebugRecordingsHandler(
      content::BrowserContext* browser_context);

  AudioDebugRecordingsHandler(const AudioDebugRecordingsHandler&) = delete;
  AudioDebugRecordingsHandler& operator=(const AudioDebugRecordingsHandler&) =
      delete;

  // Starts an audio debug recording. The recording lasts the given |delay|,
  // unless |delay| is zero, in which case recording will continue until
  // StopAudioDebugRecordings() is explicitly invoked.
  // |callback| is invoked once recording stops. If |delay| is zero
  // |callback| is invoked once recording starts.
  // If a recording was already in progress, |error_callback| is invoked instead
  // of |callback|.
  void StartAudioDebugRecordings(content::RenderProcessHost* host,
                                 base::TimeDelta delay,
                                 RecordingDoneCallback callback,
                                 RecordingErrorCallback error_callback);

  // Stops an audio debug recording. |callback| is invoked once recording
  // stops. If no recording was in progress, |error_callback| is invoked instead
  // of |callback|.
  void StopAudioDebugRecordings(content::RenderProcessHost* host,
                                RecordingDoneCallback callback,
                                RecordingErrorCallback error_callback);

 private:
  friend class base::RefCountedThreadSafe<AudioDebugRecordingsHandler>;

  virtual ~AudioDebugRecordingsHandler();

  // Helper for starting audio debug recordings.
  void DoStartAudioDebugRecordings(int render_process_host_id,
                                   base::TimeDelta delay,
                                   RecordingDoneCallback callback,
                                   RecordingErrorCallback error_callback,
                                   const base::FilePath& log_directory);

  // Helper for stopping audio debug recordings.
  void DoStopAudioDebugRecordings(int render_process_host_id,
                                  bool is_manual_stop,
                                  uint64_t audio_debug_recordings_id,
                                  RecordingDoneCallback callback,
                                  RecordingErrorCallback error_callback,
                                  const base::FilePath& log_directory);

  // The browser context associated with our renderer process.
  const raw_ptr<content::BrowserContext, AcrossTasksDanglingUntriaged>
      browser_context_;

  // This counter allows saving each debug recording in separate files.
  uint64_t current_audio_debug_recordings_id_;

  // Used for controlling debug recordings.
  std::unique_ptr<media::AudioDebugRecordingSession>
      audio_debug_recording_session_;
};

#endif  // CHROME_BROWSER_MEDIA_WEBRTC_AUDIO_DEBUG_RECORDINGS_HANDLER_H_
