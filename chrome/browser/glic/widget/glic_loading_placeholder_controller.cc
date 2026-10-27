// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/widget/glic_loading_placeholder_controller.h"

#include <memory>
#include <utility>

#include "base/check_deref.h"
#include "chrome/browser/glic/widget/glic_view.h"
#include "content/public/browser/web_contents.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/native/native_view_host.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace glic {

GlicLoadingPlaceholderController::GlicLoadingPlaceholderController(
    GlicView* glic_view)
    : glic_view_(CHECK_DEREF(glic_view)) {}

GlicLoadingPlaceholderController::~GlicLoadingPlaceholderController() {
  ResetCapturer();
}

void GlicLoadingPlaceholderController::Configure(
    GlicLoadingPlaceholderView::Mode mode,
    base::RepeatingClosure close_callback) {
  views::FocusManager* focus_manager = glic_view_->GetFocusManager();
  const views::View* focused_view =
      focus_manager ? focus_manager->GetFocusedView() : nullptr;
  const views::View* stored_focus_view =
      focus_manager ? focus_manager->GetStoredFocusView() : nullptr;
  const bool placeholder_had_focus = placeholder_view_ && focused_view &&
                                     placeholder_view_->Contains(focused_view);
  const bool placeholder_had_stored_focus =
      placeholder_view_ && stored_focus_view &&
      placeholder_view_->Contains(stored_focus_view);

  mode_ = mode;
  close_callback_ = std::move(close_callback);
  if (placeholder_view_) {
    glic_view_->RemoveChildViewT(std::exchange(placeholder_view_, nullptr));
  }
  if (!is_showing_) {
    return;
  }
  EnsurePlaceholderView();
  if (placeholder_view_) {
    if (placeholder_had_focus) {
      placeholder_view_->RequestFocus();
    } else if (placeholder_had_stored_focus && focus_manager) {
      focus_manager->SetStoredFocusView(placeholder_view_);
    }
  }
}

void GlicLoadingPlaceholderController::SetShowing(bool showing) {
  views::FocusManager* focus_manager = glic_view_->GetFocusManager();
  const views::View* focused_view =
      focus_manager ? focus_manager->GetFocusedView() : nullptr;
  const views::View* stored_focus_view =
      focus_manager ? focus_manager->GetStoredFocusView() : nullptr;
  const bool view_had_focus_outside_placeholder =
      showing && focused_view && glic_view_->Contains(focused_view) &&
      (!placeholder_view_ || !placeholder_view_->Contains(focused_view));
  const bool view_had_stored_focus_outside_placeholder =
      showing && stored_focus_view && glic_view_->Contains(stored_focus_view) &&
      (!placeholder_view_ || !placeholder_view_->Contains(stored_focus_view));
  const bool placeholder_had_focus = !showing && placeholder_view_ &&
                                     focused_view &&
                                     placeholder_view_->Contains(focused_view);
  const bool placeholder_had_stored_focus =
      !showing && placeholder_view_ && stored_focus_view &&
      placeholder_view_->Contains(stored_focus_view);

  is_showing_ = showing;
  if (showing) {
    restore_focus_on_web_contents_attach_ = false;
    restore_stored_focus_on_web_contents_attach_ = false;
    EnsurePlaceholderView();
  }
  if (placeholder_view_) {
    placeholder_view_->SetVisible(showing);
    placeholder_view_->SetBoundsRect(glic_view_->GetContentsBounds());
  }
  glic_view_->holder()->SetVisible(!showing);
  SyncFocusAccessibilityState();
  UpdateLoadingCapturer();
  if (showing && placeholder_view_) {
    if (view_had_focus_outside_placeholder) {
      placeholder_view_->RequestFocus();
    } else if (view_had_stored_focus_outside_placeholder && focus_manager) {
      focus_manager->SetStoredFocusView(placeholder_view_);
    }
  } else if (!showing) {
    if (placeholder_had_focus || restore_focus_on_web_contents_attach_) {
      if (content::WebContents* web_contents = glic_view_->web_contents()) {
        restore_focus_on_web_contents_attach_ = false;
        restore_stored_focus_on_web_contents_attach_ = false;
        glic_view_->RequestFocus();
        web_contents->Focus();
      } else {
        restore_focus_on_web_contents_attach_ = true;
      }
    } else if (placeholder_had_stored_focus ||
               restore_stored_focus_on_web_contents_attach_) {
      if (glic_view_->web_contents()) {
        restore_stored_focus_on_web_contents_attach_ = false;
        if (focus_manager) {
          focus_manager->SetStoredFocusView(&glic_view_.get());
        }
      } else {
        restore_stored_focus_on_web_contents_attach_ = true;
      }
    }
  }
  if (focus_manager && glic_view_->HasFocus() &&
      !glic_view_->GetViewAccessibility().IsAccessibilityFocusable()) {
    focus_manager->AdvanceFocusIfNecessary();
  }
}

