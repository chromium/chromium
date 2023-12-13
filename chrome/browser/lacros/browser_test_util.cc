// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lacros/browser_test_util.h"
#include "base/memory/raw_ptr.h"

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/ui/lacros/window_utility.h"
#include "chromeos/lacros/lacros_service.h"
#include "ui/aura/env.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher_observer.h"
#include "ui/platform_window/platform_window.h"
#include "ui/views/widget/desktop_aura/desktop_window_tree_host_lacros.h"

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

    if (Done()) {
      run_loop_->Quit();
    }
  }

  bool Done() const { return mouse_down_seen_ && mouse_up_seen_; }

 private:
  // Must outlive the observer.
  raw_ptr<base::RunLoop> run_loop_;
  bool mouse_down_seen_ = false;
  bool mouse_up_seen_ = false;
};

bool IsTestControllerAvailable(
    crosapi::mojom::TestController::MethodMinVersions min_version) {
  auto* lacros_service = chromeos::LacrosService::Get();
  if (!lacros_service ||
      !lacros_service->IsAvailable<crosapi::mojom::TestController>()) {
    return false;
  }

  int interface_version =
      lacros_service->GetInterfaceVersion<crosapi::mojom::TestController>();
  return (interface_version >= static_cast<int>(min_version));
}

bool WaitForWindow(const std::string& id, bool exists) {
  if (!IsTestControllerAvailable(
          crosapi::mojom::TestController::MethodMinVersions::
              kDoesWindowExistMinVersion)) {
    return false;
  }
  base::RunLoop outer_loop;
  bool actual_exists = false;
  auto wait_for_window = base::BindRepeating(
      [](base::RunLoop* outer_loop, const std::string& id, bool expected_exists,
         bool* actual_exists) {
        auto* lacros_service = chromeos::LacrosService::Get();
        base::RunLoop inner_loop(base::RunLoop::Type::kNestableTasksAllowed);
        lacros_service->GetRemote<crosapi::mojom::TestController>()
            ->DoesWindowExist(
                id, base::BindOnce(
                        [](base::RunLoop* loop, bool* out_exist, bool exist) {
                          *out_exist = std::move(exist);
                          loop->Quit();
                        },
                        &inner_loop, actual_exists));
        inner_loop.Run();

        if (*actual_exists == expected_exists) {
          outer_loop->Quit();
        }
      },
      &outer_loop, id, exists, &actual_exists);

  // Wait for the window to exist / not exist.
  base::RepeatingTimer timer;
  timer.Start(FROM_HERE, base::Milliseconds(1), std::move(wait_for_window));
  outer_loop.Run();
  return actual_exists == exists;
}

bool WaitForElement(const std::string& id, bool exists) {
  if (!IsTestControllerAvailable(
          crosapi::mojom::TestController::MethodMinVersions::
              kDoesElementExistMinVersion)) {
    return false;
  }
  base::RunLoop outer_loop;
  bool actual_exists = false;
  auto wait_for_element = base::BindRepeating(
      [](base::RunLoop* outer_loop, const std::string& id, bool expected_exists,
         bool* actual_exists) {
        auto* lacros_service = chromeos::LacrosService::Get();
        base::RunLoop inner_loop(base::RunLoop::Type::kNestableTasksAllowed);
        lacros_service->GetRemote<crosapi::mojom::TestController>()
            ->DoesElementExist(
                id, base::BindOnce(
                        [](base::RunLoop* loop, bool* out_exist, bool exist) {
                          *out_exist = std::move(exist);
                          loop->Quit();
                        },
                        &inner_loop, actual_exists));
        inner_loop.Run();

        if (*actual_exists == expected_exists) {
          outer_loop->Quit();
        }
      },
      &outer_loop, id, exists, &actual_exists);

  // Wait for the element to exist / not exist.
  base::RepeatingTimer timer;
  timer.Start(FROM_HERE, base::Milliseconds(1), std::move(wait_for_element));
  outer_loop.Run();
  return actual_exists == exists;
}

}  // namespace

bool WaitForElementCreation(const std::string& element_name) {
  return WaitForElement(element_name, /*exists=*/true);
}

bool WaitForWindowCreation(const std::string& id) {
  return WaitForWindow(id, /*exists=*/true);
}

