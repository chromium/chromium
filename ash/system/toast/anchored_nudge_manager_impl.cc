// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/toast/anchored_nudge_manager_impl.h"

#include <algorithm>
#include <memory>
#include <string>

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/system/anchored_nudge_data.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/system/toast/anchored_nudge.h"
#include "base/containers/contains.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "ui/aura/window.h"
#include "ui/events/event.h"
#include "ui/events/event_observer.h"
#include "ui/events/types/event_type.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/event_monitor.h"
#include "ui/views/view.h"
#include "ui/views/view_observer.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_client_view.h"

namespace ash {

namespace {

std::string GetNudgeTimeToActionHistogramName(const base::TimeDelta& time) {
  std::string time_range;
  if (time <= base::Minutes(1)) {
    return "Ash.NotifierFramework.Nudge.TimeToAction.Within1m";
  }
  if (time <= base::Hours(1)) {
    return "Ash.NotifierFramework.Nudge.TimeToAction.Within1h";
  }
  return "Ash.NotifierFramework.Nudge.TimeToAction.WithinSession";
}

}  // namespace

///////////////////////////////////////////////////////////////////////////////
//  NudgeHoverObserver
class AnchoredNudgeManagerImpl::NudgeHoverObserver : public ui::EventObserver {
 public:
  NudgeHoverObserver(aura::Window* widget_window,
                     const std::string& nudge_id,
                     HoverStateChangeCallback hover_state_change_callback,
                     AnchoredNudgeManagerImpl* anchored_Nudge_mananger)
      : event_monitor_(views::EventMonitor::CreateWindowMonitor(
            /*event_observer=*/this,
            widget_window,
            {ui::ET_MOUSE_ENTERED, ui::ET_MOUSE_EXITED})),
        nudge_id_(nudge_id),
        hover_state_change_callback_(std::move(hover_state_change_callback)),
        anchored_nudge_manager_(anchored_Nudge_mananger) {}

  NudgeHoverObserver(const NudgeHoverObserver&) = delete;

  NudgeHoverObserver& operator=(const NudgeHoverObserver&) = delete;

  ~NudgeHoverObserver() override = default;

  // ui::EventObserver:
  void OnEvent(const ui::Event& event) override {
    switch (event.type()) {
      case ui::ET_MOUSE_ENTERED:
        anchored_nudge_manager_->OnNudgeHoverStateChanged(
            /*nudge_id=*/nudge_id_,
            /*is_hovering=*/true);
        if (!hover_state_change_callback_.is_null()) {
          std::move(hover_state_change_callback_).Run(true);
        }
        break;
      case ui::ET_MOUSE_EXITED:
        anchored_nudge_manager_->OnNudgeHoverStateChanged(
            /*nudge_id=*/nudge_id_,
            /*is_hovering=*/false);
        if (!hover_state_change_callback_.is_null()) {
          std::move(hover_state_change_callback_).Run(false);
        }
        break;
      default:
        NOTREACHED();
        break;
    }
  }

 private:
  // While this `EventMonitor` object exists, this object will only look for
  // `ui::ET_MOUSE_ENTERED` and `ui::ET_MOUSE_EXITED` events that occur in the
  // `widget_window` indicated in the constructor.
  std::unique_ptr<views::EventMonitor> event_monitor_;

  const std::string nudge_id_;

