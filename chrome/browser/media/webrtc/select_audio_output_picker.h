// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_WEBRTC_SELECT_AUDIO_OUTPUT_PICKER_H_
#define CHROME_BROWSER_MEDIA_WEBRTC_SELECT_AUDIO_OUTPUT_PICKER_H_

#include <string>

#include "base/types/expected.h"
#include "content/public/browser/select_audio_output_request.h"

class Browser;

class SelectAudioOutputPicker {
 public:
  static std::unique_ptr<SelectAudioOutputPicker> Create(
      const content::SelectAudioOutputRequest& request);

  SelectAudioOutputPicker() = default;

  SelectAudioOutputPicker(const SelectAudioOutputPicker&) = delete;
  SelectAudioOutputPicker& operator=(const SelectAudioOutputPicker&) = delete;

  virtual ~SelectAudioOutputPicker() = default;

  virtual void Show(Browser* browser,
                    const content::SelectAudioOutputRequest& request,
                    content::SelectAudioOutputCallback callback) = 0;
};

#endif  // CHROME_BROWSER_MEDIA_WEBRTC_SELECT_AUDIO_OUTPUT_PICKER_H_
