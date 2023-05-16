// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_TOAST_ANCHORED_NUDGE_H_
#define ASH_SYSTEM_TOAST_ANCHORED_NUDGE_H_

#include <string>

#include "ash/ash_export.h"
#include "ash/public/cpp/shelf_types.h"
#include "ash/public/cpp/system/anchored_nudge_data.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"

namespace views {
class Widget;
}  // namespace views

namespace ash {

class SystemToastStyle;

// Contents view class for the anchored nudge widget.
class ASH_EXPORT AnchoredNudge : public views::BubbleDialogDelegateView {
 public:
  METADATA_HEADER(AnchoredNudge);

  // Used to notify nudge events to `AnchoredNudgeManagerImpl`.
  class ASH_EXPORT Delegate {
   public:
    virtual ~Delegate() {}

    // Called when the nudge is being destroyed.
    virtual void OnNudgeClosed(const std::string& id) = 0;
  };

  AnchoredNudge(Delegate* delegate, const AnchoredNudgeData& nudge_data);
  AnchoredNudge(const AnchoredNudge&) = delete;
  AnchoredNudge& operator=(const AnchoredNudge&) = delete;
  ~AnchoredNudge() override;

  // views::WidgetDelegate:
  std::unique_ptr<views::NonClientFrameView> CreateNonClientFrameView(
      views::Widget* widget) override;

  const std::string& id() { return id_; }

 private:
  friend class AnchoredNudgeManagerImplTest;

  // Used to notify nudge events to the manager.
  raw_ptr<Delegate> delegate_;

  // Unique id used to find and dismiss the nudge through the manager.
  const std::string id_;

  // Owned by the views hierarchy. Contents view of the anchored nudge.
  raw_ptr<SystemToastStyle> toast_contents_view_ = nullptr;
};

}  // namespace ash

#endif  // ASH_SYSTEM_TOAST_ANCHORED_NUDGE_H_
