// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_LOGIN_UI_LOCAL_AUTHENTICATION_REQUEST_WIDGET_H_
#define ASH_LOGIN_UI_LOCAL_AUTHENTICATION_REQUEST_WIDGET_H_

#include <memory>
#include <string>

#include "ash/ash_export.h"
#include "ash/login/ui/local_authentication_request_view.h"
#include "ash/public/cpp/login/local_authentication_request_controller.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"

namespace views {
class Widget;
}

namespace ash {

enum class LocalAuthenticationRequestReason;
enum class LocalAuthenticationRequestViewState;
class UserContext;

// Widget to display the Local Password Request View in a standalone container.
class ASH_EXPORT LocalAuthenticationRequestWidget {
 public:
  class ASH_EXPORT TestApi {
   public:
    TestApi();
    ~TestApi();

    // Returns nullptr if the dialog does not exists.
    static LocalAuthenticationRequestView* GetView();
    // Returns false if the dialog does not exists or exists but not visible.
    static bool IsVisible();
    // Returns false if the dialog does not exists.
    static bool CancelDialog();
    // Returns false if the dialog does not exists.
    static bool SubmitPassword(const std::string& password);
  };

  LocalAuthenticationRequestWidget(const LocalAuthenticationRequestWidget&) =
      delete;
  LocalAuthenticationRequestWidget& operator=(
      const LocalAuthenticationRequestWidget&) = delete;

  // Creates and shows the instance of LocalAuthenticationRequestWidget.
  // This widget is modal and only one instance can be created at a time. It
  // will be destroyed when dismissed.
  static void Show(
      LocalAuthenticationCallback local_authentication_callback,
      const std::u16string& title,
      const std::u16string& description,
      base::WeakPtr<LocalAuthenticationRequestView::Delegate> delegate,
      std::unique_ptr<UserContext> user_context);

  // Returns the instance of LocalAuthenticationRequestWidget or nullptr if it
  // does not exits.
  static LocalAuthenticationRequestWidget* Get();

  // Toggles showing an error state and updates displayed strings.
  void UpdateState(LocalAuthenticationRequestViewState state,
                   const std::u16string& title,
                   const std::u16string& description);

  // Enables or disables input textfield.
  void SetInputEnabled(bool enabled);

  // Clears previously entered password.
  void ClearInput();

  // Closes the widget.
  // |success| describes whether the validation was successful and is passed to
  // |on_local_authentication_request_done_|.
  void Close(bool success, std::unique_ptr<UserContext> user_context);

  // Returns the associated view for testing purposes.
  static LocalAuthenticationRequestView* GetViewForTesting();

 private:
  LocalAuthenticationRequestWidget(
      LocalAuthenticationCallback local_authentication_callback,
      const std::u16string& title,
      const std::u16string& description,
      base::WeakPtr<LocalAuthenticationRequestView::Delegate> delegate,
      std::unique_ptr<UserContext> user_context);
  ~LocalAuthenticationRequestWidget();

  // Shows the |widget_|.
  void Show();

  // Returns the associated view.
  LocalAuthenticationRequestView* GetView();

  // Callback invoked when closing the widget.
  LocalAuthenticationCallback local_authentication_callback_;

  std::unique_ptr<views::Widget> widget_;

  base::WeakPtrFactory<LocalAuthenticationRequestWidget> weak_factory_{this};
};

}  // namespace ash

#endif  // ASH_LOGIN_UI_LOCAL_AUTHENTICATION_REQUEST_WIDGET_H_
