// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TTC_CORE_MIC_PRIVACY_INDICATOR_CHROMEOS_H_
#define CHROME_BROWSER_TTC_CORE_MIC_PRIVACY_INDICATOR_CHROMEOS_H_

namespace ttc {

// Shows the ChromeOS microphone privacy indicator (the shelf dot and the
// "using your microphone" notification) while a session captures audio. TTC
// opens the microphone itself, so `MediaStreamCaptureIndicator` never reports
// it to Ash.
class MicPrivacyIndicator {
 public:
  MicPrivacyIndicator();
  MicPrivacyIndicator(const MicPrivacyIndicator&) = delete;
  MicPrivacyIndicator& operator=(const MicPrivacyIndicator&) = delete;
  // Hides the indicator: the session's final capture stop never reaches it.
  ~MicPrivacyIndicator();

  void SetCapturing(bool capturing);

 private:
  bool capturing_ = false;
};

}  // namespace ttc

#endif  // CHROME_BROWSER_TTC_CORE_MIC_PRIVACY_INDICATOR_CHROMEOS_H_