void GlicLoadingPlaceholderController::OnWebContentsAttached(
    content::WebContents* new_web_contents) {
  SyncFocusAccessibilityState();
  glic_view_->holder()->SetVisible(!is_showing_);
  UpdateLoadingCapturer();
  if (new_web_contents && !is_showing_) {
    if (restore_focus_on_web_contents_attach_) {
      restore_focus_on_web_contents_attach_ = false;
      restore_stored_focus_on_web_contents_attach_ = false;
      glic_view_->RequestFocus();
      new_web_contents->Focus();
    } else if (restore_stored_focus_on_web_contents_attach_) {
      restore_stored_focus_on_web_contents_attach_ = false;
      if (glic_view_->GetWidget() && glic_view_->GetWidget()->IsActive()) {
        glic_view_->RequestFocus();
        new_web_contents->Focus();
      } else if (views::FocusManager* focus_manager =
                     glic_view_->GetFocusManager()) {
        focus_manager->SetStoredFocusView(&glic_view_.get());
      }
    }
  }
}

bool GlicLoadingPlaceholderController::MaybeRequestFocus() {
  if (is_showing_ && placeholder_view_) {
    placeholder_view_->RequestFocus();
    return true;
  }
  return false;
}

void GlicLoadingPlaceholderController::UpdateBounds() {
  if (placeholder_view_) {
    placeholder_view_->SetBoundsRect(glic_view_->GetContentsBounds());
  }
}

bool GlicLoadingPlaceholderController::IsPointWithinDraggableRegion(
    const gfx::Point& point_in_glic_view) const {
  if (!is_showing_ || !placeholder_view_) {
    return false;
  }
  const gfx::Point point_in_placeholder = views::View::ConvertPointToTarget(
      &glic_view_.get(), placeholder_view_.get(), point_in_glic_view);
  return placeholder_view_->IsPointWithinDraggableHeader(point_in_placeholder);
}

void GlicLoadingPlaceholderController::SetBackgroundRoundedCorners(
    const gfx::RoundedCornersF& radii) {
  if (placeholder_view_) {
    placeholder_view_->SetBackgroundRoundedCorners(radii);
  }
}

void GlicLoadingPlaceholderController::ResetCapturer() {
  loading_capturer_runner_.RunAndReset();
}

void GlicLoadingPlaceholderController::SyncFocusAccessibilityState() {
  glic_view_->GetViewAccessibility().UpdateFocusableState();
  glic_view_->GetViewAccessibility().SetHasFocusableAncestorRecursive(
      glic_view_->GetFocusBehavior() != views::View::FocusBehavior::NEVER);
}

void GlicLoadingPlaceholderController::EnsurePlaceholderView() {
  if (placeholder_view_) {
    return;
  }
  auto placeholder =
      std::make_unique<GlicLoadingPlaceholderView>(mode_, close_callback_);
  placeholder->SetBackgroundRoundedCorners(
      glic_view_->background_rounded_corners());
  placeholder->SetBoundsRect(glic_view_->GetContentsBounds());
  placeholder->SetVisible(is_showing_);
  placeholder_view_ = glic_view_->AddChildView(std::move(placeholder));
  SyncFocusAccessibilityState();
}

void GlicLoadingPlaceholderController::UpdateLoadingCapturer() {
  loading_capturer_runner_.RunAndReset();
  if (is_showing_ && glic_view_->web_contents()) {
    loading_capturer_runner_ =
        glic_view_->web_contents()->IncrementCapturerCount(
            gfx::Size(), /*stay_hidden=*/false, /*stay_awake=*/true,
            /*is_activity=*/false);
  }
}

}  // namespace glic
