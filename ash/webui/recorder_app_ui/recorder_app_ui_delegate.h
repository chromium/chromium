// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_RECORDER_APP_UI_RECORDER_APP_UI_DELEGATE_H_
#define ASH_WEBUI_RECORDER_APP_UI_RECORDER_APP_UI_DELEGATE_H_

#include "components/soda/constants.h"

namespace ash {
// A delegate which exposes browser functionality from //chrome to the recorder
// app ui page handler.
class RecorderAppUIDelegate {
 public:
  virtual void InstallSoda(speech::LanguageCode languageCode) = 0;

  virtual ~RecorderAppUIDelegate() = default;
};

}  // namespace ash

#endif  // ASH_WEBUI_RECORDER_APP_UI_RECORDER_APP_UI_DELEGATE_H_
