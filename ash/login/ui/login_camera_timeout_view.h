// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_LOGIN_UI_LOGIN_CAMERA_TIMEOUT_VIEW_H_
#define ASH_LOGIN_UI_LOGIN_CAMERA_TIMEOUT_VIEW_H_

#include "ash/ash_export.h"
#include "ash/login/ui/non_accessible_view.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/label.h"
#include "ui/views/view.h"

namespace ash {

class ArrowButtonView;

// UI that is shown on camera usage timeout after the third-party SAML dialog
// has been dismissed. It consists of a text message and an arrow button, which
// leads user back to sign-in page. For more info check discussion under privacy
// review in FLB crbug.com/1221337.
class ASH_EXPORT LoginCameraTimeoutView : public NonAccessibleView {
  METADATA_HEADER(LoginCameraTimeoutView, NonAccessibleView)

 public:
  // TestApi is used for tests to get internal implementation details.
  class ASH_EXPORT TestApi {
   public:
    explicit TestApi(LoginCameraTimeoutView* view);
    ~TestApi();

    views::View* arrow_button() const;

   private:
    const raw_ptr<LoginCameraTimeoutView> view_;
  };

  using OnPublicAccountTapped = base::RepeatingClosure;

  LoginCameraTimeoutView(views::Button::PressedCallback callback);

  LoginCameraTimeoutView(const LoginCameraTimeoutView&) = delete;
  LoginCameraTimeoutView& operator=(const LoginCameraTimeoutView&) = delete;

  ~LoginCameraTimeoutView() override;

  // views::View:
  void RequestFocus() override;

 private:
  raw_ptr<views::Label> title_;
  raw_ptr<views::Label> subtitle_;
  raw_ptr<ArrowButtonView> arrow_button_;
};

}  // namespace ash

#endif  // ASH_LOGIN_UI_LOGIN_CAMERA_TIMEOUT_VIEW_H_
