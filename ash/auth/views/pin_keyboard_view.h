// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_AUTH_VIEWS_PIN_KEYBOARD_VIEW_H_
#define ASH_AUTH_VIEWS_PIN_KEYBOARD_VIEW_H_

#include "ash/ash_export.h"
#include "ash/style/icon_button.h"
#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/view.h"

namespace ash {

class ASH_EXPORT PinKeyboardView : public views::View {
  METADATA_HEADER(PinKeyboardView, views::View)
 public:
  class Observer : public base::CheckedObserver {
   public:
    virtual void OnDigitButtonPressed(int digit) {}
    virtual void OnBackspacePressed() {}
  };

  class TestApi {
   public:
    explicit TestApi(PinKeyboardView* view);
    ~TestApi();

    views::Button* backspace_button();
    views::Button* digit_button(int digit);

    void SetEnabled(bool enabled);
    bool GetEnabled();

    void AddObserver(Observer* observer);
    void RemoveObserver(Observer* observer);

    PinKeyboardView* GetView();

   private:
    const raw_ptr<PinKeyboardView> view_;
  };

  PinKeyboardView();

  PinKeyboardView(const PinKeyboardView&) = delete;
  PinKeyboardView& operator=(const PinKeyboardView&) = delete;

  ~PinKeyboardView() override;

  // views::View:
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 private:
  void OnDigitButtonPressed(int digit);
  void OnBackspacePressed();

  void AddDigitButton(int digit);

  base::ObserverList<Observer> observers_;

  raw_ptr<IconButton> backspace_button_ = nullptr;

  base::flat_map<int, raw_ptr<IconButton>> digit_buttons_;

  base::WeakPtrFactory<PinKeyboardView> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_AUTH_VIEWS_PIN_KEYBOARD_VIEW_H_
