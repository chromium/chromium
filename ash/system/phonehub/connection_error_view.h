// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_PHONEHUB_CONNECTION_ERROR_VIEW_H_
#define ASH_SYSTEM_PHONEHUB_CONNECTION_ERROR_VIEW_H_

#include "ash/ash_export.h"
#include "ash/system/phonehub/phone_hub_content_view.h"
#include "ui/views/controls/button/button.h"

namespace chromeos {
namespace phonehub {
class ConnectionScheduler;
}  // namespace phonehub
}  // namespace chromeos

namespace ash {

class PhoneHubInterstitialView;

// An interstitial view represeting that the Phone Hub feature is not available
// due to connection issues.
class ASH_EXPORT ConnectionErrorView : public PhoneHubContentView,
                                       public views::ButtonListener {
 public:
  METADATA_HEADER(ConnectionErrorView);

  // Defines possible connection error states of the Phone Hub feature.
  enum class ErrorStatus {
    kDisconnected,  // The connection to the phone has been interrupted.
    kReconnecting,  // Attempts to resume the connection to the phone.
  };

  ConnectionErrorView(
      ErrorStatus error,
      chromeos::phonehub::ConnectionScheduler* connection_scheduler);
  ConnectionErrorView(const ConnectionErrorView&) = delete;
  ConnectionErrorView& operator=(const ConnectionErrorView&) = delete;
  ~ConnectionErrorView() override;

  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

 private:
  chromeos::phonehub::ConnectionScheduler* connection_scheduler_ = nullptr;

  PhoneHubInterstitialView* content_view_ = nullptr;
};

}  // namespace ash

#endif  // ASH_SYSTEM_PHONEHUB_CONNECTION_ERROR_VIEW_H_
