// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SCANNER_SCANNER_ACTION_VIEW_MODEL_H_
#define ASH_SCANNER_SCANNER_ACTION_VIEW_MODEL_H_

#include <string>

#include "ash/ash_export.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "components/manta/proto/scanner.pb.h"

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
      manta::proto::ScannerAction unpopulated_action,
      scoped_refptr<base::RefCountedMemory> downscaled_jpeg_bytes,
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
  manta::proto::ScannerAction::ActionCase GetActionCase() const;

  const manta::proto::ScannerAction& unpopulated_action() const {
    return unpopulated_action_;
  }
  const scoped_refptr<base::RefCountedMemory>& downscaled_jpeg_bytes() const {
    return downscaled_jpeg_bytes_;
  }
  base::WeakPtr<ScannerCommandDelegate> delegate() const { return delegate_; }

 private:
  manta::proto::ScannerAction unpopulated_action_;
  scoped_refptr<base::RefCountedMemory> downscaled_jpeg_bytes_;
  base::WeakPtr<ScannerCommandDelegate> delegate_;
};

}  // namespace ash

#endif  // ASH_SCANNER_SCANNER_ACTION_VIEW_MODEL_H_
