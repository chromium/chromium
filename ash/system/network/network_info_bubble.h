// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_NETWORK_NETWORK_INFO_BUBBLE_H_
#define ASH_SYSTEM_NETWORK_NETWORK_INFO_BUBBLE_H_

#include <string>

#include "ash/ash_export.h"
#include "base/memory/weak_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/events/event.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/widget/widget.h"

namespace ash {

// This class encapsulates the logic to find and show the IP addresses and mac
// addresses of the default network and available network technologies.
class ASH_EXPORT NetworkInfoBubble : public views::BubbleDialogDelegateView {
  METADATA_HEADER(NetworkInfoBubble, views::BubbleDialogDelegateView)

 public:
  // This class declares the interface that should be implemented by any class
  // that intends to instantiate NetworkInfoBubble.
  class Delegate {
   public:
    Delegate() = default;
    virtual ~Delegate() = default;

    // Used to determine whether the info bubble should include the mac
    // addresses of the ethernet, WiFi, and cellular devices.
    virtual bool ShouldIncludeDeviceAddresses() = 0;

    // Used to notify the delegate that the bubble is destructing.
    virtual void OnInfoBubbleDestroyed() = 0;
  };

  NetworkInfoBubble(base::WeakPtr<Delegate> delegate, views::View* anchor);
  NetworkInfoBubble(const NetworkInfoBubble&) = delete;
  NetworkInfoBubble& operator=(const NetworkInfoBubble&) = delete;
  ~NetworkInfoBubble() override;

 private:
  friend class NetworkInfoBubbleTest;

  // Used for testing. This is 1 because view IDs should not be 0.
  static constexpr int kNetworkInfoBubbleLabelViewId = 1;

  // views::View:
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;
  void OnMouseExited(const ui::MouseEvent& event) override;

  // views::OnBeforeBubbleWidgetInit:
  void OnBeforeBubbleWidgetInit(views::Widget::InitParams* params,
                                views::Widget* widget) const override;

  // Computes the text to be shown in the info bubble. The text will be
  // comprised of the IP addresses, if available, as well as the mac addresses
  // for the ethernet, WiFi, and cellular devices.
  std::u16string ComputeInfoText();

  // The container for info labels.
  raw_ptr<views::View> label_container_ = nullptr;

  base::WeakPtr<Delegate> delegate_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_NETWORK_NETWORK_INFO_BUBBLE_H_
