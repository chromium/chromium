// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_AW_SPEECH_RECOGNITION_MANAGER_DELEGATE_H_
#define ANDROID_WEBVIEW_BROWSER_AW_SPEECH_RECOGNITION_MANAGER_DELEGATE_H_

#include "base/compiler_specific.h"
#include "content/public/browser/speech_recognition_event_listener.h"
#include "content/public/browser/speech_recognition_manager_delegate.h"
#include "content/public/browser/speech_recognition_session_config.h"

namespace android_webview {

// Android WebView implementation of the SpeechRecognitionManagerDelgate
// interface.
class AwSpeechRecognitionManagerDelegate
    : public content::SpeechRecognitionManagerDelegate,
      public content::SpeechRecognitionEventListener {
 public:
  AwSpeechRecognitionManagerDelegate();

  AwSpeechRecognitionManagerDelegate(
      const AwSpeechRecognitionManagerDelegate&) = delete;
  AwSpeechRecognitionManagerDelegate& operator=(
      const AwSpeechRecognitionManagerDelegate&) = delete;

  ~AwSpeechRecognitionManagerDelegate() override;

 protected:
  // SpeechRecognitionEventListener methods.
  void OnRecognitionStart(int session_id) override;
  void OnAudioStart(int session_id) override;
  void OnSoundStart(int session_id) override;
  void OnSoundEnd(int session_id) override;
  void OnAudioEnd(int session_id) override;
  void OnRecognitionEnd(int session_id) override;
  void OnRecognitionResults(
      int session_id,
      const std::vector<media::mojom::WebSpeechRecognitionResultPtr>& result)
      override;
  void OnRecognitionError(
      int session_id,
      const media::mojom::SpeechRecognitionError& error) override;
  void OnAudioLevelsChange(int session_id,
                           float volume,
                           float noise_volume) override;

  // SpeechRecognitionManagerDelegate methods.
  void CheckRecognitionIsAllowed(
      int session_id,
      base::OnceCallback<void(bool ask_user, bool is_allowed)> callback)
      override;
  content::SpeechRecognitionEventListener* GetEventListener() override;

 private:
  // Checks for mojom::ViewType::kTabContents host in the UI thread and notifies
  // back the result in the IO thread through |callback|.
  static void CheckRenderFrameType(
      base::OnceCallback<void(bool ask_user, bool is_allowed)> callback,
      int render_process_id,
      int render_frame_id);
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_AW_SPEECH_RECOGNITION_MANAGER_DELEGATE_H_
