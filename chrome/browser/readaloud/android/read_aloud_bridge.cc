// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/readaloud/android/read_aloud_bridge.h"

#include <memory>
#include <string>
#include <vector>

#include "base/android/jni_string.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/readaloud/read_aloud_service_factory.h"
#include "content/public/browser/web_contents.h"
#include "url/android/gurl_android.h"

// JNI header generated automatically by JNI Zero.
#include "chrome/browser/readaloud/android/jni_headers/ReadAloudController_jni.h"

using jni_zero::AttachCurrentThread;
using jni_zero::JavaRef;
using jni_zero::ScopedJavaLocalRef;

namespace readaloud {

ReadAloudBridge::ReadAloudBridge(JNIEnv* env,
                                 const JavaRef<jobject>& j_controller,
                                 ReadAloudService* service)
    : weak_java_controller_(env, j_controller), service_(service) {}

ReadAloudBridge::~ReadAloudBridge() = default;

// ============================================================================
// ReadAloudService::Delegate (Outbound Callbacks: C++ -> Java)
// ============================================================================

void ReadAloudBridge::OnMetadataAvailable(std::string_view title,
                                          std::string_view publisher) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> java_controller = weak_java_controller_.get(env);
  if (!java_controller) {
    return;
  }
  Java_ReadAloudController_onMetadataAvailable(
      env, java_controller, std::string(title), std::string(publisher));
}

void ReadAloudBridge::OnPlaybackProgressUpdated(base::TimeDelta elapsed,
                                                base::TimeDelta duration) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> java_controller = weak_java_controller_.get(env);
  if (!java_controller) {
    return;
  }
  Java_ReadAloudController_onPlaybackProgressUpdated(
      env, java_controller, elapsed.InNanoseconds(), duration.InNanoseconds());
}

void ReadAloudBridge::OnPlaybackStateChanged(
    ReadAloudService::PlaybackState playback_state) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> java_controller = weak_java_controller_.get(env);
  if (!java_controller) {
    return;
  }
  Java_ReadAloudController_onPlaybackStateChanged(
      env, java_controller, static_cast<jint>(playback_state));
}

void ReadAloudBridge::OnVoicesAvailable(
    const std::vector<ReadAloudService::Voice>& voices,
    std::string_view selected_voice_id) {
  std::vector<std::string> ids;
  std::vector<std::string> display_names;
  ids.reserve(voices.size());
  display_names.reserve(voices.size());
  for (const auto& voice : voices) {
    ids.push_back(voice.id);
    display_names.push_back(voice.display_name);
  }

  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> java_controller = weak_java_controller_.get(env);
  if (!java_controller) {
    return;
  }
  Java_ReadAloudController_onVoicesAvailable(
      env, java_controller, ids, display_names, std::string(selected_voice_id));
}

void ReadAloudBridge::OnWordHighlightUpdated(int absolute_start_index,
                                             int absolute_end_index) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> java_controller = weak_java_controller_.get(env);
  if (!java_controller) {
    return;
  }
  Java_ReadAloudController_onWordHighlightUpdated(
      env, java_controller, absolute_start_index, absolute_end_index);
}

void ReadAloudBridge::OnHighlightingSupported(bool supported) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> java_controller = weak_java_controller_.get(env);
  if (!java_controller) {
    return;
  }
  Java_ReadAloudController_onHighlightingSupported(env, java_controller,
                                                   supported);
}

void ReadAloudBridge::OnFallbackEngaged() {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> java_controller = weak_java_controller_.get(env);
  if (!java_controller) {
    return;
  }
  Java_ReadAloudController_onFallbackEngaged(env, java_controller);
}

void ReadAloudBridge::OnPlaybackError(std::string_view error_message) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> java_controller = weak_java_controller_.get(env);
  if (!java_controller) {
    return;
  }
  Java_ReadAloudController_onPlaybackError(env, java_controller,
                                           std::string(error_message));
}

void ReadAloudBridge::OnVoicePreviewPlaybackStateChanged(
    std::string_view voice_id,
    ReadAloudService::PlaybackState playback_state) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> java_controller = weak_java_controller_.get(env);
  if (!java_controller) {
    return;
  }
  Java_ReadAloudController_onVoicePreviewPlaybackStateChanged(
      env, java_controller, std::string(voice_id),
      static_cast<jint>(playback_state));
}

void ReadAloudBridge::OnReadabilityResult(const GURL& url, bool is_readable) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> java_controller = weak_java_controller_.get(env);
  if (!java_controller) {
    return;
  }
  Java_ReadAloudController_onReadabilityResult(env, java_controller, url,
                                               is_readable);
}

void ReadAloudBridge::OnNativeDestroyed() {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> java_controller = weak_java_controller_.get(env);
  if (!java_controller) {
    return;
  }
  Java_ReadAloudController_onNativeDestroyed(env, java_controller);
}

// ============================================================================
// JNI Inbound Methods (Called by Java -> C++)
// ============================================================================

static jlong JNI_ReadAloudController_GetReadAloudService(JNIEnv* env,
                                                         Profile* profile) {
  if (!profile) {
    return 0;
  }
  ReadAloudService* service = ReadAloudServiceFactory::GetForProfile(profile);
  return reinterpret_cast<jlong>(service);
}

