// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_AUTH_VIEWS_ACTIVE_SESSION_AUTH_VIEW_H_
#define ASH_AUTH_VIEWS_ACTIVE_SESSION_AUTH_VIEW_H_

#include <memory>
#include <string>

#include "ash/ash_export.h"
#include "ash/auth/views/auth_container_view.h"
#include "ash/auth/views/auth_header_view.h"
#include "ash/public/cpp/login_types.h"
#include "ash/style/icon_button.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/scoped_observation.h"
#include "chromeos/ash/components/cryptohome/auth_factor.h"
#include "components/account_id/account_id.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view.h"

namespace cryptohome {
class PinStatus;
}  // namespace cryptohome

namespace ash {

// ActiveSessionAuthView is a view that contains a header view: close button,
// user avatar, title and description. Below the header view it also shows the
// authtentication container.
class ASH_EXPORT ActiveSessionAuthView : public views::View,
                                         public AuthContainerView::Observer,
                                         public AuthHeaderView::Observer {
  METADATA_HEADER(ActiveSessionAuthView, views::View)
 public:
  // Observer Interface: Notifies about events within the ActiveSessionAuthView
  // (e.g., password submission, PIN submission, contents changed, close
  // request)
  class Observer : public base::CheckedObserver {
   public:
    virtual void OnPasswordSubmit(const std::u16string& password) {}
    virtual void OnPinSubmit(const std::u16string& pin) {}
    virtual void OnClose() {}
  };

  class TestApi {
   public:
    explicit TestApi(ActiveSessionAuthView* view);
    ~TestApi();
    TestApi(const TestApi&) = delete;
    TestApi& operator=(const TestApi&) = delete;

    raw_ptr<AuthHeaderView> GetAuthHeaderView();
    raw_ptr<views::Button> GetCloseButton();
    raw_ptr<AuthContainerView> GetAuthContainerView();

    raw_ptr<ActiveSessionAuthView> GetView();

   private:
    const raw_ptr<ActiveSessionAuthView> view_;
  };

  ActiveSessionAuthView(const AccountId& account_id,
                        const std::u16string& title,
                        const std::u16string& description,
                        AuthFactorSet auth_factors);

  ActiveSessionAuthView(const ActiveSessionAuthView&) = delete;
  ActiveSessionAuthView& operator=(const ActiveSessionAuthView&) = delete;

  ~ActiveSessionAuthView() override;

  // views::View:
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;
  void ChildPreferredSizeChanged(views::View* child) override;
  std::string GetObjectName() const override;
  void RequestFocus() override;

  // AuthContainerView::Observer:
  void OnPasswordSubmit(const std::u16string& password) override;
  void OnPinSubmit(const std::u16string& pin) override;
  void OnEscape() override;
  void OnContentsChanged() override;

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

  // Enables or disables the input area of the view. The header area (e.g.,
  // close button) remains accessible even in the disabled state.
  void SetInputEnabled(bool enabled);

  // Actions:
  void Close();
  void SetErrorTitle(const std::u16string& error_str);
  // Reset the input fields text and visibility.
  void ResetInputfields();

  void OnTitleChanged(const std::u16string& error_str) override;

  // FingerprintView actions:
  void SetFingerprintState(FingerprintState state);
  void NotifyFingerprintAuthSuccess(
      base::OnceClosure on_success_animation_finished);
  void NotifyFingerprintAuthFailure();

 private:
  // Internal methods for managing views.
  void AddHeaderAndCloseButton(const std::u16string& title,
                               const std::u16string& description);
  void AddAuthContainer(AuthFactorSet auth_factors);

  // Layout and Components:
  raw_ptr<views::BoxLayout> layout_ = nullptr;
  raw_ptr<AuthHeaderView> auth_header_ = nullptr;
  raw_ptr<AuthContainerView> auth_container_ = nullptr;
  raw_ptr<IconButton> close_button_ = nullptr;

  const AccountId account_id_;

  base::ObserverList<Observer> observers_;

  base::ScopedObservation<AuthHeaderView, AuthHeaderView::Observer>
      header_observation_{this};

  base::WeakPtrFactory<ActiveSessionAuthView> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_AUTH_VIEWS_ACTIVE_SESSION_AUTH_VIEW_H_
