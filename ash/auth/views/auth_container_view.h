// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_AUTH_VIEWS_AUTH_CONTAINER_VIEW_H_
#define ASH_AUTH_VIEWS_AUTH_CONTAINER_VIEW_H_

#include <memory>
#include <string>

#include "ash/ash_export.h"
#include "ash/auth/views/auth_common.h"
#include "ash/auth/views/auth_input_row_view.h"
#include "ash/auth/views/fingerprint_view.h"
#include "ash/auth/views/pin_container_view.h"
#include "ash/auth/views/pin_status_view.h"
#include "ash/login/ui/animated_rounded_image_view.h"
#include "ash/style/icon_button.h"
#include "ash/style/pill_button.h"
#include "base/containers/enum_set.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view.h"

namespace cryptohome {
class PinStatus;
}  // namespace cryptohome

namespace ash {

// AuthContainer is a view that contains the authentication views currently only
// the PinContainerView and the AuthInputRow aka password. Later it can be
// extended with additional views e.g fingerprint. If a password and PIN are
// both available, the view displays a toggle button, allowing the user to
// switch between authentication methods.
class ASH_EXPORT AuthContainerView : public views::View {
  METADATA_HEADER(AuthContainerView, views::View)
 public:
  // Observer Interface: Notifies about events within the AuthContainerView
  // (e.g., password submission, PIN submission, etc.)
  class Observer : public base::CheckedObserver {
   public:
    virtual void OnPasswordSubmit(const std::u16string& password) {}
    virtual void OnPinSubmit(const std::u16string& pin) {}
    // Escape key was pressed.
    virtual void OnEscape() {}
    // Something happened on the view e.g: the switch button was pressed or the
    // input textfield is changed.
    virtual void OnContentsChanged() {}
  };

  class TestApi {
   public:
    explicit TestApi(AuthContainerView* view);
    ~TestApi();
    TestApi(const TestApi&) = delete;
    TestApi& operator=(const TestApi&) = delete;

    raw_ptr<PinContainerView> GetPinContainerView();

    raw_ptr<AuthInputRowView> GetPasswordView();

    raw_ptr<views::Button> GetSwitchButton();

    raw_ptr<PinStatusView> GetPinStatusView();

    raw_ptr<FingerprintView> GetFingerprintView();

    AuthInputType GetCurrentInputType();

    raw_ptr<AuthContainerView> GetView();

   private:
    const raw_ptr<AuthContainerView> view_;
  };

  AuthContainerView(AuthFactorSet auth_factors);

  AuthContainerView(const AuthContainerView&) = delete;
  AuthContainerView& operator=(const AuthContainerView&) = delete;

  ~AuthContainerView() override;

  // views::View:
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;
  std::string GetObjectName() const override;
  void RequestFocus() override;

  // Observer Management.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Setters and Getters for password and PIN.
  void SetHasPassword(bool has_password);
  bool HasPassword() const;

  void SetHasPin(bool has_pin);
  bool HasPin() const;
  void SetPinStatus(std::unique_ptr<cryptohome::PinStatus> pin_status);
  const std::u16string& GetPinStatusMessage() const;

  // Enables or disables the following UI elements:
  // - View
  // - Password input
  // - PIN container
  // - Switch button
  // No "Get" function is needed since the state is the same as
  // the GetEnabled return value.
  void SetInputEnabled(bool enabled);

  // Actions:
  void ToggleCurrentAuthType();
  void PinSubmit(const std::u16string& pin) const;
  void PasswordSubmit(const std::u16string& password) const;
  void Escape() const;
  void ContentsChanged() const;
  // Reset the input fields text and visibility.
  void ResetInputfields();

  // FingerprintView actions:
  void SetFingerprintState(FingerprintState state);
  void NotifyFingerprintAuthSuccess(
      base::OnceClosure on_success_animation_finished);
  void NotifyFingerprintAuthFailure();

 private:
  // Internal methods for managing views.
  void AddPasswordView();
  void AddPinView();
  void AddSwitchButton();
  void AddPinStatusView();
  void AddFingerprintView();
  void UpdateAuthInput();
  void UpdateSwitchButtonState();

  // Layout and Components:
  raw_ptr<views::BoxLayout> layout_ = nullptr;
  raw_ptr<PinContainerView> pin_container_ = nullptr;
  std::unique_ptr<PinContainerView::Observer> pin_observer_;
  raw_ptr<AuthInputRowView> password_view_ = nullptr;
  std::unique_ptr<AuthInputRowView::Observer> password_observer_;
  raw_ptr<PinStatusView> pin_status_ = nullptr;
  raw_ptr<FingerprintView> fingerprint_view_ = nullptr;

  // Switch Button and Spacer. When the switch button is hidden
  // this also should be hidden.
  raw_ptr<views::View> switch_button_spacer_ = nullptr;
  raw_ptr<views::LabelButton> switch_button_ = nullptr;

  // State:
  AuthInputType current_input_type_ = AuthInputType::kPassword;

  base::ObserverList<Observer> observers_;

  AuthFactorSet available_auth_factors_;

  base::WeakPtrFactory<AuthContainerView> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_AUTH_VIEWS_AUTH_CONTAINER_VIEW_H_
