// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SCANNER_SCANNER_ACTION_VIEW_MODEL_H_
#define ASH_SCANNER_SCANNER_ACTION_VIEW_MODEL_H_

#include <string>

#include "ash/ash_export.h"
#include "ash/public/cpp/scanner/scanner_action.h"
#include "base/functional/callback_forward.h"

namespace gfx {
struct VectorIcon;
}

namespace ash {

// A view model wrapper around a `ScannerAction`, which handles the conversion
// to a user-facing text string, icon, and a callback.
class ASH_EXPORT ScannerActionViewModel {
 public:
  using ActionFinishedCallback = base::OnceCallback<void(bool success)>;

  explicit ScannerActionViewModel(ScannerAction action);
  ScannerActionViewModel(const ScannerActionViewModel&);
  ScannerActionViewModel& operator=(const ScannerActionViewModel&);
  ~ScannerActionViewModel();

  std::u16string GetText() const;
  const gfx::VectorIcon& GetIcon() const;

  // Converts this action into a `base::OnceClosure` which, when called,
  // executes the action.
  // When the action is finished, `action_finished_callback` is called with a
  // boolean value representing whether the action execution was successful.
  // It is guaranteed that `action_finished_callback` will never be called
  // before the returned closure is called.
  //
  // As the returned closure needs to take ownership of this action, this must
  // be called with an rvalue reference to `this`, such as:
  //     std::move(action).ToCallback(std::move(on_finished))
  //
  // Alternatively, if the intent is to _copy_ the action into the returned
  // closure, explicitly create a copy:
  //     ScannerActionViewModel(action).ToCallback(std::move(on_finished))
  base::OnceClosure ToCallback(
      ActionFinishedCallback action_finished_callback) &&;

 private:
  ScannerAction action_;
};

}  // namespace ash

#endif  // ASH_SCANNER_SCANNER_ACTION_VIEW_MODEL_H_
