// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accelerators/ash_focus_manager_factory.h"

#include <memory>

#include "ash/accelerators/accelerator_controller_impl.h"
#include "ash/accessibility/magnifier/docked_magnifier_controller.h"
#include "ash/accessibility/magnifier/fullscreen_magnifier_controller.h"
#include "ash/shell.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/focus/focus_manager_delegate.h"
#include "ui/views/view.h"

namespace ash {
namespace {

// As the name implies, this class is responsible for handling accelerators
// *after* pre-target accelerators and after the target is given a chance to
// process the accelerator.
class PostTargetAcceleratorHandler : public views::FocusManagerDelegate {
 public:
  PostTargetAcceleratorHandler() = default;

  PostTargetAcceleratorHandler(const PostTargetAcceleratorHandler&) = delete;
  PostTargetAcceleratorHandler& operator=(const PostTargetAcceleratorHandler&) =
      delete;

  ~PostTargetAcceleratorHandler() override = default;

  // views::FocusManagerDelegate overrides:
  bool ProcessAccelerator(const ui::Accelerator& accelerator) override;
};

bool PostTargetAcceleratorHandler::ProcessAccelerator(
    const ui::Accelerator& accelerator) {
  AcceleratorController* controller = Shell::Get()->accelerator_controller();
  return controller && controller->Process(accelerator);
}

}  // namespace

AshFocusManagerFactory::AshFocusManagerFactory() = default;
AshFocusManagerFactory::~AshFocusManagerFactory() = default;

std::unique_ptr<views::FocusManager> AshFocusManagerFactory::CreateFocusManager(
    views::Widget* widget) {
  return std::make_unique<views::FocusManager>(
      widget, std::make_unique<PostTargetAcceleratorHandler>());
}

}  // namespace ash
