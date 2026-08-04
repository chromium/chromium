// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/experimental_opt_in/glic_experimental_opt_in_controller.h"

#include <memory>
#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/task/sequenced_task_runner.h"
#include "build/build_config.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/public/glic_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"

namespace glic {

GlicExperimentalOptInController::GlicExperimentalOptInController(
    Profile* profile)
    : profile_(profile),
      host_(GlicExperimentalOptInUIHost::Create(profile_, this)) {
  CHECK(host_);
}

GlicExperimentalOptInController::~GlicExperimentalOptInController() = default;

void GlicExperimentalOptInController::ShowDialog(
    content::WebContents* web_contents,
    base::OnceCallback<void(bool)> callback) {
  GlicKeyedService* service =
      GlicKeyedServiceFactory::GetGlicKeyedService(profile_);
  if (!service) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), false));
    return;
  }

  if (service->enabling().GetRequiredExperimentalOptIn() ==
      RequiredExperimentalOptIn::kNotNeeded) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), true));
    return;
  }

  callbacks_.push_back(std::move(callback));
  host_->Show(web_contents);
}

void GlicExperimentalOptInController::CloseDialog(bool accepted) {
  host_->Close(accepted);
}

void GlicExperimentalOptInController::OnUIClosed(bool accepted) {
  for (auto& callback : std::exchange(callbacks_, {})) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), accepted));
  }
}

void GlicExperimentalOptInController::OpenLinkInNewTab(const GURL& url) {
  host_->OpenLinkInNewTab(url);
}

#if !BUILDFLAG(IS_ANDROID)
views::Widget* GlicExperimentalOptInController::GetDialogWidgetForTesting() {
  return host_->GetWidgetForTesting();  // IN-TEST
}

GlicExperimentalOptInDialogView*
GlicExperimentalOptInController::GetDialogViewForTesting() {
  return host_->GetDialogViewForTesting();  // IN-TEST
}
#endif

void GlicExperimentalOptInController::SetTickClockForTesting(
    const base::TickClock* clock) {
  host_->SetTickClockForTesting(clock);  // IN-TEST
}

content::WebContents*
GlicExperimentalOptInController::GetOrCreateSuitableWebContents() {
  return host_->GetOrCreateSuitableWebContents();
}

}  // namespace glic
