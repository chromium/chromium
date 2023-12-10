// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_LOGIN_UI_LOCAL_AUTHENTICATION_REQUEST_CONTROLLER_IMPL_H_
#define ASH_LOGIN_UI_LOCAL_AUTHENTICATION_REQUEST_CONTROLLER_IMPL_H_

#include <memory>

#include "ash/login/ui/local_authentication_request_view.h"
#include "ash/login/ui/local_authentication_request_widget.h"
#include "ash/public/cpp/login/local_authentication_request_controller.h"
#include "base/memory/weak_ptr.h"

namespace ash {

class UserContext;

// Implementation of LocalAuthenticationRequestController. It serves to finalize
// the re-auth session with local authentication.
class ASH_EXPORT LocalAuthenticationRequestControllerImpl
    : public LocalAuthenticationRequestController,
      public LocalAuthenticationRequestView::Delegate {
 public:
  LocalAuthenticationRequestControllerImpl();
  LocalAuthenticationRequestControllerImpl(
      const LocalAuthenticationRequestControllerImpl&) = delete;
  LocalAuthenticationRequestControllerImpl& operator=(
      const LocalAuthenticationRequestControllerImpl&) = delete;
  ~LocalAuthenticationRequestControllerImpl() override;

  // LocalAuthenticationRequestView::Delegate:
  void OnClose() override;

  // LocalAuthenticationRequestController:
  bool ShowWidget(LocalAuthenticationCallback local_authentication_callback,
                  std::unique_ptr<UserContext> user_context) override;

 private:
  base::WeakPtrFactory<LocalAuthenticationRequestControllerImpl> weak_factory_{
      this};
};

}  // namespace ash

#endif  // ASH_LOGIN_UI_LOCAL_AUTHENTICATION_REQUEST_CONTROLLER_IMPL_H_
