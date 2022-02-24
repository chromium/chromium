// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/fuchsia/chrome_browser_main_parts_fuchsia.h"

#include <fuchsia/element/cpp/fidl.h>
#include <fuchsia/ui/app/cpp/fidl.h>
#include <fuchsia/ui/composition/cpp/fidl.h>
#include <fuchsia/ui/scenic/cpp/fidl.h>
#include <lib/sys/cpp/component_context.h>

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/check.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/fuchsia/fuchsia_logging.h"
#include "base/fuchsia/process_context.h"
#include "base/fuchsia/process_lifecycle.h"
#include "base/fuchsia/scoped_service_binding.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/fuchsia/element_manager_impl.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/common/chrome_switches.h"
#include "components/keep_alive_registry/keep_alive_types.h"
#include "components/keep_alive_registry/scoped_keep_alive.h"
#include "ui/gfx/switches.h"
#include "ui/ozone/public/ozone_switches.h"
#include "ui/platform_window/fuchsia/initialize_presenter_api_view.h"

namespace {

// Checks the supported ozone platform with Scenic if no arg is specified.
// TODO(fxbug.dev/94001): Delete this after Flatland migration is completed.
void HandleOzonePlatformArgs() {
  base::CommandLine* const launch_args = base::CommandLine::ForCurrentProcess();
  if (launch_args->HasSwitch(switches::kOzonePlatform))
    return;
  fuchsia::ui::scenic::ScenicSyncPtr scenic;
  zx_status_t status =
      base::ComponentContextForProcess()->svc()->Connect(scenic.NewRequest());
  if (status != ZX_OK) {
    ZX_LOG(ERROR, status) << "Couldn't connect to Scenic.";
    return;
  }
  bool scenic_uses_flatland = false;
  scenic->UsesFlatland(&scenic_uses_flatland);
  launch_args->AppendSwitchNative(switches::kOzonePlatform,
                                  scenic_uses_flatland ? "flatland" : "scenic");
}

void EnsureChromeStartsInBackground() {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kNoStartupWindow);
}

bool NotifyNewBrowserWindow(const base::CommandLine& command_line) {
  base::FilePath path;
  return ChromeBrowserMainParts::ProcessSingletonNotificationCallback(
      command_line, path);
}

}  // namespace

// Helper class that configures Ozone to use GraphicalPresenter to display a new
// View for each new top-level window.
class ChromeBrowserMainPartsFuchsia::ViewPresenter final {
 public:
  explicit ViewPresenter(ElementManagerImpl* element_manager)
      : element_manager_(element_manager) {
    ui::fuchsia::SetScenicViewPresenter(base::BindRepeating(
        &ViewPresenter::PresentScenicView, base::Unretained(this)));
    ui::fuchsia::SetFlatlandViewPresenter(base::BindRepeating(
        &ViewPresenter::PresentFlatlandView, base::Unretained(this)));

    base::ComponentContextForProcess()->svc()->Connect(
        graphical_presenter_.NewRequest());
    graphical_presenter_.set_error_handler(base::LogFidlErrorAndExitProcess(
        FROM_HERE, "fuchsia.element.GraphicalPresenter"));
  }
  ~ViewPresenter() = default;

  ViewPresenter(const ViewPresenter&) = delete;
  ViewPresenter& operator=(const ViewPresenter&) = delete;

 private:
  fuchsia::element::ViewControllerPtr PresentScenicView(
      fuchsia::ui::views::ViewHolderToken view_holder_token,
      fuchsia::ui::views::ViewRef view_ref) {
    fuchsia::element::ViewControllerPtr view_controller;
    fuchsia::element::ViewSpec view_spec;
    view_spec.set_view_holder_token(std::move(view_holder_token));
    view_spec.set_view_ref(std::move(view_ref));
    view_spec.set_annotations(fidl::Clone(element_manager_->GetAnnotations()));
    graphical_presenter_->PresentView(std::move(view_spec), nullptr,
                                      view_controller.NewRequest(),
                                      [](auto result) {});
    return view_controller;
  }

  fuchsia::element::ViewControllerPtr PresentFlatlandView(
      fuchsia::ui::views::ViewportCreationToken viewport_creation_token) {
    fuchsia::element::ViewControllerPtr view_controller;
    auto view_ref_pair = scenic::ViewRefPair::New();
    fuchsia::element::ViewSpec view_spec;
    view_spec.set_viewport_creation_token(std::move(viewport_creation_token));
    view_spec.set_view_ref(std::move(view_ref_pair.view_ref));
    view_spec.set_annotations(fidl::Clone(element_manager_->GetAnnotations()));
    graphical_presenter_->PresentView(std::move(view_spec), nullptr,
                                      view_controller.NewRequest(),
                                      [](auto result) {});
    return view_controller;
  }

  base::raw_ptr<ElementManagerImpl> element_manager_;
  fuchsia::element::GraphicalPresenterPtr graphical_presenter_;
};

// ChromeBrowserMainPartsFuchsia -----------------------------------------------

ChromeBrowserMainPartsFuchsia::ChromeBrowserMainPartsFuchsia(
    content::MainFunctionParams parameters,
    StartupData* startup_data)
    : ChromeBrowserMainParts(std::move(parameters), startup_data) {}

ChromeBrowserMainPartsFuchsia::~ChromeBrowserMainPartsFuchsia() = default;

void ChromeBrowserMainPartsFuchsia::ShowMissingLocaleMessageBox() {
  // Locale data should be bundled for all possible platform locales,
  // so crash here to make missing-locale states more visible.
  CHECK(false);
}

int ChromeBrowserMainPartsFuchsia::PreEarlyInitialization() {
  HandleOzonePlatformArgs();
  EnsureChromeStartsInBackground();
  return ChromeBrowserMainParts::PreEarlyInitialization();
}

int ChromeBrowserMainPartsFuchsia::PreMainMessageLoopRun() {
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(switches::kHeadless)) {
    // Configure Ozone to create top-level Views via GraphicalPresenter.
    element_manager_ = std::make_unique<ElementManagerImpl>(
        base::ComponentContextForProcess()->outgoing().get(),
        base::BindRepeating(&NotifyNewBrowserWindow));
    keep_alive_ = std::make_unique<ScopedKeepAlive>(
        KeepAliveOrigin::BROWSER_PROCESS_FUCHSIA,
        KeepAliveRestartOption::ENABLED);
    view_presenter_ = std::make_unique<ViewPresenter>(element_manager_.get());
  }

  zx_status_t status =
      base::ComponentContextForProcess()->outgoing()->ServeFromStartupInfo();
  ZX_CHECK(status == ZX_OK, status);

  // Publish the fuchsia.process.lifecycle.Lifecycle service to allow graceful
  // teardown. If there is a |ui_task| then this is a browser-test and graceful
  // shutdown is not required.
  if (!parameters().ui_task) {
    lifecycle_ = std::make_unique<base::ProcessLifecycle>(
        base::BindOnce(&chrome::ExitIgnoreUnloadHandlers));
  }

  return ChromeBrowserMainParts::PreMainMessageLoopRun();
}
