// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dictation/session_ui_impl.h"

#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/dictation/session_ui_delegate.h"
#include "chrome/browser/profiles/profile.h"

namespace dictation {

SessionUiImpl::SessionUiImpl(BrowserWindowInterface& window,
                             SessionUiDelegate& delegate)
    : controller_(delegate) {}

SessionUiImpl::~SessionUiImpl() = default;

}  // namespace dictation
