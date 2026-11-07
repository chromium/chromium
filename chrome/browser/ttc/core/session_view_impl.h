// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TTC_CORE_SESSION_VIEW_IMPL_H_
#define CHROME_BROWSER_TTC_CORE_SESSION_VIEW_IMPL_H_

#include <memory>

#include "base/callback_list.h"
#include "chrome/browser/ttc/core/session_view.h"
#include "chrome/browser/ttc/core/states.h"
#include "chrome/browser/ui/toasts/api/toast_id.h"
#include "chrome/browser/ui/views/ttc/ttc_view_delegate.h"
#include "components/ttc/app/public/error_codes.h"

namespace ttc {

class OverlayController;
class SessionViewDelegate;

ToastId GetToastIdForError(ErrorCode error);

class SessionViewImpl : public SessionView, public TtcViewDelegate {
 public:
  explicit SessionViewImpl(SessionViewDelegate& delegate);
  ~SessionViewImpl() override;
  SessionViewImpl(const SessionViewImpl&) = delete;
  SessionViewImpl& operator=(const SessionViewImpl&) = delete;

  // SessionController::Observer:
  void OnError(ErrorCode error) override;
  void OnUserAudioLevelUpdate(float audio_level) override;

  // TtcViewDelegate:
  void EndSessionAsync() override;
  base::CallbackListSubscription RegisterAudioLevelCallback(
      base::RepeatingCallback<void(float)> callback) override;

 private:
  // SessionView:
  void OnUiStateChanged(UiState state) override;

  // The views that show the audio level subscribe to it here.
  base::RepeatingCallbackList<void(float)> audio_level_callbacks_;

  // Owns the UI the session shows on top of the browser window.
  std::unique_ptr<OverlayController> overlay_controller_;
};

}  // namespace ttc

#endif  // CHROME_BROWSER_TTC_CORE_SESSION_VIEW_IMPL_H_
