// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DICTATION_SESSION_CONTROLLER_DELEGATE_H_
#define CHROME_BROWSER_DICTATION_SESSION_CONTROLLER_DELEGATE_H_

#include <memory>

#include "base/callback_list.h"

namespace dictation {

class SessionController;
class StreamProvider;
class Ui;

enum class Mode { kDisabled, kEnabled };

// Interface for a Profile-level delegate for the dictation session. The
// SessionControllerDelegate class is responsible creation of the concrete
// implementations of various objects
class SessionControllerDelegate {
 public:
  using ModeChangeCallbackList =
      base::RepeatingCallbackList<void(Mode /*new_mode*/)>;

  virtual ~SessionControllerDelegate() = default;

  // Registers a callback to be called whenever the current operating mode
  // changes. The callback is additionally called on registration.
  virtual base::CallbackListSubscription RegisterModeChangeCallback(
      ModeChangeCallbackList::CallbackType cb) = 0;

  virtual std::unique_ptr<StreamProvider> CreateStreamProvider(
      SessionController& controller) const = 0;
  virtual std::unique_ptr<Ui> CreateUi(SessionController& controller) const = 0;
};

}  // namespace dictation

#endif  // CHROME_BROWSER_DICTATION_SESSION_CONTROLLER_DELEGATE_H_
