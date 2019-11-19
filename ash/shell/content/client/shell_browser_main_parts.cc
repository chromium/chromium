// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shell/content/client/shell_browser_main_parts.h"

#include <memory>
#include <utility>

#include "ash/keyboard/test_keyboard_ui.h"
#include "ash/login_status.h"
#include "ash/public/cpp/event_rewriter_controller.h"
#include "ash/session/test_pref_service_provider.h"
#include "ash/shell.h"
#include "ash/shell/content/client/shell_new_window_delegate.h"
#include "ash/shell/content/embedded_browser.h"
#include "ash/shell/example_app_list_client.h"
#include "ash/shell/example_session_controller_client.h"
#include "ash/shell/shell_delegate_impl.h"
#include "ash/shell/shell_views_delegate.h"
#include "ash/shell/window_type_launcher.h"
#include "ash/shell/window_watcher.h"
#include "ash/shell_init_params.h"
#include "ash/sticky_keys/sticky_keys_controller.h"
#include "ash/test/ash_test_helper.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/run_loop.h"
#include "chromeos/dbus/biod/biod_client.h"
#include "chromeos/dbus/shill/shill_clients.h"
#include "chromeos/network/network_handler.h"
#include "components/exo/file_helper.h"
#include "content/public/browser/context_factory.h"
#include "content/public/browser/system_connector.h"
#include "content/public/common/content_switches.h"
#include "content/shell/browser/shell_browser_context.h"
#include "net/base/net_module.h"
#include "services/service_manager/public/cpp/connector.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/ui_base_features.h"
#include "ui/chromeos/events/event_rewriter_chromeos.h"
#include "ui/views/examples/examples_window_with_content.h"

namespace ash {
namespace shell {
namespace {
ShellBrowserMainParts* main_parts = nullptr;
}

// static
content::BrowserContext* ShellBrowserMainParts::GetBrowserContext() {
  DCHECK(main_parts);
  return main_parts->browser_context();
}

ShellBrowserMainParts::ShellBrowserMainParts(
    const content::MainFunctionParams& parameters)
    : parameters_(parameters) {
  DCHECK(!main_parts);
  main_parts = this;
}

ShellBrowserMainParts::~ShellBrowserMainParts() {
  DCHECK(main_parts);
  main_parts = nullptr;
}

void ShellBrowserMainParts::PostEarlyInitialization() {
  content::BrowserMainParts::PostEarlyInitialization();
  chromeos::shill_clients::InitializeFakes();
  chromeos::NetworkHandler::Initialize();
}

void ShellBrowserMainParts::PreMainMessageLoopStart() {}

void ShellBrowserMainParts::PostMainMessageLoopStart() {
  chromeos::BiodClient::InitializeFake();
}

void ShellBrowserMainParts::ToolkitInitialized() {
  // A ViewsDelegate is required.
  views_delegate_ = std::make_unique<ShellViewsDelegate>();
}

void ShellBrowserMainParts::PreMainMessageLoopRun() {
  browser_context_.reset(new content::ShellBrowserContext(false));

  ash_test_helper_ = std::make_unique<AshTestHelper>();

  AshTestHelper::InitParams init_params;
  // TODO(oshima): Separate the class for ash_shell to reduce the test binary
  // size.
  if (parameters_.ui_task)
    init_params.config_type = AshTestHelper::kPerfTest;
  else {
    new_window_delegate_ = std::make_unique<ShellNewWindowDelegate>();
    init_params.config_type = AshTestHelper::kShell;
  }

  ShellInitParams shell_init_params;
  shell_init_params.delegate = std::make_unique<shell::ShellDelegateImpl>();
  shell_init_params.context_factory = content::GetContextFactory();
  shell_init_params.context_factory_private =
      content::GetContextFactoryPrivate();
  shell_init_params.connector = content::GetSystemConnector();
  shell_init_params.keyboard_ui_factory =
      std::make_unique<TestKeyboardUIFactory>();

  ash_test_helper_->SetUp(init_params, std::move(shell_init_params));

  window_watcher_ = std::make_unique<WindowWatcher>();

  Shell::GetPrimaryRootWindow()->GetHost()->Show();

  Shell::Get()->InitWaylandServer(nullptr);

  if (!parameters_.ui_task) {
    // Install Rewriter so that function keys are properly re-mapped.
    auto* event_rewriter_controller = EventRewriterController::Get();
    event_rewriter_controller->AddEventRewriter(
        std::make_unique<ui::EventRewriterChromeOS>(
            nullptr, Shell::Get()->sticky_keys_controller()));

    // Initialize session controller client and create fake user sessions. The
    // fake user sessions makes ash into the logged in state.
    example_session_controller_client_ =
        std::make_unique<ExampleSessionControllerClient>(
            Shell::Get()->session_controller(),
            ash_test_helper_->prefs_provider());
    example_session_controller_client_->Initialize();

    example_app_list_client_ = std::make_unique<ExampleAppListClient>(
        Shell::Get()->app_list_controller());

    shell::InitWindowTypeLauncher(
        base::BindRepeating(views::examples::ShowExamplesWindowWithContent,
                            base::Passed(base::OnceClosure()),
                            base::Unretained(browser_context_.get()), nullptr),
        base::BindRepeating(base::IgnoreResult(&EmbeddedBrowser::Create),
                            base::Unretained(browser_context_.get()),
                            GURL("https://www.google.com"), base::nullopt));
  }
}

bool ShellBrowserMainParts::MainMessageLoopRun(int* result_code) {
  if (parameters_.ui_task) {
    parameters_.ui_task->Run();
    delete parameters_.ui_task;
  } else {
    base::RunLoop run_loop;
    example_session_controller_client_->set_quit_closure(
        run_loop.QuitWhenIdleClosure());
    run_loop.Run();
  }
  return true;
}

void ShellBrowserMainParts::PostMainMessageLoopRun() {
  window_watcher_.reset();
  example_app_list_client_.reset();
  example_session_controller_client_.reset();

  ash_test_helper_->TearDown();
  ash_test_helper_.reset();

  views_delegate_.reset();

  // The keyboard may have created a WebContents. The WebContents is destroyed
  // with the UI, and it needs the BrowserContext to be alive during its
  // destruction. So destroy all of the UI elements before destroying the
  // browser context.
  browser_context_.reset();
}

void ShellBrowserMainParts::PostDestroyThreads() {
  chromeos::NetworkHandler::Shutdown();
  chromeos::shill_clients::Shutdown();
  content::BrowserMainParts::PostDestroyThreads();
}

}  // namespace shell
}  // namespace ash
