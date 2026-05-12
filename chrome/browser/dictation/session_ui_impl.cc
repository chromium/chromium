// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dictation/session_ui_impl.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/dictation/session_ui_delegate.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/profile_browser_collection.h"
#include "chrome/browser/ui/views/dictation/dictation_bubble_ui.h"
#include "chrome/browser/ui/views/interaction/browser_elements_views.h"

namespace dictation {

SessionUiImpl::SessionUiImpl(BrowserWindowInterface& window,
                             SessionUiDelegate& delegate)
    : controller_(delegate) {
  views::View* anchor_view =
      BrowserElementsViews::From(&window)->GetView(kTopContainerElementId);
  if (!anchor_view) {
    return;
  }

  bubble_ui_ = std::make_unique<DictationBubbleUi>(
      anchor_view,
      base::BindRepeating(&SessionUiImpl::OnDictationBubbleCloseClicked,
                          base::Unretained(this)));

  // TODO(b/510778034): Determine what we need to make this accessibility
  // friendly.
  bubble_ui_->Show();
}

SessionUiImpl::~SessionUiImpl() = default;

void SessionUiImpl::OnDictationBubbleCloseClicked() {
  controller_->RequestEndSession();
}

}  // namespace dictation
