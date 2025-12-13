// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_insert/views/quick_insert_feature_tour.h"

#include <memory>
#include <string>
#include <utility>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/quick_insert/views/quick_insert_feature_tour_dialog_view.h"
#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/task/sequenced_task_runner.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "ui/aura/window.h"
#include "ui/base/ui_base_types.h"
#include "ui/compositor/layer.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/public/activation_client.h"

namespace ash {
namespace {

std::unique_ptr<views::Widget> CreateWidget(
    QuickInsertFeatureTour::EditorStatus editor_status,
    base::RepeatingClosure learn_more_callback,
    base::OnceClosure completion_callback) {
  auto feature_tour_dialog =
      views::Builder<QuickInsertFeatureTourDialogView>(
          std::make_unique<QuickInsertFeatureTourDialogView>(
              editor_status, std::move(learn_more_callback),
              std::move(completion_callback)))
          .Build();

  views::Widget::InitParams params(
      views::Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET,
      views::Widget::InitParams::TYPE_POPUP);
  params.delegate = feature_tour_dialog.release();
  params.name = "QuickInsertFeatureTourWidget";
  params.activatable = views::Widget::InitParams::Activatable::kYes;
  params.z_order = ui::ZOrderLevel::kFloatingUIElement;

  auto widget = std::make_unique<views::Widget>(std::move(params));
  widget->GetLayer()->SetFillsBoundsOpaquely(false);
  return widget;
}

}  // namespace

QuickInsertFeatureTour::QuickInsertFeatureTour() = default;

QuickInsertFeatureTour::~QuickInsertFeatureTour() {
  if (widget_) {
    widget_->CloseNow();
  }
}

void QuickInsertFeatureTour::RegisterProfilePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(prefs::kQuickInsertFeatureTourCompletedPref,
                                false);
}

bool QuickInsertFeatureTour::MaybeShowForFirstUse(
    PrefService* prefs,
    EditorStatus editor_status,
    base::RepeatingClosure learn_more_callback,
    base::RepeatingClosure completion_callback) {
  auto* pref =
      prefs->FindPreference(prefs::kQuickInsertFeatureTourCompletedPref);
  // Don't show if `pref` is null (this happens in unit tests that don't call
  // `RegisterProfilePrefs`).
  if (pref == nullptr || pref->GetValue()->GetBool()) {
    return false;
  }

  widget_ = CreateWidget(
      editor_status,
      base::BindRepeating(
          &QuickInsertFeatureTour::SetOnWindowDeactivatedCallback,
          weak_ptr_factory_.GetWeakPtr(), std::move(learn_more_callback)),
      base::BindOnce(&QuickInsertFeatureTour::SetOnWindowDeactivatedCallback,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(completion_callback)));

  aura::Window* window = widget_->GetNativeWindow();
  CHECK_NE(window, nullptr);
  wm::ActivationClient* activation_client =
      wm::GetActivationClient(window->GetRootWindow());
  CHECK_NE(activation_client, nullptr);
  obs_.Reset();
  obs_.Observe(activation_client);

  widget_->Show();

  prefs->SetBoolean(prefs::kQuickInsertFeatureTourCompletedPref, true);
  return true;
}

const views::Link* QuickInsertFeatureTour::learn_more_link_for_testing() const {
  if (!widget_) {
    return nullptr;
  }

  auto* feature_tour_dialog = static_cast<QuickInsertFeatureTourDialogView*>(
      widget_->GetContentsView());
  return feature_tour_dialog != nullptr
             ? feature_tour_dialog->learn_more_link_for_testing()  // IN-TEST
             : nullptr;
}

const views::Button* QuickInsertFeatureTour::complete_button_for_testing()
    const {
  if (!widget_) {
    return nullptr;
  }

  auto* feature_tour_dialog = static_cast<QuickInsertFeatureTourDialogView*>(
      widget_->GetContentsView());
  return feature_tour_dialog != nullptr
             ? feature_tour_dialog->complete_button_for_testing()  // IN-TEST
             : nullptr;
}

const views::Button* QuickInsertFeatureTour::close_button_for_testing() const {
  if (!widget_) {
    return nullptr;
  }
  auto* feature_tour_dialog = static_cast<QuickInsertFeatureTourDialogView*>(
      widget_->GetContentsView());
  return feature_tour_dialog != nullptr
             ? feature_tour_dialog->close_button_for_testing()  // IN-TEST
             : nullptr;
}

views::Widget* QuickInsertFeatureTour::widget_for_testing() {
  return widget_.get();
}

void QuickInsertFeatureTour::OnWindowActivated(ActivationReason reason,
                                               aura::Window* gained_active,
                                               aura::Window* lost_active) {
  RunOnWindowDeactivatedIfNeeded();
}

void QuickInsertFeatureTour::SetOnWindowDeactivatedCallback(
    base::OnceClosure callback) {
  on_window_deactivated_callback_ = std::move(callback);

  RunOnWindowDeactivatedIfNeeded();
}

void QuickInsertFeatureTour::RunOnWindowDeactivatedIfNeeded() {
  if (on_window_deactivated_callback_.is_null()) {
    return;
  }
  if (widget_ && obs_.IsObserving() &&
      widget_->GetNativeWindow() == obs_.GetSource()->GetActiveWindow()) {
    return;
  }

  // As of writing, this method is called from two code paths:
  //
  // 1. `OnWindowActivated`, which is called from
  // `wm::FocusController::SetActiveWindow`.
  // When `OnWindowActivated` is called, the active window should be set... but
  // we cannot activate any other windows (such as Quick Insert) synchronously
  // due to being in the middle of `wm::FocusController::SetActiveWindow`'s
  // "active window stack".
  // Doing so will cause a `DCHECK` crash in
  // `wm::FocusController::FocusAndActivateWindow` due to the active window
  // changing reentrantly. Turning off `DCHECK`s will result in no window
  // being shown / activated.
  //
  // We should only run callbacks after the `SetActiveWindow` "stack" is fully
  // resolved to avoid this. The only feasible way of doing this is to post a
  // task.
  //
  // 2. `SetOnWindowDeactivatedCallback`, which is passed in as callbacks to
  // `SystemDialogDelegateView`. Those callbacks are called from
  // `SystemDialogDelegateView::RunCallbackAndCloseDialog` before the widget is
  // closed.
  // Therefore, the active window should still be `widget_`'s native window, so
  // we should not have gotten to this point.
  //
  // However, `SystemDialogDelegateView` behaviour might change in the future.
  // The worst case would be `SystemDialogDelegateView` changing its behaviour
  // to call callbacks during `OnWindowDeactivated`, which would be equivalent
  // to the above code path. Therefore, we should also post a task in this
  // scenario.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, std::move(on_window_deactivated_callback_));
}

}  // namespace ash
