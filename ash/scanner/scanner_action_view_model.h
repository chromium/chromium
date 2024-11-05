// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SCANNER_SCANNER_ACTION_VIEW_MODEL_H_
#define ASH_SCANNER_SCANNER_ACTION_VIEW_MODEL_H_

#include <string>

#include "ash/ash_export.h"
#include "ash/scanner/scanner_action_handler.h"
#include "ash/scanner/scanner_unpopulated_action.h"
#include "base/memory/weak_ptr.h"

namespace gfx {
struct VectorIcon;
}

namespace ash {

class ScannerCommandDelegate;

// A view model wrapper around a `ScannerUnpopulatedAction`, which handles the
// conversion to a user-facing text string, icon, and a callback.
class ASH_EXPORT ScannerActionViewModel {
 public:
  explicit ScannerActionViewModel(
      ScannerUnpopulatedAction unpopulated_action,
      base::WeakPtr<ScannerCommandDelegate> delegate);
  ScannerActionViewModel(const ScannerActionViewModel&);
  ScannerActionViewModel& operator=(const ScannerActionViewModel&);
  ScannerActionViewModel(ScannerActionViewModel&&);
  ScannerActionViewModel& operator=(ScannerActionViewModel&&);
  ~ScannerActionViewModel();

  // Gets the UI facing text for this action.
  // This may crash if this action has been previously moved.
  std::u16string GetText() const;
  const gfx::VectorIcon& GetIcon() const;

  // Executes this action, running the provided callback with a success value
  // when the execution finishes.
  void ExecuteAction(ScannerCommandCallback action_finished_callback) const;

 private:
  ScannerUnpopulatedAction unpopulated_action_;
  base::WeakPtr<ScannerCommandDelegate> delegate_;
};

}  // namespace ash

#endif  // ASH_SCANNER_SCANNER_ACTION_VIEW_MODEL_H_