  // This is run whenever the mouse enters or exits the observed window with a
  // parameter to indicate whether the window is being hovered.
  HoverStateChangeCallback hover_state_change_callback_;
  const raw_ptr<AnchoredNudgeManagerImpl, ExperimentalAsh>
      anchored_nudge_manager_;
};

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

// A widget observer that is used to clean up the cached objects related to a
// nudge when its widget is destroying.
class AnchoredNudgeManagerImpl::NudgeWidgetObserver
    : public views::WidgetObserver {
 public:
  NudgeWidgetObserver(AnchoredNudge* anchored_nudge,
                      AnchoredNudgeManagerImpl* anchored_nudge_manager)
      : anchored_nudge_(anchored_nudge),
        anchored_nudge_manager_(anchored_nudge_manager) {
    anchored_nudge->GetWidget()->AddObserver(this);
  }

  NudgeWidgetObserver(const NudgeWidgetObserver&) = delete;

  NudgeWidgetObserver& operator=(const NudgeWidgetObserver&) = delete;

  ~NudgeWidgetObserver() override {
    if (anchored_nudge_ && anchored_nudge_->GetWidget()) {
      anchored_nudge_->GetWidget()->RemoveObserver(this);
    }
  }

  // WidgetObserver:
  void OnWidgetDestroying(views::Widget* widget) override {
    widget->RemoveObserver(this);
    anchored_nudge_manager_->HandleNudgeWidgetDestroying(anchored_nudge_->id());
  }

 private:
  // Owned by the views hierarchy.
  raw_ptr<AnchoredNudge> anchored_nudge_;

  // Owned by `Shell`.
  raw_ptr<AnchoredNudgeManagerImpl> anchored_nudge_manager_;
};

AnchoredNudgeManagerImpl::AnchoredNudgeManagerImpl() {
  DCHECK(features::IsSystemNudgeV2Enabled());
  Shell::Get()->session_controller()->AddObserver(this);
}

AnchoredNudgeManagerImpl::~AnchoredNudgeManagerImpl() {
  CloseAllNudges();

  Shell::Get()->session_controller()->RemoveObserver(this);
}

void AnchoredNudgeManagerImpl::Show(AnchoredNudgeData& nudge_data) {
  std::string id = nudge_data.id;
  CHECK(!id.empty());

  // If `id` is already in use, cancel the nudge so it can be replaced.
  if (IsNudgeShown(id)) {
    Cancel(id);
  }

  views::View* anchor_view = nudge_data.anchor_view;
  // Nudges cannot show without a visible anchor view or without a widget.
  if (!anchor_view->GetVisible() || !anchor_view->GetWidget()) {
    return;
  }

  // Chain callbacks with `Cancel()` so nudge is dismissed on button pressed.
  // TODO(b/285023559): Add `ChainedCancelCallback` class so we don't have to
  // manually modify the provided callbacks.
  nudge_data.dismiss_callback =
      ChainCancelCallback(nudge_data.dismiss_callback, id);
  nudge_data.second_button_callback =
      ChainCancelCallback(nudge_data.second_button_callback, id);

  auto anchored_nudge = std::make_unique<AnchoredNudge>(nudge_data);
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

  // The widget is not activated so the nudge does not steal focus.
  anchored_nudge_widget->ShowInactive();

  RecordNudgeShown(nudge_data.catalog_name);

  nudge_widget_observers_[id] =
      std::make_unique<NudgeWidgetObserver>(anchored_nudge_ptr, this);

  anchor_view_observers_[id] = std::make_unique<AnchorViewObserver>(
      anchored_nudge_ptr, anchor_view, this);

  nudge_hover_observers_[id] = std::make_unique<NudgeHoverObserver>(
      anchored_nudge_widget->GetNativeWindow(), id,
      std::move(nudge_data.hover_state_change_callback), this);

  // Only nudges that expire should be able to persist on hover (i.e. nudges
  // with infinite duration persist regardless of hover).
  if (!nudge_data.has_infinite_duration) {
    StartDismissTimer(id);
  }
}

void AnchoredNudgeManagerImpl::Cancel(const std::string& id) {
  if (!IsNudgeShown(id)) {
    return;
  }

  // Cache cleanup occurs on `HandleNudgeWidgetDestroying()`.
  shown_nudges_[id]->GetWidget()->CloseNow();
}

void AnchoredNudgeManagerImpl::MaybeRecordNudgeAction(
    NudgeCatalogName catalog_name) {
  auto& nudge_registry = GetNudgeRegistry();
  auto it = std::find_if(
      std::begin(nudge_registry), std::end(nudge_registry),
      [catalog_name](
          const std::pair<NudgeCatalogName, base::TimeTicks> registry_entry) {
        return catalog_name == registry_entry.first;
      });

  // Don't record "TimeToAction" metric if the nudge hasn't been shown before.
  if (it == std::end(nudge_registry)) {
    return;
  }

  const base::TimeDelta delta = base::TimeTicks::Now() - it->second;
  base::UmaHistogramEnumeration(GetNudgeTimeToActionHistogramName(delta),
                                catalog_name);

  nudge_registry.erase(it);
}

void AnchoredNudgeManagerImpl::CloseAllNudges() {
  while (!shown_nudges_.empty()) {
    Cancel(/*id=*/shown_nudges_.begin()->first);
  }
}

void AnchoredNudgeManagerImpl::HandleNudgeWidgetDestroying(
    const std::string& id) {
  dismiss_timers_.erase(id);
  nudge_hover_observers_.erase(id);
  anchor_view_observers_.erase(id);
  nudge_widget_observers_.erase(id);
  shown_nudges_.erase(id);
}

void AnchoredNudgeManagerImpl::OnNudgeHoverStateChanged(const std::string& id,
                                                        bool is_hovering) {
  // If |has_infinite_duration| is true then no dismiss timer will be created
  // for the nudge. Adding the check to prevent stopping a non-exist timer or
  // creating new timer when hover event happens.
  if (base::Contains(dismiss_timers_, id)) {
    if (is_hovering) {
      StopDismissTimer(id);
    } else {
      StartDismissTimer(id);
    }
  }
}

void AnchoredNudgeManagerImpl::OnSessionStateChanged(
    session_manager::SessionState state) {
  CloseAllNudges();
}

bool AnchoredNudgeManagerImpl::IsNudgeShown(const std::string& id) {
  return base::Contains(shown_nudges_, id);
}

const std::u16string& AnchoredNudgeManagerImpl::GetNudgeBodyTextForTest(
    const std::string& id) {
  CHECK(IsNudgeShown(id));
  return shown_nudges_[id]->GetBodyText();
}

views::View* AnchoredNudgeManagerImpl::GetNudgeAnchorViewForTest(
    const std::string& id) {
  CHECK(IsNudgeShown(id));
  return shown_nudges_[id]->GetAnchorView();
}

views::LabelButton* AnchoredNudgeManagerImpl::GetNudgeDismissButtonForTest(
    const std::string& id) {
  CHECK(IsNudgeShown(id));
  return shown_nudges_[id]->GetDismissButton();
}

views::LabelButton* AnchoredNudgeManagerImpl::GetNudgeSecondButtonForTest(
    const std::string& id) {
  CHECK(IsNudgeShown(id));
  return shown_nudges_[id]->GetSecondButton();
}

AnchoredNudge* AnchoredNudgeManagerImpl::GetShownNudgeForTest(
    const std::string& id) {
  CHECK(IsNudgeShown(id));
  return shown_nudges_[id];
}

void AnchoredNudgeManagerImpl::ResetNudgeRegistryForTesting() {
  GetNudgeRegistry().clear();
}

// static
std::vector<std::pair<NudgeCatalogName, base::TimeTicks>>&
AnchoredNudgeManagerImpl::GetNudgeRegistry() {
  static auto nudge_registry =
      std::vector<std::pair<NudgeCatalogName, base::TimeTicks>>();
  return nudge_registry;
}

void AnchoredNudgeManagerImpl::RecordNudgeShown(NudgeCatalogName catalog_name) {
  base::UmaHistogramEnumeration("Ash.NotifierFramework.Nudge.ShownCount",
                                catalog_name);

  // Record nudge shown time in the nudge registry.
  auto& nudge_registry = GetNudgeRegistry();
  auto it = std::find_if(
      std::begin(nudge_registry), std::end(nudge_registry),
      [catalog_name](
          const std::pair<NudgeCatalogName, base::TimeTicks> registry_entry) {
        return catalog_name == registry_entry.first;
      });

  if (it == std::end(nudge_registry)) {
    nudge_registry.emplace_back(catalog_name, base::TimeTicks::Now());
  } else {
    it->second = base::TimeTicks::Now();
  }
}

base::RepeatingClosure AnchoredNudgeManagerImpl::ChainCancelCallback(
    base::RepeatingClosure callback,
    const std::string& id) {
  return callback ? std::move(callback).Then(
                        base::BindRepeating(&AnchoredNudgeManagerImpl::Cancel,
                                            base::Unretained(this), id))
                  : base::BindRepeating(&AnchoredNudgeManagerImpl::Cancel,
                                        base::Unretained(this), id);
}

void AnchoredNudgeManagerImpl::StartDismissTimer(const std::string& id) {
  // TODO(b/282805060): Use a `PausableTimer` instead of restarting timer.
  dismiss_timers_[id].Start(FROM_HERE, kAnchoredNudgeDuration,
                            base::BindOnce(&AnchoredNudgeManagerImpl::Cancel,
                                           base::Unretained(this), id));
}

void AnchoredNudgeManagerImpl::StopDismissTimer(const std::string& id) {
  if (!base::Contains(dismiss_timers_, id)) {
    return;
  }
  dismiss_timers_[id].Stop();
}

}  // namespace ash
