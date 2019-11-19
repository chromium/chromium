// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accelerators/ash_focus_manager_factory.h"

#include <memory>

#include "ash/accelerators/accelerator_controller_impl.h"
#include "ash/magnifier/docked_magnifier_controller_impl.h"
#include "ash/magnifier/magnification_controller.h"
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
  ~PostTargetAcceleratorHandler() override = default;

  // views::FocusManagerDelegate overrides:
  bool ProcessAccelerator(const ui::Accelerator& accelerator) override;
  void OnDidChangeFocus(views::View* focused_before,
                        views::View* focused_now) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(PostTargetAcceleratorHandler);
};

bool PostTargetAcceleratorHandler::ProcessAccelerator(
    const ui::Accelerator& accelerator) {
  AcceleratorController* controller = Shell::Get()->accelerator_controller();
  return controller && controller->Process(accelerator);
}

void PostTargetAcceleratorHandler::OnDidChangeFocus(views::View* focused_before,
                                                    views::View* focused_now) {
  // TODO: seems as though this code should live closer to magnification code.
  if (!focused_now || focused_now == focused_before)
    return;

  const gfx::Rect bounds_in_screen = focused_now->GetBoundsInScreen();
  if (bounds_in_screen.IsEmpty())
    return;

  // Note that both magnifiers are mutually exclusive.
  DockedMagnifierControllerImpl* docked_magnifier =
      Shell::Get()->docked_magnifier_controller();
  MagnificationController* fullscreen_magnifier =
      Shell::Get()->magnification_controller();

  gfx::Point point_of_interest = bounds_in_screen.CenterPoint();
  const ui::InputMethod* input_method = focused_now->GetInputMethod();
  const bool docked_magnifier_enabled = docked_magnifier->GetEnabled();
  if (input_method && input_method->GetTextInputClient() &&
      input_method->GetTextInputClient()->GetTextInputType() !=
          ui::TEXT_INPUT_TYPE_NONE) {
    // If this view is a text input client with valid caret bounds, forward it
    // to the enabled magnifier as an |OnCaretBoundsChanged()| event, since the
    // magnifiers might have special handling for caret events.
    const gfx::Rect caret_bounds =
        input_method->GetTextInputClient()->GetCaretBounds();
    // Note: Don't use Rect::IsEmpty() for the below check as in many cases the
    // caret can have a zero width or height, but still be valid.
    if (caret_bounds.width() || caret_bounds.height()) {
      if (docked_magnifier_enabled) {
        docked_magnifier->OnCaretBoundsChanged(
            input_method->GetTextInputClient());
      } else if (fullscreen_magnifier->IsEnabled()) {
        fullscreen_magnifier->OnCaretBoundsChanged(
            input_method->GetTextInputClient());
      }

      return;
    }

    // If the caret bounds are unavailable, then use the view's top left corner
    // as the point of interest.
    point_of_interest = bounds_in_screen.origin();
  }

  if (docked_magnifier_enabled)
    docked_magnifier->CenterOnPoint(point_of_interest);
  else if (fullscreen_magnifier->IsEnabled())
    fullscreen_magnifier->CenterOnPoint(point_of_interest);
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
