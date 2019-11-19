// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_TOAST_TOAST_MANAGER_IMPL_H_
#define ASH_SYSTEM_TOAST_TOAST_MANAGER_IMPL_H_

#include <memory>
#include <string>

#include "ash/ash_export.h"
#include "ash/public/cpp/toast_data.h"
#include "ash/public/cpp/toast_manager.h"
#include "ash/session/session_observer.h"
#include "ash/system/toast/toast_overlay.h"
#include "base/containers/circular_deque.h"
#include "base/memory/weak_ptr.h"

namespace ash {

// Class managing toast requests.
class ASH_EXPORT ToastManagerImpl : public ToastManager,
                                    public ToastOverlay::Delegate,
                                    public SessionObserver {
 public:
  ToastManagerImpl();
  ~ToastManagerImpl() override;

  // ToastManager overrides:
  void Show(const ToastData& data) override;

  void Cancel(const std::string& id);

  // ToastOverlay::Delegate overrides:
  void OnClosed() override;

  // SessionObserver:
  void OnSessionStateChanged(session_manager::SessionState state) override;

 private:
  friend class ToastManagerImplTest;

  void ShowLatest();
  void OnDurationPassed(int toast_number);

  ToastOverlay* GetCurrentOverlayForTesting() { return overlay_.get(); }
  int serial_for_testing() const { return serial_; }
  void ResetSerialForTesting() { serial_ = 0; }

  // Data of the toast which is currently shown. Empty if no toast is visible.
  base::Optional<ToastData> current_toast_data_;

  int serial_ = 0;
  bool locked_;
  base::circular_deque<ToastData> queue_;
  std::unique_ptr<ToastOverlay> overlay_;

  ScopedSessionObserver scoped_session_observer_{this};
  base::WeakPtrFactory<ToastManagerImpl> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ToastManagerImpl);
};

}  // namespace ash

#endif  // ASH_SYSTEM_TOAST_TOAST_MANAGER_IMPL_H_
