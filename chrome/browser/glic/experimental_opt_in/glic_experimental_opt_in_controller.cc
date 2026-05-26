// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/experimental_opt_in/glic_experimental_opt_in_controller.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/default_tick_clock.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "chrome/browser/glic/experimental_opt_in/glic_experimental_opt_in_dialog_view.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/public/glic_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/public/tab_dialog_manager.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/window_open_disposition.h"
#include "ui/views/widget/widget.h"
#include "url/gurl.h"

namespace glic {

GlicExperimentalOptInController::GlicExperimentalOptInController(
    Profile* profile)
    : profile_(profile), tick_clock_(base::DefaultTickClock::GetInstance()) {}

GlicExperimentalOptInController::~GlicExperimentalOptInController() = default;

views::Widget* GlicExperimentalOptInController::ShowDialog(
    content::WebContents* web_contents,
    base::OnceCallback<void(bool)> callback) {
  GlicKeyedService* service =
      GlicKeyedServiceFactory::GetGlicKeyedService(profile_);
  if (!service) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), false));
    return nullptr;
  }

  if (service->enabling().GetRequiredExperimentalOptIn() ==
      RequiredExperimentalOptIn::kNotNeeded) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), true));
    return nullptr;
  }

  callbacks_.push_back(std::move(callback));

  if (dialog_widget_) {
    dialog_widget_->Show();
    return dialog_widget_.get();
  }

  tabs::TabInterface* tab_interface =
      tabs::TabInterface::MaybeGetFromContents(web_contents);
  if (!tab_interface) {
    CloseDialog(false);
    return nullptr;
  }

  tab_interface_ = tab_interface->GetWeakPtr();

  dialog_view_ = std::make_unique<GlicExperimentalOptInDialogView>(
      profile_, tab_interface);
  dialog_view_->SetOwnershipOfNewWidget(
      views::Widget::InitParams::CLIENT_OWNS_WIDGET);

  dialog_widget_ = tab_interface->GetTabFeatures()
                       ->tab_dialog_manager()
                       ->CreateAndShowDialog(
                           dialog_view_.get(),
                           std::make_unique<tabs::TabDialogManager::Params>());

  dialog_open_time_ = tick_clock_->NowTicks();
  if (tab_interface->IsVisible()) {
    visibility_start_time_ = dialog_open_time_;
  }

  tab_subscriptions_.push_back(tab_interface->RegisterDidBecomeVisible(
      base::BindRepeating(&GlicExperimentalOptInController::TabDidBecomeVisible,
                          weak_ptr_factory_.GetWeakPtr())));
  tab_subscriptions_.push_back(tab_interface->RegisterWillBecomeHidden(
      base::BindRepeating(&GlicExperimentalOptInController::TabWillBecomeHidden,
                          weak_ptr_factory_.GetWeakPtr())));

  dialog_widget_->MakeCloseSynchronous(
      base::BindOnce(&GlicExperimentalOptInController::CloseWidget,
                     weak_ptr_factory_.GetWeakPtr()));

  return dialog_widget_.get();
}

void GlicExperimentalOptInController::CloseDialog(bool accepted) {
  CloseWidget(accepted ? views::Widget::ClosedReason::kAcceptButtonClicked
                       : views::Widget::ClosedReason::kCancelButtonClicked);
}

void GlicExperimentalOptInController::CloseWidget(
    views::Widget::ClosedReason reason) {
  tab_subscriptions_.clear();

  if (dialog_widget_) {
    if (!dialog_open_time_.is_null()) {
      base::UmaHistogramMediumTimes(
          "Glic.ExperimentalTriggering.OptInDialog.ShowDuration",
          tick_clock_->NowTicks() - dialog_open_time_);
      dialog_open_time_ = base::TimeTicks();
    }

    if (!visibility_start_time_.is_null()) {
      visible_duration_ += tick_clock_->NowTicks() - visibility_start_time_;
      visibility_start_time_ = base::TimeTicks();
    }

    base::UmaHistogramMediumTimes(
        "Glic.ExperimentalTriggering.OptInDialog.VisibleDuration",
        visible_duration_);
    visible_duration_ = base::TimeDelta();
  }

  bool accepted = (reason == views::Widget::ClosedReason::kAcceptButtonClicked);
  for (auto& callback : std::exchange(callbacks_, {})) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), accepted));
  }

  dialog_widget_.reset();
  dialog_view_.reset();
  tab_interface_.reset();
}

void GlicExperimentalOptInController::OpenLinkInNewTab(const GURL& url) {
  if (!tab_interface_) {
    return;
  }
  if (auto* browser_window = tab_interface_->GetBrowserWindowInterface()) {
    browser_window->OpenGURL(url, WindowOpenDisposition::NEW_FOREGROUND_TAB);
  }
}

void GlicExperimentalOptInController::TabDidBecomeVisible(
    tabs::TabInterface* tab_interface) {
  if (visibility_start_time_.is_null()) {
    visibility_start_time_ = tick_clock_->NowTicks();
  }
}

void GlicExperimentalOptInController::TabWillBecomeHidden(
    tabs::TabInterface* tab_interface) {
  if (!visibility_start_time_.is_null()) {
    visible_duration_ += tick_clock_->NowTicks() - visibility_start_time_;
    visibility_start_time_ = base::TimeTicks();
  }
}

}  // namespace glic
