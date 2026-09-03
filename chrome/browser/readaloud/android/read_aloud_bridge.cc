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
#include "chrome/browser/readaloud/android/jni_headers/ReadAloudNativeBridge_jni.h"

using jni_zero::AttachCurrentThread;
using jni_zero::JavaRef;
using jni_zero::ScopedJavaLocalRef;

namespace readaloud {

ReadAloudBridge::ReadAloudBridge(JNIEnv* env,
                                 const JavaRef<jobject>& j_native_bridge,
                                 ReadAloudService* service)
    : weak_java_native_bridge_(env, j_native_bridge), service_(service) {}

ReadAloudBridge::~ReadAloudBridge() = default;

// ============================================================================
// ReadAloudService::Delegate (Outbound Callbacks: C++ -> Java)
// ============================================================================

void ReadAloudBridge::OnMetadataAvailable(std::string_view title,
                                          std::string_view publisher) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> j_bridge = weak_java_native_bridge_.get(env);
  if (!j_bridge) {
    return;
  }
  Java_ReadAloudNativeBridge_onMetadataAvailable(
      env, j_bridge, std::string(title), std::string(publisher));
}

void ReadAloudBridge::OnPlaybackProgressUpdated(base::TimeDelta elapsed,
                                                base::TimeDelta duration) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> j_bridge = weak_java_native_bridge_.get(env);
  if (!j_bridge) {
    return;
  }
  Java_ReadAloudNativeBridge_onPlaybackProgressUpdated(
      env, j_bridge, elapsed.InNanoseconds(), duration.InNanoseconds());
}

void ReadAloudBridge::OnPlaybackStateChanged(
    ReadAloudService::PlaybackState playback_state) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> j_bridge = weak_java_native_bridge_.get(env);
  if (!j_bridge) {
    return;
  }
  Java_ReadAloudNativeBridge_onPlaybackStateChanged(
      env, j_bridge, static_cast<jint>(playback_state));
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
  ScopedJavaLocalRef<jobject> j_bridge = weak_java_native_bridge_.get(env);
  if (!j_bridge) {
    return;
  }
  Java_ReadAloudNativeBridge_onVoicesAvailable(
      env, j_bridge, ids, display_names, std::string(selected_voice_id));
}

void ReadAloudBridge::OnWordHighlightUpdated(int absolute_start_index,
                                             int absolute_end_index) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> j_bridge = weak_java_native_bridge_.get(env);
  if (!j_bridge) {
    return;
  }
  Java_ReadAloudNativeBridge_onWordHighlightUpdated(
      env, j_bridge, absolute_start_index, absolute_end_index);
}

void ReadAloudBridge::OnTextChunked(const std::vector<std::u16string>& chunks) {
  // TODO(crbug.com/524283143)): JNI Bridge for Text Chunks.
}

void ReadAloudBridge::OnHighlightingSupported(bool supported) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> j_bridge = weak_java_native_bridge_.get(env);
  if (!j_bridge) {
    return;
  }
  Java_ReadAloudNativeBridge_onHighlightingSupported(env, j_bridge, supported);
}

void ReadAloudBridge::OnFallbackEngaged() {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> j_bridge = weak_java_native_bridge_.get(env);
  if (!j_bridge) {
    return;
  }
  Java_ReadAloudNativeBridge_onFallbackEngaged(env, j_bridge);
}

void ReadAloudBridge::OnPlaybackError(std::string_view error_message) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> j_bridge = weak_java_native_bridge_.get(env);
  if (!j_bridge) {
    return;
  }
  Java_ReadAloudNativeBridge_onPlaybackError(env, j_bridge,
                                             std::string(error_message));
}

void ReadAloudBridge::OnVoicePreviewPlaybackStateChanged(
    std::string_view voice_id,
    ReadAloudService::PlaybackState playback_state) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> j_bridge = weak_java_native_bridge_.get(env);
  if (!j_bridge) {
    return;
  }
  Java_ReadAloudNativeBridge_onVoicePreviewPlaybackStateChanged(
      env, j_bridge, std::string(voice_id), static_cast<jint>(playback_state));
}