bool WaitForWindowDestruction(const std::string& id) {
  return WaitForWindow(id, /*exists=*/false);
}

bool WaitForShelfItem(const std::string& id, bool exists) {
  if (!IsTestControllerAvailable(
          crosapi::mojom::TestController::MethodMinVersions::
              kDoesItemExistInShelfMinVersion)) {
    return false;
  }
  base::RunLoop outer_loop;
  bool actual_exists = false;
  auto wait_for_shelf_item = base::BindRepeating(
      [](base::RunLoop* outer_loop, const std::string& id, bool expected_exists,
         bool* actual_exists) {
        auto* lacros_service = chromeos::LacrosService::Get();
        base::RunLoop inner_loop(base::RunLoop::Type::kNestableTasksAllowed);
        lacros_service->GetRemote<crosapi::mojom::TestController>()
            ->DoesItemExistInShelf(
                id, base::BindOnce(
                        [](base::RunLoop* loop, bool* out_exist, bool exist) {
                          *out_exist = std::move(exist);
                          loop->Quit();
                        },
                        &inner_loop, actual_exists));
        inner_loop.Run();

        if (*actual_exists == expected_exists) {
          outer_loop->Quit();
        }
      },
      &outer_loop, id, exists, &actual_exists);

  // Wait for the item to exist / not exist.
  base::RepeatingTimer timer;
  timer.Start(FROM_HERE, base::Milliseconds(1), std::move(wait_for_shelf_item));
  outer_loop.Run();
  return actual_exists == exists;
}

bool WaitForShelfItemState(const std::string& id,
                           uint32_t state,
                           const base::Location& location) {
  if (!IsTestControllerAvailable(
          crosapi::mojom::TestController::MethodMinVersions::
              kGetShelfItemStateMinVersion)) {
    return false;
  }
  base::RunLoop outer_loop;
  uint32_t actual_state =
      static_cast<uint32_t>(crosapi::mojom::ShelfItemState::kNormal);
  auto wait_for_state = base::BindRepeating(
      [](base::RunLoop* outer_loop, const std::string& id,
         uint32_t expected_state, uint32_t* actual_state) {
        auto* lacros_service = chromeos::LacrosService::Get();
        base::RunLoop inner_loop(base::RunLoop::Type::kNestableTasksAllowed);
        lacros_service->GetRemote<crosapi::mojom::TestController>()
            ->GetShelfItemState(id,
                                base::BindOnce(
                                    [](base::RunLoop* loop, uint32_t* out_state,
                                       uint32_t state) {
                                      *out_state = std::move(state);
                                      loop->Quit();
                                    },
                                    &inner_loop, actual_state));
        inner_loop.Run();

        if (*actual_state == expected_state) {
          outer_loop->Quit();
        }
      },
      &outer_loop, id, state, &actual_state);

  base::RepeatingTimer timer;
  timer.Start(FROM_HERE, base::Milliseconds(1), std::move(wait_for_state));
  outer_loop.Run(location);
  return actual_state == state;
}

// Sends a TestController message to Ash to send a mouse click to this |window|.
// Waits for both the mouse-down and the mouse-up events to be seen by
// |window|. The AuraObserver only waits for the up-event to start processing
// before quitting the run loop.
bool SendAndWaitForMouseClick(aura::Window* window) {
  if (!IsTestControllerAvailable(
          crosapi::mojom::TestController::MethodMinVersions::
              kClickWindowMinVersion)) {
    return false;
  }
  DCHECK(window->IsRootWindow());
  std::string id = lacros_window_utility::GetRootWindowUniqueId(window);

  base::RunLoop run_loop;
  std::unique_ptr<AuraObserver> obs = std::make_unique<AuraObserver>(&run_loop);
  aura::Env::GetInstance()->AddWindowEventDispatcherObserver(obs.get());

  auto* lacros_service = chromeos::LacrosService::Get();
  lacros_service->GetRemote<crosapi::mojom::TestController>()->ClickWindow(id);
  run_loop.Run();
  aura::Env::GetInstance()->RemoveWindowEventDispatcherObserver(obs.get());
  return obs->Done();
}

}  // namespace browser_test_util
