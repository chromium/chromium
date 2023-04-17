// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_AMBIENT_AMBIENT_UI_LAUNCHER_H_
#define ASH_AMBIENT_AMBIENT_UI_LAUNCHER_H_

#include <memory>

#include "ash/ambient/model/ambient_backend_model.h"
#include "ash/constants/ambient_theme.h"
#include "base/functional/callback_forward.h"
#include "ui/views/view.h"

namespace ash {

// AmbientUiLauncher is used to start ambient UIs. Every implementation of
// this abstract class is tied a particular UI (slideshow, animation etc) but it
// is able to launch multiple ambient UI sessions.
//
// Where each ambient UI session starts when the `Initialize` method is called
// for the first time and ends when the `Finalize` method is called.
class AmbientUiLauncher {
 public:
  using InitializationCallback = base::OnceCallback<void(bool success)>;
  virtual ~AmbientUiLauncher() = default;

  // Do any asynchronous initialization before launching the UI. This method is
  // only expected to be run once per ambient UI session.
  virtual void Initialize(InitializationCallback on_done) = 0;

  // After Initialize() is complete, we call this method to create the view,
  // this can be called multiple times during an ambient UI session in case
  // there are multiple screens.
  virtual std::unique_ptr<views::View> CreateView() = 0;

  // Stop any processing and ends the current ambient session. This method is
  // only expected to run once to end the ambient UI session.
  virtual void Finalize() = 0;

  // TODO(esum): Remove when we get rid of the ambient backend model dependency
  // from the ambient controller and PhotoView.
  virtual AmbientBackendModel* GetAmbientBackendModel() = 0;

  // Returns whether an ambient UI session is active.
  virtual bool IsActive() = 0;

  // Returns whether an ambient UI session is ready to be started and the
  // `Intiailize` method can be called. Note: This can potentially disable
  // ambient mode until the next lock/unlock event if this is false on the lock
  // screen.
  virtual bool IsReady() = 0;
};

}  // namespace ash

#endif  // ASH_AMBIENT_AMBIENT_UI_LAUNCHER_H_
