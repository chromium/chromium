// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_COMMON_BACKEND_ACCESSIBILITY_FEATURES_H_
#define ASH_WEBUI_COMMON_BACKEND_ACCESSIBILITY_FEATURES_H_

#include "ash/accessibility/accessibility_observer.h"
#include "ash/webui/common/mojom/accessibility_features.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote_set.h"

namespace ash {

class AccessibilityFeatures : public common::mojom::AccessibilityFeatures,
                              public AccessibilityObserver {
 public:
  AccessibilityFeatures();

  AccessibilityFeatures(const AccessibilityFeatures&) = delete;
  AccessibilityFeatures& operator=(const AccessibilityFeatures&) = delete;

  ~AccessibilityFeatures() override;

  // common::mojom::AccessibilityFeatures:
  void ObserveForceHiddenElementsVisible(
      mojo::PendingRemote<common::mojom::ForceHiddenElementsVisibleObserver>
          observer,
      ObserveForceHiddenElementsVisibleCallback callback) override;

  // AccessibilityObserver:
  void OnAccessibilityStatusChanged() override;

  // Binds receiver_ by consuming |pending_receiver|.
  void BindInterface(mojo::PendingReceiver<common::mojom::AccessibilityFeatures>
                         pending_receiver);

 private:
  // Receives and dispatches method calls to this implementation of the
  // ash::common::mojom::AccessibilityFeatures interface.
  mojo::Receiver<common::mojom::AccessibilityFeatures> receiver_{this};

  // Used to send updates to the state of the 'Force Hidden Elements Visible'
  // group of accessbility features.
  mojo::RemoteSet<common::mojom::ForceHiddenElementsVisibleObserver>
      force_hidden_elements_visible_observers_;

  // Tracks the current state of the 'Force Hidden Elements Visible' group of
  // accessbility features.
  bool force_hidden_elements_visible_ = false;
};

}  // namespace ash

#endif  // ASH_WEBUI_COMMON_BACKEND_ACCESSIBILITY_FEATURES_H_