static void JNI_ReadAloudController_SetController(
    JNIEnv* env,
    jlong read_aloud_service_ptr,
    const JavaRef<jobject>& j_caller) {
  auto* service = reinterpret_cast<ReadAloudService*>(read_aloud_service_ptr);
  if (!service) {
    return;
  }
  auto bridge = std::make_unique<ReadAloudBridge>(env, j_caller, service);
  service->SetDelegate(std::move(bridge));
}

static void JNI_ReadAloudController_ClearController(
    JNIEnv* env,
    jlong read_aloud_service_ptr) {
  auto* service = reinterpret_cast<ReadAloudService*>(read_aloud_service_ptr);
  if (!service) {
    return;
  }
  service->SetDelegate(nullptr);
}

static void JNI_ReadAloudController_Play(JNIEnv* env,
                                         jlong read_aloud_service_ptr,
                                         content::WebContents* web_contents) {
  auto* service = reinterpret_cast<ReadAloudService*>(read_aloud_service_ptr);
  if (!service) {
    return;
  }
  service->Play(web_contents);
}

static void JNI_ReadAloudController_Pause(JNIEnv* env,
                                          jlong read_aloud_service_ptr) {
  auto* service = reinterpret_cast<ReadAloudService*>(read_aloud_service_ptr);
  if (!service) {
    return;
  }
  service->Pause();
}

static void JNI_ReadAloudController_Stop(JNIEnv* env,
                                         jlong read_aloud_service_ptr) {
  auto* service = reinterpret_cast<ReadAloudService*>(read_aloud_service_ptr);
  if (!service) {
    return;
  }
  service->Stop();
}

static void JNI_ReadAloudController_SeekToWordIndex(
    JNIEnv* env,
    jlong read_aloud_service_ptr,
    jint word_index) {
  auto* service = reinterpret_cast<ReadAloudService*>(read_aloud_service_ptr);
  if (!service) {
    return;
  }
  service->SeekToWordIndex(word_index);
}

static void JNI_ReadAloudController_Seek(JNIEnv* env,
                                         jlong read_aloud_service_ptr,
                                         jlong absolute_time_nanos) {
  auto* service = reinterpret_cast<ReadAloudService*>(read_aloud_service_ptr);
  if (!service) {
    return;
  }
  if (absolute_time_nanos < 0) {
    return;
  }
  service->Seek(base::Nanoseconds(absolute_time_nanos));
}

static void JNI_ReadAloudController_SeekRelative(JNIEnv* env,
                                                 jlong read_aloud_service_ptr,
                                                 jlong offset_nanos) {
  auto* service = reinterpret_cast<ReadAloudService*>(read_aloud_service_ptr);
  if (!service) {
    return;
  }
  service->SeekRelative(base::Nanoseconds(offset_nanos));
}

static void JNI_ReadAloudController_SetPlaybackRate(
    JNIEnv* env,
    jlong read_aloud_service_ptr,
    jfloat rate) {
  auto* service = reinterpret_cast<ReadAloudService*>(read_aloud_service_ptr);
  if (!service) {
    return;
  }
  service->SetPlaybackRate(rate);
}

static void JNI_ReadAloudController_SetVoice(JNIEnv* env,
                                             jlong read_aloud_service_ptr,
                                             const std::string& voice_id) {
  auto* service = reinterpret_cast<ReadAloudService*>(read_aloud_service_ptr);
  if (!service) {
    return;
  }
  service->SetVoice(voice_id);
}

static void JNI_ReadAloudController_PreviewVoice(JNIEnv* env,
                                                 jlong read_aloud_service_ptr,
                                                 const std::string& voice_id) {
  auto* service = reinterpret_cast<ReadAloudService*>(read_aloud_service_ptr);
  if (!service) {
    return;
  }
  service->PreviewVoice(voice_id);
}

static void JNI_ReadAloudController_StopVoicePreview(
    JNIEnv* env,
    jlong read_aloud_service_ptr) {
  auto* service = reinterpret_cast<ReadAloudService*>(read_aloud_service_ptr);
  if (!service) {
    return;
  }
  service->StopVoicePreview();
}

static void JNI_ReadAloudController_SetPlaybackMode(
    JNIEnv* env,
    jlong read_aloud_service_ptr,
    jint mode) {
  auto* service = reinterpret_cast<ReadAloudService*>(read_aloud_service_ptr);
  if (!service) {
    return;
  }
  service->SetPlaybackMode(static_cast<ReadAloudService::PlaybackMode>(mode));
}

static void JNI_ReadAloudController_SetHighlightingEnabled(
    JNIEnv* env,
    jlong read_aloud_service_ptr,
    jboolean enabled) {
  auto* service = reinterpret_cast<ReadAloudService*>(read_aloud_service_ptr);
  if (!service) {
    return;
  }
  service->SetHighlightingEnabled(enabled);
}

static void JNI_ReadAloudController_SendFeedback(JNIEnv* env,
                                                 jlong read_aloud_service_ptr,
                                                 jint feedback_type) {
  auto* service = reinterpret_cast<ReadAloudService*>(read_aloud_service_ptr);
  if (!service) {
    return;
  }
  service->SendFeedback(
      static_cast<ReadAloudService::FeedbackType>(feedback_type));
}

static void JNI_ReadAloudController_CheckReadability(
    JNIEnv* env,
    jlong read_aloud_service_ptr,
    const GURL& url) {
  auto* service = reinterpret_cast<ReadAloudService*>(read_aloud_service_ptr);
  if (!service) {
    return;
  }
  service->CheckReadability(url);
}

}  // namespace readaloud

DEFINE_JNI(ReadAloudController)
