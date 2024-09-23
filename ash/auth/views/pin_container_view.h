// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_AUTH_VIEWS_PIN_CONTAINER_VIEW_H_
#define ASH_AUTH_VIEWS_PIN_CONTAINER_VIEW_H_

#include <memory>
#include <string>

#include "ash/ash_export.h"
#include "ash/auth/views/auth_input_row_view.h"
#include "ash/auth/views/pin_keyboard_input_bridge.h"
#include "ash/auth/views/pin_keyboard_view.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/view.h"

namespace ash {

// Contains the PIN keyboard and the PIN input field and use
// PinKeyboardInputBridge to connects their functions.

class ASH_EXPORT PinContainerView : public views::View {
  METADATA_HEADER(PinContainerView, views::View)
 public:
  using Observer = AuthInputRowView::Observer;

  class TestApi {
   public:
    explicit TestApi(PinContainerView* view);
    ~TestApi();
    TestApi(const TestApi&) = delete;
    TestApi& operator=(const TestApi&) = delete;

    raw_ptr<AuthInputRowView> GetAuthInputRowView();
    raw_ptr<PinKeyboardView> GetPinKeyboardView();

    raw_ptr<PinContainerView> GetView();

   private:
    const raw_ptr<PinContainerView> view_;
  };

  PinContainerView();

  PinContainerView(const PinContainerView&) = delete;
  PinContainerView& operator=(const PinContainerView&) = delete;

  ~PinContainerView() override;

  // views::View:
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;
  std::string GetObjectName() const override;
  void RequestFocus() override;

  // Enables or disables the following UI elements:
  // - View
  // - Auth input row
  // No "Get" function is needed since the state is the same as
  // the GetEnabled return value.
  void SetInputEnabled(bool enabled);

  // Clear the textfield and set the display text button to hide state.
  void ResetState();

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 private:
  raw_ptr<PinKeyboardView> pin_keyboard_ = nullptr;

  raw_ptr<AuthInputRowView> auth_input_ = nullptr;

  std::unique_ptr<PinKeyboardInputBridge> bridge_;

  base::WeakPtrFactory<PinContainerView> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_AUTH_VIEWS_PIN_CONTAINER_VIEW_H_
