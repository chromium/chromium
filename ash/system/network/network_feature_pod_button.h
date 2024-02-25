// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_NETWORK_NETWORK_FEATURE_POD_BUTTON_H_
#define ASH_SYSTEM_NETWORK_NETWORK_FEATURE_POD_BUTTON_H_

#include "ash/ash_export.h"
#include "ash/system/unified/feature_pod_button.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"

namespace ash {

// This class that provides the corresponding controller with notifications of
// when the theme for this view changes.
class ASH_EXPORT NetworkFeaturePodButton : public FeaturePodButton {
  METADATA_HEADER(NetworkFeaturePodButton, FeaturePodButton)

 public:
  // This class defines the interface that NetworkFeaturePodButton will use to
  // propagate theme changes.
  class Delegate {
   public:
    virtual void OnFeaturePodButtonThemeChanged() = 0;
  };

  NetworkFeaturePodButton(FeaturePodControllerBase* controller,
                          Delegate* delegate);
  NetworkFeaturePodButton(const NetworkFeaturePodButton&) = delete;
  NetworkFeaturePodButton& operator=(const NetworkFeaturePodButton&) = delete;
  ~NetworkFeaturePodButton() override;

 private:
  friend class NetworkFeaturePodButtonTest;

  // views::Button:
  void OnThemeChanged() override;

  raw_ptr<Delegate, DanglingUntriaged> delegate_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_NETWORK_NETWORK_FEATURE_POD_BUTTON_H_
