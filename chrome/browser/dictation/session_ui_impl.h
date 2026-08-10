// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DICTATION_SESSION_UI_IMPL_H_
#define CHROME_BROWSER_DICTATION_SESSION_UI_IMPL_H_

#include <memory>

#include "base/callback_list.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/dictation/session_state.h"
#include "chrome/browser/dictation/session_ui.h"
#include "components/tabs/public/tab_interface.h"

namespace dictation {

class SessionUiDelegate;
class DictationBubbleUi;
class DictationOverlayView;

class SessionUiImpl : public SessionUi {
 public:
  explicit SessionUiImpl(tabs::TabInterface& tab, SessionUiDelegate& delegate);
  ~SessionUiImpl() override;

  SessionUiImpl(const SessionUiImpl&) = delete;
  SessionUiImpl& operator=(const SessionUiImpl&) = delete;

 private:
  friend class DictationSessionUiImplBrowserTest;

  // SessionUi:
  void OnError(StreamType stream_type) override;
  void OnStopped() override;
  void UpdateAudioLevel(float audio_level) override;
  void OnStartedStream(content::GlobalDOMNodeId target_id) override;

  void OnDictationBubbleCloseClicked();
  void OnToggleActiveStreamClicked();
  void OnSessionStateChanged(SessionState state);
  void OnTabWillDetach(tabs::TabInterface* tab,
                       tabs::TabInterface::DetachReason reason);
  void OnTabInserted(tabs::TabInterface* tab);
  void OnTabWillDeactivate(tabs::TabInterface* tab);

  // Using raw_ref because we observe tab close events and synchronously end
  // the session which owns this, so SessionUi will never outlive the tab.
  const base::raw_ref<tabs::TabInterface> tab_;
  // Owns this.
  const base::raw_ref<SessionUiDelegate> controller_;

  base::CallbackListSubscription session_state_changed_subscription_;
  base::CallbackListSubscription tab_detach_subscription_;
  base::CallbackListSubscription tab_insert_subscription_;
  base::CallbackListSubscription tab_will_deactivate_subscription_;

  // This is the main bubble/toast that shows up at the top-center of the
  // screen.
  std::unique_ptr<DictationBubbleUi> bubble_ui_;

  // This is the overlay button that follows the caret.
  std::unique_ptr<DictationOverlayView> overlay_view_;

  base::WeakPtrFactory<SessionUiImpl> weak_ptr_factory_{this};
};

}  // namespace dictation

#endif  // CHROME_BROWSER_DICTATION_SESSION_UI_IMPL_H_
