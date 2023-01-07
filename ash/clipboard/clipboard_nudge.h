// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CLIPBOARD_CLIPBOARD_NUDGE_H_
#define ASH_CLIPBOARD_CLIPBOARD_NUDGE_H_

#include "ash/ash_export.h"
#include "ash/clipboard/clipboard_nudge_constants.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_observer.h"
#include "ash/system/tray/system_nudge.h"
#include "base/scoped_observation.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/widget/unique_widget_ptr.h"
#include "ui/views/widget/widget.h"

namespace ash {

// Implements a contextual nudge for multipaste.
class ASH_EXPORT ClipboardNudge : public SystemNudge {
 public:
  explicit ClipboardNudge(ClipboardNudgeType nudge_type);
  ClipboardNudge(const ClipboardNudge&) = delete;
  ClipboardNudge& operator=(const ClipboardNudge&) = delete;
  ~ClipboardNudge() override;

  ClipboardNudgeType nudge_type() { return nudge_type_; }

 protected:
  // SystemNudge:
  std::unique_ptr<SystemNudgeLabel> CreateLabelView() const override;
  const gfx::VectorIcon& GetIcon() const override;
  std::u16string GetAccessibilityText() const override;

 private:
  ClipboardNudgeType nudge_type_;
};

}  // namespace ash

#endif  // ASH_CLIPBOARD_CLIPBOARD_NUDGE_H_