void ReadAloudBridge::OnReadabilityResult(const GURL& url, bool is_readable) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> j_bridge = weak_java_native_bridge_.get(env);
  if (!j_bridge) {
    return;
  }
  Java_ReadAloudNativeBridge_onReadabilityResult(env, j_bridge, url,
                                                 is_readable);
}

void ReadAloudBridge::OnNativeDestroyed() {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> j_bridge = weak_java_native_bridge_.get(env);
  if (j_bridge) {
    Java_ReadAloudNativeBridge_onNativeDestroyed(env, j_bridge);
  }
  weak_java_native_bridge_.reset();
  service_ = nullptr;
}

// ============================================================================
// JNI Inbound Methods (Java -> C++ Commands)
// ============================================================================

static jlong JNI_ReadAloudNativeBridge_Init(
    JNIEnv* env,
    Profile* profile,
    const JavaRef<jobject>& j_native_bridge) {
  if (!profile) {
    return 0;
  }
  ReadAloudService* service = ReadAloudServiceFactory::GetForProfile(profile);
  if (!service) {
    return 0;
  }
  auto bridge =
      std::make_unique<ReadAloudBridge>(env, j_native_bridge, service);
  auto* bridge_ptr = bridge.get();
  service->SetDelegate(std::move(bridge));
  return reinterpret_cast<jlong>(bridge_ptr);
}

void ReadAloudBridge::InitializeSession(JNIEnv* env,
                                        content::WebContents* web_contents) {
  if (service_) {
    service_->Initialize(web_contents);
  }
}

void ReadAloudBridge::Play(JNIEnv* env, content::WebContents* web_contents) {
  if (service_) {
    service_->Play(web_contents);
  }
}

void ReadAloudBridge::Pause(JNIEnv* env) {
  if (service_) {
    service_->Pause();
  }
}

void ReadAloudBridge::Stop(JNIEnv* env) {
  if (service_) {
    service_->Stop();
  }
}

void ReadAloudBridge::SeekToWordIndex(JNIEnv* env, jint word_index) {
  if (service_) {
    service_->SeekToWordIndex(word_index);
  }
}

void ReadAloudBridge::Seek(JNIEnv* env, jlong absolute_time_nanos) {
  if (service_ && absolute_time_nanos >= 0) {
    service_->Seek(base::Nanoseconds(absolute_time_nanos));
  }
}

void ReadAloudBridge::SeekRelative(JNIEnv* env, jlong offset_nanos) {
  if (service_) {
    service_->SeekRelative(base::Nanoseconds(offset_nanos));
  }
}

void ReadAloudBridge::SetPlaybackRate(JNIEnv* env, jfloat rate) {
  if (service_) {
    service_->SetPlaybackRate(rate);
  }
}

void ReadAloudBridge::SetVoice(JNIEnv* env, const std::string& voice_id) {
  if (service_) {
    service_->SetVoice(voice_id);
  }
}

void ReadAloudBridge::PreviewVoice(JNIEnv* env, const std::string& voice_id) {
  if (service_) {
    service_->PreviewVoice(voice_id);
  }
}

void ReadAloudBridge::StopVoicePreview(JNIEnv* env) {
  if (service_) {
    service_->StopVoicePreview();
  }
}

void ReadAloudBridge::SetPlaybackMode(JNIEnv* env, jint mode) {
  if (service_) {
    service_->SetPlaybackMode(
        static_cast<ReadAloudService::PlaybackMode>(mode));
  }
}

void ReadAloudBridge::SetHighlightingEnabled(JNIEnv* env, jboolean enabled) {
  if (service_) {
    service_->SetHighlightingEnabled(enabled);
  }
}

void ReadAloudBridge::SendFeedback(JNIEnv* env, jint feedback_type) {
  if (service_) {
    service_->SendFeedback(
        static_cast<ReadAloudService::FeedbackType>(feedback_type));
  }
}

void ReadAloudBridge::CheckReadability(JNIEnv* env, const GURL& url) {
  if (service_) {
    service_->CheckReadability(url);
  }
}

void ReadAloudBridge::Destroy(JNIEnv* env) {
  weak_java_native_bridge_.reset();
  if (service_) {
    service_->SetDelegate(nullptr);
  }
}

}  // namespace readaloud

DEFINE_JNI(ReadAloudNativeBridge)
