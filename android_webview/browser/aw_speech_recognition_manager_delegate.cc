// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/aw_speech_recognition_manager_delegate.h"

#include <string>

#include "base/bind.h"
#include "base/macros.h"
#include "base/task/post_task.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/speech_recognition_manager.h"
#include "content/public/browser/speech_recognition_session_context.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/mojom/speech/speech_recognition_error.mojom.h"
#include "third_party/blink/public/mojom/speech/speech_recognition_result.mojom.h"

using content::BrowserThread;

namespace android_webview {

AwSpeechRecognitionManagerDelegate::AwSpeechRecognitionManagerDelegate() {}

AwSpeechRecognitionManagerDelegate::~AwSpeechRecognitionManagerDelegate() {}

void AwSpeechRecognitionManagerDelegate::OnRecognitionStart(int session_id) {}

void AwSpeechRecognitionManagerDelegate::OnAudioStart(int session_id) {}

void AwSpeechRecognitionManagerDelegate::OnEnvironmentEstimationComplete(
    int session_id) {}

void AwSpeechRecognitionManagerDelegate::OnSoundStart(int session_id) {}

void AwSpeechRecognitionManagerDelegate::OnSoundEnd(int session_id) {}

void AwSpeechRecognitionManagerDelegate::OnAudioEnd(int session_id) {}

void AwSpeechRecognitionManagerDelegate::OnRecognitionResults(
    int session_id,
    const std::vector<blink::mojom::SpeechRecognitionResultPtr>& result) {}

void AwSpeechRecognitionManagerDelegate::OnRecognitionError(
    int session_id,
    const blink::mojom::SpeechRecognitionError& error) {}

void AwSpeechRecognitionManagerDelegate::OnAudioLevelsChange(
    int session_id,
    float volume,
    float noise_volume) {}

void AwSpeechRecognitionManagerDelegate::OnRecognitionEnd(int session_id) {}

void AwSpeechRecognitionManagerDelegate::CheckRecognitionIsAllowed(
    int session_id,
    base::OnceCallback<void(bool ask_user, bool is_allowed)> callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  const content::SpeechRecognitionSessionContext& context =
      content::SpeechRecognitionManager::GetInstance()->GetSessionContext(
          session_id);

  // Make sure that initiators (extensions/web pages) properly set the
  // |render_process_id| field, which is needed later to retrieve the profile.
  DCHECK_NE(context.render_process_id, 0);

  int render_process_id = context.render_process_id;
  int render_frame_id = context.render_frame_id;
  if (context.embedder_render_process_id) {
    // If this is a request originated from a guest, we need to re-route the
    // permission check through the embedder (app).
    render_process_id = context.embedder_render_process_id;
    render_frame_id = context.embedder_render_frame_id;
  }

  // Check that the render frame type is appropriate, and whether or not we
  // need to request permission from the user.
  base::PostTask(FROM_HERE, {BrowserThread::UI},
                 base::BindOnce(&CheckRenderFrameType, std::move(callback),
                                render_process_id, render_frame_id));
}

content::SpeechRecognitionEventListener*
AwSpeechRecognitionManagerDelegate::GetEventListener() {
  return this;
}

bool AwSpeechRecognitionManagerDelegate::FilterProfanities(
    int render_process_id) {
  // TODO: to confirm whether this setting is relevant for android,
  // https://crbug.com/876801.
  return false;
}

// static.
void AwSpeechRecognitionManagerDelegate::CheckRenderFrameType(
    base::OnceCallback<void(bool ask_user, bool is_allowed)> callback,
    int render_process_id,
    int render_frame_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // Regular tab contents.
  base::PostTask(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(std::move(callback), true /* check_permission */,
                     true /* allowed */));
}

}  // namespace android_webview
