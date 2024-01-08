// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/lock_screen_apps/first_app_run_toast_manager.h"

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/values.h"
#include "chrome/browser/ash/lock_screen_apps/toast_dialog_view.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/common/extension.h"
#include "ui/aura/window.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace lock_screen_apps {

namespace {

// Toast dialog's vertical offset from the app window bottom.
constexpr int kToastDialogVerticalOffset = 20;

}  // namespace

// Observes the note taking app widget so bounds changes can update the toast
// position.
class FirstAppRunToastManager::AppWidgetObserver
    : public views::WidgetObserver {
 public:
  AppWidgetObserver(FirstAppRunToastManager* manager, views::Widget* widget)
      : manager_(manager), widget_(widget) {
    widget_->AddObserver(this);
  }

  AppWidgetObserver(const AppWidgetObserver&) = delete;
  AppWidgetObserver& operator=(const AppWidgetObserver&) = delete;

  ~AppWidgetObserver() override {
    // This is a no-op of the observer was previously removed.
    widget_->RemoveObserver(this);
    CHECK(!IsInObserverList());
  }

  // views::WidgetObserver:
  void OnWidgetBoundsChanged(views::Widget* widget,
                             const gfx::Rect& new_bounds) override {
    manager_->AdjustToastWidgetBounds();
  }

  void OnWidgetDestroying(views::Widget* widget) override {
    widget_->RemoveObserver(this);
  }

 private:
  raw_ptr<FirstAppRunToastManager> manager_;
  raw_ptr<views::Widget> widget_;
};

FirstAppRunToastManager::FirstAppRunToastManager(Profile* profile)
    : profile_(profile) {}

FirstAppRunToastManager::~FirstAppRunToastManager() {
  Reset();
}

void FirstAppRunToastManager::RunForAppWindow(
    extensions::AppWindow* app_window) {
  if (app_window_)
    return;

  DCHECK(app_window->GetNativeWindow());

  const extensions::Extension* app = app_window->GetExtension();
  const base::Value::Dict& toast_shown =
      profile_->GetPrefs()->GetDict(prefs::kNoteTakingAppsLockScreenToastShown);
  if (toast_shown.FindBoolByDottedPath(app->id()).value_or(false)) {
    return;
  }

  app_window_ = app_window;
  views::Widget* app_widget =
      views::Widget::GetWidgetForNativeWindow(app_window_->GetNativeWindow());
  DCHECK(app_widget);
  app_widget_observer_ = std::make_unique<AppWidgetObserver>(this, app_widget);

  if (app_window_->GetNativeWindow()->HasFocus()) {
    CreateAndShowToastDialog();
  } else {
    app_window_observation_.Observe(
        extensions::AppWindowRegistry::Get(app_window_->browser_context()));
  }
}

void FirstAppRunToastManager::Reset() {
  app_widget_observer_.reset();
  app_window_observation_.Reset();
  toast_widget_observation_.Reset();

  app_window_ = nullptr;

  weak_ptr_factory_.InvalidateWeakPtrs();

  if (toast_widget_ && !toast_widget_->IsClosed())
    toast_widget_->Close();
  toast_widget_ = nullptr;
}

void FirstAppRunToastManager::OnWidgetDestroyed(views::Widget* widget) {
  Reset();
}

void FirstAppRunToastManager::OnAppWindowActivated(
    extensions::AppWindow* app_window) {
  if (app_window == app_window_) {
    app_window_observation_.Reset();

    // Start toast dialog creation asynchronously so it happens after app window
    // activation completes.
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&FirstAppRunToastManager::CreateAndShowToastDialog,
                       weak_ptr_factory_.GetWeakPtr()));
  }
}

void FirstAppRunToastManager::CreateAndShowToastDialog() {
  auto* toast_dialog = new ToastDialogView(
      base::UTF8ToUTF16(app_window_->GetExtension()->short_name()),
      base::BindOnce(&FirstAppRunToastManager::ToastDialogDismissed,
                     weak_ptr_factory_.GetWeakPtr()));
  toast_widget_ = views::BubbleDialogDelegateView::CreateBubble(toast_dialog);
  toast_widget_->Show();
  AdjustToastWidgetBounds();
  toast_widget_observation_.Observe(toast_widget_.get());
}

void FirstAppRunToastManager::ToastDialogDismissed() {
  {
    const extensions::Extension* app = app_window_->GetExtension();
    ScopedDictPrefUpdate dict_update(
        profile_->GetPrefs(), prefs::kNoteTakingAppsLockScreenToastShown);
    dict_update->Set(app->id(), true);
  }
  Reset();
}

void FirstAppRunToastManager::AdjustToastWidgetBounds() {
  if (!toast_widget_)
    return;

  DCHECK(app_window_);

  const gfx::Rect app_window_bounds =
      app_window_->GetNativeWindow()->GetBoundsInScreen();
  const gfx::Rect original_bounds = toast_widget_->GetWindowBoundsInScreen();

  gfx::Point intended_origin = gfx::Point(
      // Center toast widget horizontally relative to app_window bounds.
      app_window_bounds.x() +
          (app_window_bounds.width() - original_bounds.width()) / 2,
      // Position toast widget dialog at the bottom of app window, with an
      // additional offset (so poirtion of the dialog is painted outside the
      // app window bounds).
      app_window_bounds.bottom() - original_bounds.height() +
          kToastDialogVerticalOffset);

  toast_widget_->SetBounds(gfx::Rect(intended_origin, original_bounds.size()));
}

}  // namespace lock_screen_apps
