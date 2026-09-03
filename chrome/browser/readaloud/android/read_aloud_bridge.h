// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_READALOUD_ANDROID_READ_ALOUD_BRIDGE_H_
#define CHROME_BROWSER_READALOUD_ANDROID_READ_ALOUD_BRIDGE_H_

#include <string_view>
#include <vector>

#include "base/android/jni_android.h"
#include "base/android/jni_weak_ref.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/readaloud/read_aloud_service.h"
#include "content/public/browser/web_contents.h"
#include "third_party/jni_zero/jni_zero.h"
#include "url/gurl.h"

namespace readaloud {

// C++ JNI bridge class managing 2-way communications between native
// ReadAloudService and Java ReadAloudNativeBridge.
//
// Objective:
// Implements ReadAloudService::Delegate to marshal engine events (metadata,
// playback progress, state transitions, readability results) from C++ to Java,
// and hosts JNI Zero instance method dispatch targets for inbound Java playback
// control commands (Play, Pause, Stop, Seek, etc.).
//
// Lifecycle & Threading:
// - Instantiated in JNI_ReadAloudNativeBridge_Init() when the Java
//   ReadAloudNativeBridge initializes for a Profile.
// - Registered as the ReadAloudService::Delegate via service->SetDelegate().
// - Retains a JavaObjectWeakGlobalRef to ReadAloudNativeBridge to prevent
// Activity leaks.
// - Destroyed when ReadAloudNativeBridge.destroy() calls Destroy() or when
//   the profile/ReadAloudService is torn down.
// - All JNI calls and delegate callbacks execute on the UI thread.
class ReadAloudBridge : public ReadAloudService::Delegate {
 public:
  ReadAloudBridge(JNIEnv* env,
                  const jni_zero::JavaRef<jobject>& j_native_bridge,
                  ReadAloudService* service);

  ReadAloudBridge(const ReadAloudBridge&) = delete;
  ReadAloudBridge& operator=(const ReadAloudBridge&) = delete;

  ~ReadAloudBridge() override;

  // ReadAloudService::Delegate (C++ -> Java Callbacks):
  void OnMetadataAvailable(std::string_view title,
                           std::string_view publisher) override;
  void OnPlaybackProgressUpdated(base::TimeDelta elapsed,
                                 base::TimeDelta duration) override;
  void OnPlaybackStateChanged(
      ReadAloudService::PlaybackState playback_state) override;
  void OnVoicesAvailable(const std::vector<ReadAloudService::Voice>& voices,
                         std::string_view selected_voice_id) override;
  void OnWordHighlightUpdated(int absolute_start_index,
                              int absolute_end_index) override;
  void OnTextChunked(const std::vector<std::u16string>& chunks) override;
  void OnHighlightingSupported(bool supported) override;
  void OnFallbackEngaged() override;
  void OnPlaybackError(std::string_view error_message) override;
  void OnVoicePreviewPlaybackStateChanged(
      std::string_view voice_id,
      ReadAloudService::PlaybackState playback_state) override;
  void OnReadabilityResult(const GURL& url, bool is_readable) override;
  void OnNativeDestroyed() override;

  // JNI Zero instance method dispatch targets (Java -> C++ Commands):
  void InitializeSession(JNIEnv* env, content::WebContents* web_contents);
  void Play(JNIEnv* env, content::WebContents* web_contents);
  void Pause(JNIEnv* env);
  void Stop(JNIEnv* env);
  void SeekToWordIndex(JNIEnv* env, jint word_index);
  void Seek(JNIEnv* env, jlong absolute_time_nanos);
  void SeekRelative(JNIEnv* env, jlong offset_nanos);
  void SetPlaybackRate(JNIEnv* env, jfloat rate);
  void SetVoice(JNIEnv* env, const std::string& voice_id);
  void PreviewVoice(JNIEnv* env, const std::string& voice_id);
  void StopVoicePreview(JNIEnv* env);
  void SetPlaybackMode(JNIEnv* env, jint mode);
  void SetHighlightingEnabled(JNIEnv* env, jboolean enabled);
  void SendFeedback(JNIEnv* env, jint feedback_type);
  void CheckReadability(JNIEnv* env, const GURL& url);
  void Destroy(JNIEnv* env);

 private:
  // Weak global reference to Java ReadAloudNativeBridge to prevent Activity
  // leaks.
  JavaObjectWeakGlobalRef weak_java_native_bridge_;
  // ReadAloudService pointer (for handling Java -> native calls).
  raw_ptr<ReadAloudService> service_;
};

}  // namespace readaloud

#endif  // CHROME_BROWSER_READALOUD_ANDROID_READ_ALOUD_BRIDGE_H_
