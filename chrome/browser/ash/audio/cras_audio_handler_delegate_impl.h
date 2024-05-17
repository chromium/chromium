// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_AUDIO_CRAS_AUDIO_HANDLER_DELEGATE_IMPL_H_
#define CHROME_BROWSER_ASH_AUDIO_CRAS_AUDIO_HANDLER_DELEGATE_IMPL_H_

#include "chromeos/ash/components/audio/cras_audio_handler.h"

namespace ash {

// A Delegate class that implements the CrasAudioHandler::Delegate, to expose
// the chrome browser functionality to ash.
class CrasAudioHandlerDelegateImpl : public CrasAudioHandler::Delegate {
 public:
  CrasAudioHandlerDelegateImpl() = default;
  CrasAudioHandlerDelegateImpl(const CrasAudioHandlerDelegateImpl&) = delete;
  CrasAudioHandlerDelegateImpl& operator=(const CrasAudioHandlerDelegateImpl&) =
      delete;
  ~CrasAudioHandlerDelegateImpl() override = default;

  // CrasAudioHandler::Delegate:
  void OpenSettingsAudioPage() const override;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_AUDIO_CRAS_AUDIO_HANDLER_DELEGATE_IMPL_H_
