// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ACCESSIBILITY_SELECT_TO_SPEAK_EVENT_HANDLER_DELEGATE_H_
#define CHROME_BROWSER_CHROMEOS_ACCESSIBILITY_SELECT_TO_SPEAK_EVENT_HANDLER_DELEGATE_H_

#include <memory>

#include "ash/public/interfaces/accessibility_controller.mojom.h"
#include "base/macros.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "ui/events/event.h"

namespace chromeos {

// SelectToSpeakEventHandlerDelegate receives mouse and key events and forwards
// them to the Select-to-Speak extension in Chrome. The
// SelectToSpeakEventHandler in the Ash process handles events and passes them
// to this delegate via a Mojo interface defined in
// accessibility_controller.mojom.
class SelectToSpeakEventHandlerDelegate
    : public ash::mojom::SelectToSpeakEventHandlerDelegate {
 public:
  SelectToSpeakEventHandlerDelegate();
  ~SelectToSpeakEventHandlerDelegate() override;

 private:
  // ash::mojom::SelectToSpeakEventHandlerDelegate:
  void DispatchKeyEvent(std::unique_ptr<ui::Event> event) override;
  void DispatchMouseEvent(std::unique_ptr<ui::Event> event) override;

  mojo::Binding<ash::mojom::SelectToSpeakEventHandlerDelegate> binding_;

  DISALLOW_COPY_AND_ASSIGN(SelectToSpeakEventHandlerDelegate);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_ACCESSIBILITY_SELECT_TO_SPEAK_EVENT_HANDLER_DELEGATE_H_
