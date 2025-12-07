// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_MAC_PASTEBOARD_CHANGED_OBSERVATION_H_
#define BASE_MAC_PASTEBOARD_CHANGED_OBSERVATION_H_

#include "base/base_export.h"
#include "base/callback_list.h"
#include "base/functional/callback_forward.h"

namespace base {

// Registers a callback that will be called whenever the
// NSPasteboard.generalPasteboard has been changed by any process on the system
// (including this one).
BASE_EXPORT CallbackListSubscription
RegisterPasteboardChangedCallback(RepeatingClosure callback);

}  // namespace base

#endif  // BASE_MAC_PASTEBOARD_CHANGED_OBSERVATION_H_
