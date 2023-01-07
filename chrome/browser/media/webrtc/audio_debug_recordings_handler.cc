// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/webrtc/audio_debug_recordings_handler.h"

#include <string>
#include <utility>

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "components/webrtc_logging/browser/text_log_list.h"
#include "content/public/browser/audio_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "media/audio/audio_debug_recording_session.h"
#include "services/audio/public/cpp/debug_recording_session_factory.h"

using content::BrowserThread;

// Keys used to attach handler to the RenderProcessHost
const char AudioDebugRecordingsHandler::kAudioDebugRecordingsHandlerKey[] =
    "kAudioDebugRecordingsHandlerKey";

namespace {

// Returns a path name to be used as prefix for audio debug recordings files.
base::FilePath GetAudioDebugRecordingsPrefixPath(
    const base::FilePath& directory,
    uint64_t audio_debug_recordings_id) {
  static const char kAudioDebugRecordingsFilePrefix[] = "AudioDebugRecordings.";
  return directory.AppendASCII(kAudioDebugRecordingsFilePrefix +
                               base::NumberToString(audio_debug_recordings_id));
}

base::FilePath GetLogDirectoryAndEnsureExists(
    const base::FilePath& browser_context_path) {
  base::FilePath log_dir_path =
      webrtc_logging::TextLogList::GetWebRtcLogDirectoryForBrowserContextPath(
          browser_context_path);
  base::File::Error error;
  if (!base::CreateDirectoryAndGetError(log_dir_path, &error)) {
    DLOG(ERROR) << "Could not create WebRTC log directory, error: " << error;
    return base::FilePath();
  }
  return log_dir_path;
}

}  // namespace

AudioDebugRecordingsHandler::AudioDebugRecordingsHandler(
    content::BrowserContext* browser_context)
    : browser_context_(browser_context), current_audio_debug_recordings_id_(0) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(browser_context_);
}

AudioDebugRecordingsHandler::~AudioDebugRecordingsHandler() = default;

void AudioDebugRecordingsHandler::StartAudioDebugRecordings(
    content::RenderProcessHost* host,
    base::TimeDelta delay,
    RecordingDoneCallback callback,
    RecordingErrorCallback error_callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(host);

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&GetLogDirectoryAndEnsureExists,
                     browser_context_->GetPath()),
      base::BindOnce(&AudioDebugRecordingsHandler::DoStartAudioDebugRecordings,
                     this, host->GetID(), delay, std::move(callback),
                     std::move(error_callback)));
}

void AudioDebugRecordingsHandler::StopAudioDebugRecordings(
    content::RenderProcessHost* host,
    RecordingDoneCallback callback,
    RecordingErrorCallback error_callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(host);

  const bool is_manual_stop = true;
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&GetLogDirectoryAndEnsureExists,
                     browser_context_->GetPath()),
      base::BindOnce(&AudioDebugRecordingsHandler::DoStopAudioDebugRecordings,
                     this, host->GetID(), is_manual_stop,
                     current_audio_debug_recordings_id_, std::move(callback),
                     std::move(error_callback)));
}

void AudioDebugRecordingsHandler::DoStartAudioDebugRecordings(
    int render_process_host_id,
    base::TimeDelta delay,
    RecordingDoneCallback callback,
    RecordingErrorCallback error_callback,
    const base::FilePath& log_directory) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (audio_debug_recording_session_) {
    std::move(error_callback).Run("Audio debug recordings already in progress");
    return;
  }

  base::FilePath prefix_path = GetAudioDebugRecordingsPrefixPath(
      log_directory, ++current_audio_debug_recordings_id_);

  {
    content::RenderProcessHost* const host =
        content::RenderProcessHost::FromID(render_process_host_id);
    if (!host) {
      std::move(error_callback).Run("Render process host not found");
      return;
    }
    host->EnableAudioDebugRecordings(prefix_path);
  }

  mojo::PendingRemote<audio::mojom::DebugRecording> debug_recording;
  content::GetAudioService().BindDebugRecording(
      debug_recording.InitWithNewPipeAndPassReceiver());
  audio_debug_recording_session_ = audio::CreateAudioDebugRecordingSession(
      prefix_path, std::move(debug_recording));

  if (delay.is_zero()) {
    const bool is_stopped = false, is_manual_stop = false;
    std::move(callback).Run(prefix_path.AsUTF8Unsafe(), is_stopped,
                            is_manual_stop);
    return;
  }

  const bool is_manual_stop = false;
  content::GetUIThreadTaskRunner({})->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&AudioDebugRecordingsHandler::DoStopAudioDebugRecordings,
                     this, render_process_host_id, is_manual_stop,
                     current_audio_debug_recordings_id_, std::move(callback),
                     std::move(error_callback), log_directory),
      delay);
}

void AudioDebugRecordingsHandler::DoStopAudioDebugRecordings(
    int render_process_host_id,
    bool is_manual_stop,
    uint64_t audio_debug_recordings_id,
    RecordingDoneCallback callback,
    RecordingErrorCallback error_callback,
    const base::FilePath& log_directory) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK_LE(audio_debug_recordings_id, current_audio_debug_recordings_id_);

  base::FilePath prefix_path = GetAudioDebugRecordingsPrefixPath(
      log_directory, audio_debug_recordings_id);
  // Prevent an old posted StopAudioDebugRecordings() call to stop a newer dump.
  // This could happen in a sequence like:
  //   Start(10);  // Start dump 1. Post Stop() to run after 10 seconds.
  //   Stop();     // Manually stop dump 1 before 10 seconds;
  //   Start(20);  // Start dump 2. Posted Stop() for 1 should not stop dump 2.
  if (audio_debug_recordings_id < current_audio_debug_recordings_id_) {
    const bool is_stopped = false;
    std::move(callback).Run(prefix_path.AsUTF8Unsafe(), is_stopped,
                            is_manual_stop);
    return;
  }

  if (!audio_debug_recording_session_) {
    std::move(error_callback).Run("No audio debug recording in progress");
    return;
  }

  audio_debug_recording_session_.reset();

  content::RenderProcessHost* const host =
      content::RenderProcessHost::FromID(render_process_host_id);
  if (host) {
    host->DisableAudioDebugRecordings();
  }

  const bool is_stopped = true;
  std::move(callback).Run(prefix_path.AsUTF8Unsafe(), is_stopped,
                          is_manual_stop);
}
