// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/toast/anchored_nudge_manager_impl.h"

#include <algorithm>
#include <memory>
#include <string>

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/system/anchored_nudge_data.h"
#include "ash/public/cpp/system/scoped_anchored_nudge_pause.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/system/toast/anchored_nudge.h"
#include "base/containers/contains.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "chromeos/ui/base/nudge_util.h"
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

namespace ash {

// Owns a `base::OneShotTimer` that can be paused and resumed.
class AnchoredNudgeManagerImpl::PausableTimer {
 public:
  PausableTimer() = default;
  PausableTimer(const PausableTimer&) = delete;
  PausableTimer& operator=(const PausableTimer&) = delete;
  ~PausableTimer() = default;

  void Start(base::TimeDelta duration, base::RepeatingClosure task) {
    DCHECK(!timer_.IsRunning());
    task_ = task;
    remaining_duration_ = duration;
    time_last_started_ = base::TimeTicks::Now();
    timer_.Start(FROM_HERE, remaining_duration_, task_);
  }

  void Pause() {
    DCHECK(timer_.IsRunning());
    timer_.Stop();
    remaining_duration_ -= base::TimeTicks::Now() - time_last_started_;
  }

  void Resume() {
    time_last_started_ = base::TimeTicks::Now();
    timer_.Start(FROM_HERE, remaining_duration_, task_);
  }

  void Stop() {
    remaining_duration_ = base::Seconds(0);
    task_.Reset();
    timer_.Stop();
  }

 private:
  base::OneShotTimer timer_;
  base::RepeatingClosure task_;
  base::TimeDelta remaining_duration_;
  base::TimeTicks time_last_started_;
};

// A hover observer used to pause or resume the dismiss timer, and to run
// provided callbacks that execute on hover state changes.
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
        anchored_nudge_manager_->OnNudgeHoverStateChanged(nudge_id_,
                                                          /*is_hovering=*/true);
        if (!hover_state_change_callback_.is_null()) {
          std::move(hover_state_change_callback_).Run(true);
        }
        break;
      case ui::ET_MOUSE_EXITED:
        anchored_nudge_manager_->OnNudgeHoverStateChanged(
            nudge_id_, /*is_hovering=*/false);
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
    DCHECK(anchored_nudge->GetWidget());
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

  // If `pause_counter_` is greater than 0, no nudges should be shown.
  if (pause_counter_ > 0) {
    return;
  }

  // If `id` is already in use, cancel the nudge so it can be replaced.
  if (IsNudgeShown(id)) {
    Cancel(id);
  }

  views::View* anchor_view = nudge_data.anchor_view;

  // Nudges with an anchor view won't show if `anchor_view` is not visible or
  // does not have a widget.
  if (anchor_view) {
    if (!anchor_view->GetVisible() || !anchor_view->GetWidget()) {
      return;
    }
  }

  // Chain callbacks with `Cancel()` so nudge is dismissed on button pressed.
  // TODO(b/285023559): Add `ChainedCancelCallback` class so we don't have to
  // manually modify the provided callbacks.
  if (!nudge_data.first_button_text.empty()) {
    nudge_data.first_button_callback =
        ChainCancelCallback(nudge_data.first_button_callback,
                            nudge_data.catalog_name, id, /*first_button=*/true);
  }

  if (!nudge_data.second_button_text.empty()) {
    nudge_data.second_button_callback = ChainCancelCallback(
        nudge_data.second_button_callback, nudge_data.catalog_name, id,
        /*first_button=*/false);
  }

  auto anchored_nudge = std::make_unique<AnchoredNudge>(nudge_data);
  auto* anchored_nudge_ptr = anchored_nudge.get();
  shown_nudges_[id] = anchored_nudge_ptr;

  auto* anchored_nudge_widget =
      views::BubbleDialogDelegate::CreateBubble(std::move(anchored_nudge));

  // The widget is not activated so the nudge does not steal focus.
  anchored_nudge_widget->ShowInactive();

  RecordNudgeShown(nudge_data.catalog_name);

  nudge_widget_observers_[id] =
      std::make_unique<NudgeWidgetObserver>(anchored_nudge_ptr, this);

  if (anchor_view) {
    anchor_view_observers_[id] = std::make_unique<AnchorViewObserver>(
        anchored_nudge_ptr, anchor_view, this);
  }

  nudge_hover_observers_[id] = std::make_unique<NudgeHoverObserver>(
      anchored_nudge_widget->GetNativeWindow(), id,
      std::move(nudge_data.hover_state_change_callback), this);

  dismiss_timers_[id].Start(
      nudge_data.has_long_duration ? kNudgeLongDuration : kNudgeDefaultDuration,
      base::BindRepeating(&AnchoredNudgeManagerImpl::Cancel,
                          base::Unretained(this), id));
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

  base::UmaHistogramEnumeration(chromeos::GetNudgeTimeToActionHistogramName(
                                    base::TimeTicks::Now() - it->second),
                                catalog_name);

  nudge_registry.erase(it);
}

std::unique_ptr<ScopedAnchoredNudgePause>
AnchoredNudgeManagerImpl::CreateScopedPause() {
  return std::make_unique<ScopedAnchoredNudgePause>();
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
  if (anchor_view_observers_[id]) {
    anchor_view_observers_.erase(id);
  }
  nudge_widget_observers_.erase(id);
  shown_nudges_.erase(id);
}

void AnchoredNudgeManagerImpl::OnNudgeHoverStateChanged(const std::string& id,
                                                        bool is_hovering) {
  if (is_hovering) {
    dismiss_timers_[id].Pause();
  } else {
    dismiss_timers_[id].Resume();
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

views::LabelButton* AnchoredNudgeManagerImpl::GetNudgeFirstButtonForTest(
    const std::string& id) {
  CHECK(IsNudgeShown(id));
  return shown_nudges_[id]->GetFirstButton();
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
  base::UmaHistogramEnumeration(
      chromeos::kNotifierFrameworkNudgeShownCountHistogram, catalog_name);

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
    NudgeCatalogName catalog_name,
    const std::string& id,
    bool first_button) {
  return std::move(callback)
      .Then(base::BindRepeating(&AnchoredNudgeManagerImpl::Cancel,
                                base::Unretained(this), id))
      .Then(base::BindRepeating(&AnchoredNudgeManagerImpl::RecordButtonPressed,
                                base::Unretained(this), catalog_name,
                                first_button));
}

void AnchoredNudgeManagerImpl::RecordButtonPressed(
    NudgeCatalogName catalog_name,
    bool first_button) {
  base::UmaHistogramEnumeration(
      first_button ? "Ash.NotifierFramework.Nudge.FirstButtonPressed"
                   : "Ash.NotifierFramework.Nudge.SecondButtonPressed",
      catalog_name);
}

void AnchoredNudgeManagerImpl::Pause() {
  ++pause_counter_;

  // Immediately closes all the nudges.
  CloseAllNudges();
}

void AnchoredNudgeManagerImpl::Resume() {
  CHECK_GT(pause_counter_, 0);
  --pause_counter_;
}

}  // namespace ash
