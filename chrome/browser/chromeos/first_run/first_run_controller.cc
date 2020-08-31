// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/first_run/first_run_controller.h"

#include "ash/public/cpp/first_run_helper.h"
#include "ash/public/cpp/shelf_prefs.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/chromeos/first_run/first_run_view.h"
#include "chrome/browser/chromeos/first_run/metrics.h"
#include "chrome/browser/chromeos/first_run/steps/app_list_step.h"
#include "chrome/browser/chromeos/first_run/steps/tray_step.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/ash_util.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "components/user_manager/user_manager.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/views/widget/widget.h"

namespace {

size_t NONE_STEP_INDEX = std::numeric_limits<size_t>::max();

// Instance of currently running controller, or NULL if controller is not
// running now.
chromeos::FirstRunController* g_first_run_controller_instance;

void RecordCompletion(chromeos::first_run::TutorialCompletion type) {
  UMA_HISTOGRAM_ENUMERATION("CrosFirstRun.TutorialCompletion",
                            type,
                            chromeos::first_run::TUTORIAL_COMPLETION_SIZE);
}

std::unique_ptr<views::Widget> CreateFirstRunWidget() {
  auto widget = std::make_unique<views::Widget>();
  views::Widget::InitParams params(
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.bounds = display::Screen::GetScreen()->GetPrimaryDisplay().bounds();
  params.show_state = ui::SHOW_STATE_FULLSCREEN;
  params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
  ash_util::SetupWidgetInitParamsForContainer(
      &params, ash::kShellWindowId_OverlayContainer);
  widget->Init(std::move(params));
  return widget;
}

}  // namespace

namespace chromeos {

FirstRunController::~FirstRunController() {}

// static
void FirstRunController::Start() {
  if (g_first_run_controller_instance) {
    LOG(WARNING) << "First-run tutorial is running already.";
    return;
  }
  g_first_run_controller_instance = new FirstRunController();
  g_first_run_controller_instance->Init();
}

// static
void FirstRunController::Stop() {
  if (!g_first_run_controller_instance) {
    LOG(WARNING) << "First-run tutorial is not running.";
    return;
  }
  g_first_run_controller_instance->Finalize();
  base::ThreadTaskRunnerHandle::Get()->DeleteSoon(
      FROM_HERE, g_first_run_controller_instance);
  g_first_run_controller_instance = NULL;
}

gfx::Size FirstRunController::GetOverlaySize() const {
  return widget_->GetWindowBoundsInScreen().size();
}

ash::ShelfAlignment FirstRunController::GetShelfAlignment() const {
  DCHECK(user_profile_);
  return ash::GetShelfAlignmentPref(
      user_profile_->GetPrefs(),
      display::Screen::GetScreen()->GetPrimaryDisplay().id());
}

void FirstRunController::Cancel() {
  OnCancelled();
}

FirstRunController* FirstRunController::GetInstanceForTest() {
  return g_first_run_controller_instance;
}

FirstRunController::FirstRunController()
    : actor_(NULL),
      current_step_index_(NONE_STEP_INDEX),
      user_profile_(NULL) {
}

void FirstRunController::Init() {
  start_time_ = base::Time::Now();
  user_manager::UserManager* user_manager = user_manager::UserManager::Get();
  user_profile_ = ProfileHelper::Get()->GetProfileByUserUnsafe(
      user_manager->GetActiveUser());

  first_run_helper_ = ash::FirstRunHelper::Start(
      base::BindOnce(&FirstRunController::OnCancelled, base::Unretained(this)));

  widget_ = CreateFirstRunWidget();
  FirstRunView* view =
      widget_->SetContentsView(std::make_unique<FirstRunView>());
  view->Init(user_profile_, this);
  actor_ = view->GetActor();
  actor_->set_delegate(this);
  widget_->Show();
  view->RequestFocus();
  web_contents_for_tests_ = view->GetWebContents();

  if (actor_->IsInitialized())
    OnActorInitialized();
}

void FirstRunController::Finalize() {
  int furthest_step = current_step_index_ == NONE_STEP_INDEX
                          ? steps_.size() - 1
                          : current_step_index_;
  UMA_HISTOGRAM_EXACT_LINEAR("CrosFirstRun.FurthestStep", furthest_step,
                             steps_.size());
  UMA_HISTOGRAM_MEDIUM_TIMES("CrosFirstRun.TimeSpent",
                             base::Time::Now() - start_time_);
  if (GetCurrentStep())
    GetCurrentStep()->OnBeforeHide();
  steps_.clear();
  if (actor_)
    actor_->set_delegate(NULL);
  actor_ = NULL;
  first_run_helper_.reset();
  // Close the widget.
  widget_.reset();
}

void FirstRunController::OnActorInitialized() {
  RegisterSteps();
  ShowNextStep();
}

void FirstRunController::OnNextButtonClicked(const std::string& step_name) {
  DCHECK(GetCurrentStep() && GetCurrentStep()->name() == step_name);
  GetCurrentStep()->OnBeforeHide();
  actor_->HideCurrentStep();
}

void FirstRunController::OnHelpButtonClicked() {
  RecordCompletion(first_run::TUTORIAL_COMPLETED_WITH_KEEP_EXPLORING);
  on_actor_finalized_ = base::Bind(chrome::ShowHelpForProfile, user_profile_,
                                   chrome::HELP_SOURCE_MENU);
  actor_->Finalize();
}

void FirstRunController::OnStepHidden(const std::string& step_name) {
  DCHECK(GetCurrentStep() && GetCurrentStep()->name() == step_name);
  GetCurrentStep()->OnAfterHide();
  if (!actor_->IsFinalizing())
    ShowNextStep();
}

void FirstRunController::OnStepShown(const std::string& step_name) {
  DCHECK(GetCurrentStep() && GetCurrentStep()->name() == step_name);
}

void FirstRunController::OnActorFinalized() {
  if (!on_actor_finalized_.is_null())
    on_actor_finalized_.Run();
  Stop();
}

void FirstRunController::OnActorDestroyed() {
  // Normally this shouldn't happen because we are implicitly controlling
  // actor's lifetime.
  NOTREACHED() <<
    "FirstRunActor destroyed before FirstRunController::Finalize.";
}

void FirstRunController::OnCancelled() {
  RecordCompletion(first_run::TUTORIAL_NOT_FINISHED);
  Stop();
}

void FirstRunController::RegisterSteps() {
  steps_.push_back(std::make_unique<first_run::AppListStep>(this, actor_));
  steps_.push_back(std::make_unique<first_run::TrayStep>(this, actor_));
}

void FirstRunController::ShowNextStep() {
  AdvanceStep();
  if (!GetCurrentStep()) {
    actor_->Finalize();
    RecordCompletion(first_run::TUTORIAL_COMPLETED_WITH_GOT_IT);
    return;
  }
  if (!GetCurrentStep()->Show())
    ShowNextStep();
}

void FirstRunController::AdvanceStep() {
  if (current_step_index_ == NONE_STEP_INDEX)
    current_step_index_ = 0;
  else
    ++current_step_index_;
  if (current_step_index_ >= steps_.size())
    current_step_index_ = NONE_STEP_INDEX;
}

first_run::Step* FirstRunController::GetCurrentStep() const {
  return current_step_index_ != NONE_STEP_INDEX ?
      steps_[current_step_index_].get() : NULL;
}

}  // namespace chromeos
