// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/fuchsia/chrome_browser_main_parts_fuchsia.h"

#include <fuchsia/element/cpp/fidl.h>
#include <fuchsia/ui/scenic/cpp/fidl.h>
#include <fuchsia/ui/views/cpp/fidl.h>
#include <lib/sys/cpp/component_context.h>

#include <memory>
#include <utility>

#include "base/check.h"
#include "base/command_line.h"
#include "base/fuchsia/fuchsia_logging.h"
#include "base/fuchsia/process_context.h"
#include "base/fuchsia/process_lifecycle.h"
#include "base/fuchsia/scoped_service_binding.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "build/branding_buildflags.h"
#include "chrome/browser/fuchsia/element_manager_impl.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/common/chrome_switches.h"
#include "components/fuchsia_component_support/feedback_registration.h"
#include "components/keep_alive_registry/keep_alive_types.h"
#include "components/keep_alive_registry/scoped_keep_alive.h"
#include "ui/ozone/public/ozone_switches.h"
#include "ui/platform_window/fuchsia/initialize_presenter_api_view.h"

namespace {

// Registers product data for the Chrome browser Component. This should only
// be called once per browser instance, and the calling thread must have an
// async_dispatcher.
void RegisterChromeProductData() {
  // The URL cannot be obtained programmatically - see fxbug.dev/51490.
  constexpr char kComponentUrl[] =
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
      "fuchsia-pkg://chrome.com/chrome#meta/chrome.cm";
#else
      "fuchsia-pkg://chromium.org/chrome#meta/chrome.cm";
#endif

  constexpr char kCrashProductName[] = "Chrome_Fuchsia";

  constexpr char kFeedbackAnnotationsNamespace[] =
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
      "google-chrome";
#else
      "chromium";
#endif

  fuchsia_component_support::RegisterProductDataForCrashReporting(
      kComponentUrl, kCrashProductName);

  fuchsia_component_support::RegisterProductDataForFeedback(
      kFeedbackAnnotationsNamespace);
}

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

bool NotifyNewBrowserWindow(const base::CommandLine& command_line) {
  base::FilePath path;
  return ChromeBrowserMainParts::ProcessSingletonNotificationCallback(
      command_line, path);
}

}  // namespace

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
    view_spec.set_annotations(
        element_manager_->annotations_manager().GetAnnotations());
    graphical_presenter_->PresentView(std::move(view_spec), nullptr,
                                      view_controller.NewRequest(),
                                      [](auto result) {});
    return view_controller;
  }

  fuchsia::element::ViewControllerPtr PresentFlatlandView(
      fuchsia::ui::views::ViewportCreationToken viewport_creation_token) {
    fuchsia::element::ViewControllerPtr view_controller;
    fuchsia::element::ViewSpec view_spec;
    view_spec.set_viewport_creation_token(std::move(viewport_creation_token));
    view_spec.set_annotations(
        element_manager_->annotations_manager().GetAnnotations());
    graphical_presenter_->PresentView(std::move(view_spec), nullptr,
                                      view_controller.NewRequest(),
                                      [](auto result) {});
    return view_controller;
  }

  base::raw_ptr<ElementManagerImpl> element_manager_;
  fuchsia::element::GraphicalPresenterPtr graphical_presenter_;
};

ChromeBrowserMainPartsFuchsia::ChromeBrowserMainPartsFuchsia(
    bool is_integration_test,
    StartupData* startup_data)
    : ChromeBrowserMainParts(is_integration_test, startup_data) {}

ChromeBrowserMainPartsFuchsia::~ChromeBrowserMainPartsFuchsia() = default;

void ChromeBrowserMainPartsFuchsia::ShowMissingLocaleMessageBox() {
  // Locale data should be bundled for all possible platform locales,
  // so crash here to make missing-locale states more visible.
  CHECK(false);
}

int ChromeBrowserMainPartsFuchsia::PreEarlyInitialization() {
  HandleOzonePlatformArgs();

  // The shell will make an ElementManager.ProposeElement() request when it
  // wants Chrome to display itself, including when first launched, so prevent
  // proactive display of a window on startup.
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kNoStartupWindow);

  return ChromeBrowserMainParts::PreEarlyInitialization();
}

void ChromeBrowserMainPartsFuchsia::PostCreateMainMessageLoop() {
  RegisterChromeProductData();

  ChromeBrowserMainParts::PostCreateMainMessageLoop();
}

int ChromeBrowserMainPartsFuchsia::PreMainMessageLoopRun() {
  // Configure Ozone to create top-level Views via GraphicalPresenter.
  element_manager_ = std::make_unique<ElementManagerImpl>(
      base::ComponentContextForProcess()->outgoing().get(),
      base::BindRepeating(&NotifyNewBrowserWindow));
  view_presenter_ = std::make_unique<ViewPresenter>(element_manager_.get());

  // Ensure that the browser process remains live until the first browser
  // window is opened by an ElementManager request. The browser will then
  // terminate itself as soon as the last browser window is closed, or it
  // is explicitly terminated by the Component Framework (see below).
  // TODO(crbug.com/1314718): Integrate with the Framework to coordinate
  // teardown, to avoid risk of in-flight requests being dropped.
  keep_alive_ = std::make_unique<ScopedKeepAlive>(
      KeepAliveOrigin::BROWSER_PROCESS_FUCHSIA,
      KeepAliveRestartOption::ENABLED);
  BrowserList::AddObserver(this);

  // Browser tests run in TestLauncher sub-processes, which do not have
  // some of the startup handles provided by the ELF runner when running as
  // a component in production, so disable features that require them.
  if (!is_integration_test()) {
    // chrome::ExitIgnoreUnloadHandlers() will perform a graceful shutdown,
    // flushing any pending data.  All Browser windows will then be closed,
    // removing them from the keep-alive reasons. Finally, the
    // BROWSER_PROCESS_FUCHSIA keep-alive (see above) must be manually
    // cleared.
    auto quit_closure =
        base::BindOnce(&chrome::ExitIgnoreUnloadHandlers)
            .Then(base::BindOnce(&std::unique_ptr<ScopedKeepAlive>::reset,
                                 base::Unretained(&keep_alive_), nullptr));

    lifecycle_ =
        std::make_unique<base::ProcessLifecycle>(std::move(quit_closure));

    // Take the outgoing-directory channel request from the startup handles,
    // and start serving requests over it (e.g. for outgoing services, Inspect
    // data, etc). This is not possible (see above), in browser tests,
    // where TestComponentContextForProcess() should be used to reach the
    // outgoing directory if necessary.
    zx_status_t status =
        base::ComponentContextForProcess()->outgoing()->ServeFromStartupInfo();
    ZX_CHECK(status == ZX_OK, status);
  }

  return ChromeBrowserMainParts::PreMainMessageLoopRun();
}

void ChromeBrowserMainPartsFuchsia::OnBrowserAdded(Browser* browser) {
  BrowserList::RemoveObserver(this);
  // TODO(crbug.com/1314718): Integrate with the Component Framework to tear
  // this down only when safe to terminate.
  keep_alive_ = nullptr;
}
