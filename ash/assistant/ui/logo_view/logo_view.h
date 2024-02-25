// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_UI_LOGO_VIEW_LOGO_VIEW_H_
#define ASH_ASSISTANT_UI_LOGO_VIEW_LOGO_VIEW_H_

#include <memory>

#include "base/component_export.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

namespace ash {

class COMPONENT_EXPORT(ASSISTANT_UI) LogoView : public views::View {
  METADATA_HEADER(LogoView, views::View)

 public:
  enum class State {
    kUndefined,
    kListening,
    kMic,
    kUserSpeaks,
  };

  LogoView();

  LogoView(const LogoView&) = delete;
  LogoView& operator=(const LogoView&) = delete;

  ~LogoView() override;

  // If |animate| is true, animates to the |state|.
  virtual void SetState(State state, bool animate) {}

  // Set the speech level for kUserSpeaks state. |speech_level| is the last
  // observed speech level in dB.
  virtual void SetSpeechLevel(float speech_level) {}

  // Creates LogoView based on the build flag ENABLE_CROS_LIBASSISTANT.
  static std::unique_ptr<LogoView> Create();
};

}  // namespace ash

#endif  // ASH_ASSISTANT_UI_LOGO_VIEW_LOGO_VIEW_H_
