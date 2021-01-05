// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lacros/browser_test_util.h"

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chromeos/lacros/lacros_chrome_service_impl.h"
#include "ui/aura/env.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher_observer.h"
#include "ui/aura/window_tree_host_platform.h"
#include "ui/platform_window/platform_window.h"

namespace browser_test_util {
namespace {

// Observes Aura and waits for both a mouse down and a mouse up event.
class AuraObserver : public aura::WindowEventDispatcherObserver {
 public:
  explicit AuraObserver(base::RunLoop* run_loop) : run_loop_(run_loop) {}

  void OnWindowEventDispatcherStartedProcessing(
      aura::WindowEventDispatcher* dispatcher,
      const ui::Event& event) override {
    if (event.type() == ui::EventType::ET_MOUSE_PRESSED)
      mouse_down_seen_ = true;
    if (mouse_down_seen_ && event.type() == ui::EventType::ET_MOUSE_RELEASED)
      mouse_up_seen_ = true;

    if (mouse_down_seen_ && mouse_up_seen_)
      run_loop_->Quit();
  }

 private:
  // Must outlive the observer.
  base::RunLoop* run_loop_;
  bool mouse_down_seen_ = false;
  bool mouse_up_seen_ = false;
};

void WaitForWindow(const std::string& id, bool exists) {
  base::RunLoop outer_loop;
  auto wait_for_window = base::BindRepeating(
      [](base::RunLoop* outer_loop, const std::string& id,
         bool expected_exists) {
        auto* lacros_chrome_service = chromeos::LacrosChromeServiceImpl::Get();
        CHECK(lacros_chrome_service->IsTestControllerAvailable());

        base::RunLoop inner_loop(base::RunLoop::Type::kNestableTasksAllowed);
        bool exists = false;
        lacros_chrome_service->test_controller_remote()->DoesWindowExist(
            id, base::BindOnce(
                    [](base::RunLoop* loop, bool* out_exist, bool exist) {
                      *out_exist = std::move(exist);
                      loop->Quit();
                    },
                    &inner_loop, &exists));
        inner_loop.Run();

        if (exists == expected_exists)
          outer_loop->Quit();
      },
      &outer_loop, id, exists);

  // Wait for the window to be available.
  base::RepeatingTimer timer;
  timer.Start(FROM_HERE, base::TimeDelta::FromMilliseconds(1),
              std::move(wait_for_window));
  outer_loop.Run();
}

}  // namespace

std::string GetWindowId(aura::Window* window) {
  DCHECK(window);
  DCHECK(window->IsRootWindow());
  // On desktop aura there is one WindowTreeHost per top-level window.
  aura::WindowTreeHost* window_tree_host = window->GetHost();
  DCHECK(window_tree_host);
  // Lacros is based on Ozone/Wayland, which uses PlatformWindow and
  // aura::WindowTreeHostPlatform.
  aura::WindowTreeHostPlatform* window_tree_host_platform =
      static_cast<aura::WindowTreeHostPlatform*>(window_tree_host);
  return window_tree_host_platform->platform_window()->GetWindowUniqueId();
}

void WaitForWindowCreation(const std::string& id) {
  WaitForWindow(id, /*exists=*/true);
}

void WaitForWindowDestruction(const std::string& id) {
  WaitForWindow(id, /*exists=*/false);
}

// Sends a TestController message to Ash to send a mouse click to this |window|.
// Waits for both the mouse-down and the mouse-up events to be seen by
// |window|. The AuraObserver only waits for the up-event to start processing
// before quitting the run loop.
void SendAndWaitForMouseClick(aura::Window* window) {
  DCHECK(window->IsRootWindow());
  std::string id = GetWindowId(window);

  base::RunLoop run_loop;
  std::unique_ptr<AuraObserver> obs = std::make_unique<AuraObserver>(&run_loop);
  aura::Env::GetInstance()->AddWindowEventDispatcherObserver(obs.get());

  auto* lacros_chrome_service = chromeos::LacrosChromeServiceImpl::Get();
  lacros_chrome_service->test_controller_remote()->ClickWindow(id);
  run_loop.Run();
  aura::Env::GetInstance()->RemoveWindowEventDispatcherObserver(obs.get());
}

}  // namespace browser_test_util
