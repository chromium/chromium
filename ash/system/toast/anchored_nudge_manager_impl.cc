// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/toast/anchored_nudge_manager_impl.h"

#include "ash/public/cpp/system/anchored_nudge_data.h"
#include "ash/system/toast/anchored_nudge.h"
#include "base/containers/contains.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/view_observer.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_client_view.h"

namespace ash {

// A view observer that is used to close the nudge's widget whenever its
// `anchor_view` is deleted.
class AnchoredNudgeManagerImpl::AnchorViewObserver
    : public views::ViewObserver {
 public:
  AnchorViewObserver(AnchoredNudge* anchored_nudge,
                     views::View* anchor_view,
                     AnchoredNudgeManagerImpl* anchored_nudge_manager)
      : anchored_nudge_(anchored_nudge),
        anchor_view_(anchor_view),
        anchored_nudge_manager_(anchored_nudge_manager) {
    anchor_view_->AddObserver(this);
  }

  AnchorViewObserver(const AnchorViewObserver&) = delete;

  AnchorViewObserver& operator=(const AnchorViewObserver&) = delete;

  ~AnchorViewObserver() override {
    if (anchor_view_) {
      anchor_view_->RemoveObserver(this);
    }
  }

  // ViewObserver:
  void OnViewIsDeleting(views::View* observed_view) override {
    HandleAnchorViewIsDeletingOrHiding(observed_view);
  }

  // ViewObserver:
  void OnViewVisibilityChanged(views::View* observed_view,
                               views::View* starting_view) override {
    if (!observed_view->GetVisible()) {
      HandleAnchorViewIsDeletingOrHiding(observed_view);
    }
  }

  void HandleAnchorViewIsDeletingOrHiding(views::View* observed_view) {
    CHECK_EQ(anchor_view_, observed_view);
    const std::string id = anchored_nudge_->id();

    // Make sure the nudge bubble no longer observes the anchor view.
    anchored_nudge_->SetAnchorView(nullptr);
    anchor_view_->RemoveObserver(this);
    anchor_view_ = nullptr;
    anchored_nudge_ = nullptr;
    anchored_nudge_manager_->Cancel(id);
  }

 private:
  // Owned by the views hierarchy.
  raw_ptr<AnchoredNudge> anchored_nudge_;
  raw_ptr<views::View> anchor_view_;

  // Owned by `Shell`.
  raw_ptr<AnchoredNudgeManagerImpl> anchored_nudge_manager_;
};

AnchoredNudgeManagerImpl::AnchoredNudgeManagerImpl() = default;

AnchoredNudgeManagerImpl::~AnchoredNudgeManagerImpl() {
  CloseAllNudges();
}

void AnchoredNudgeManagerImpl::Show(const AnchoredNudgeData& nudge_data) {
  std::string id = nudge_data.id;
  CHECK(!id.empty());

  // If `id` is already in use, cancel the nudge so it can be replaced.
  if (base::Contains(shown_nudges_, id)) {
    Cancel(id);
  }

  views::View* anchor_view = nudge_data.anchor_view;
  // Nudges cannot show without a visible anchor view or without a widget.
  if (!anchor_view->GetVisible() || !anchor_view->GetWidget()) {
    return;
  }

  auto anchored_nudge =
      std::make_unique<AnchoredNudge>(/*delegate=*/this, nudge_data);
  auto* anchored_nudge_ptr = anchored_nudge.get();
  shown_nudges_[id] = anchored_nudge_ptr;

  auto* anchored_nudge_widget =
      views::BubbleDialogDelegate::CreateBubble(std::move(anchored_nudge));

  // Remove accelerator so the nudge won't be closed when pressing the Esc key.
  anchored_nudge_ptr->GetDialogClientView()->RemoveAccelerator(
      ui::Accelerator(ui::VKEY_ESCAPE, ui::EF_NONE));

  // The anchored nudge bubble is not necessarily inside the same window as the
  // widget. `use_anchor_window_bounds` is set to false so an offset is not
  // applied to try to fit it inside the anchor window.
  anchored_nudge_ptr->GetBubbleFrameView()->set_use_anchor_window_bounds(false);

  // The bounds of the bubble need to be updated to reflect that we are not
  // using the anchor window bounds.
  anchored_nudge_ptr->SizeToContents();

  anchored_nudge_widget->Show();

  anchor_view_observers_[id] = std::make_unique<AnchorViewObserver>(
      anchored_nudge_ptr, anchor_view, this);
}

void AnchoredNudgeManagerImpl::Cancel(const std::string& id) {
  if (!base::Contains(shown_nudges_, id)) {
    return;
  }

  auto anchored_nudge_ptr = shown_nudges_[id];

  anchor_view_observers_.erase(id);
  shown_nudges_.erase(id);
  anchored_nudge_ptr->GetWidget()->CloseNow();
}

void AnchoredNudgeManagerImpl::CloseAllNudges() {
  while (!shown_nudges_.empty()) {
    Cancel(/*id=*/shown_nudges_.begin()->first);
  }
}

void AnchoredNudgeManagerImpl::OnNudgeClosed(const std::string& id) {
  Cancel(id);
}

}  // namespace ash
