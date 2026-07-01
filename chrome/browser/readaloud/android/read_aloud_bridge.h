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
#include "third_party/jni_zero/jni_zero.h"
#include "url/gurl.h"

namespace readaloud {

// C++ JNI bridge that implements ReadAloudService::Delegate to marshal events
// from the native service to the Java ReadAloudController, and hosts the JNI
// entry points for inbound controller commands.
class ReadAloudBridge : public ReadAloudService::Delegate {
 public:
  ReadAloudBridge(JNIEnv* env,
                  const jni_zero::JavaRef<jobject>& j_controller,
                  ReadAloudService* service);

  ReadAloudBridge(const ReadAloudBridge&) = delete;
  ReadAloudBridge& operator=(const ReadAloudBridge&) = delete;

  ~ReadAloudBridge() override;

  // ReadAloudService::Delegate:
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
  void OnHighlightingSupported(bool supported) override;
  void OnFallbackEngaged() override;
  void OnPlaybackError(std::string_view error_message) override;
  void OnVoicePreviewPlaybackStateChanged(
      std::string_view voice_id,
      ReadAloudService::PlaybackState playback_state) override;
  void OnReadabilityResult(const GURL& url, bool is_readable) override;
  void OnNativeDestroyed() override;

 private:
  // Weak global reference to the Java ReadAloudController instance to prevent
  // Activity leaks.
  JavaObjectWeakGlobalRef weak_java_controller_;
  // Pointer to the native service owning this delegate.
  raw_ptr<ReadAloudService> service_;
};

}  // namespace readaloud

#endif  // CHROME_BROWSER_READALOUD_ANDROID_READ_ALOUD_BRIDGE_H_
