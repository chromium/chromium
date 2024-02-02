// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_NOTIFICATIONS_REQUEST_SYSTEM_PROXY_CREDENTIALS_VIEW_H_
#define CHROME_BROWSER_ASH_NOTIFICATIONS_REQUEST_SYSTEM_PROXY_CREDENTIALS_VIEW_H_

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/ui_base_types.h"
#include "ui/views/view.h"
#include "ui/views/window/dialog_delegate.h"

namespace views {
class Label;
class Textfield;
}  // namespace views

namespace ash {

// A dialog box for requesting proxy authentication credentials for network
// traffic at OS level (outside the browser).
class RequestSystemProxyCredentialsView final
    : public views::DialogDelegateView {
  METADATA_HEADER(RequestSystemProxyCredentialsView, views::DialogDelegateView)

 public:
  RequestSystemProxyCredentialsView(
      const std::string& proxy_server,
      bool show_error_label,
      base::OnceClosure view_destruction_callback);
  RequestSystemProxyCredentialsView(const RequestSystemProxyCredentialsView&) =
      delete;
  RequestSystemProxyCredentialsView& operator=(
      const RequestSystemProxyCredentialsView&) = delete;
  ~RequestSystemProxyCredentialsView() override;

  // views::DialogDelegateView
  views::View* GetInitiallyFocusedView() override;
  std::u16string GetWindowTitle() const override;
  bool ShouldShowCloseButton() const override;

  // Returns the proxy server for which the dialog is asking for credentials,
  // in the format scheme://host:port.
  const std::string& GetProxyServer() const;

  std::u16string GetUsername() const;
  std::u16string GetPassword() const;

  views::Textfield* username_textfield_for_testing() {
    return username_textfield_;
  }
  views::Textfield* password_textfield_for_testing() {
    return password_textfield_;
  }
  views::Label* error_label_for_testing() { return error_label_; }

 private:
  void Init();

  const std::u16string window_title_;

  raw_ptr<views::Textfield> username_textfield_ = nullptr;
  raw_ptr<views::Textfield> password_textfield_ = nullptr;
  raw_ptr<views::Label> error_label_ = nullptr;

  const std::string proxy_server_;
  const bool show_error_label_;
  base::OnceClosure view_destruction_callback_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_NOTIFICATIONS_REQUEST_SYSTEM_PROXY_CREDENTIALS_VIEW_H_
