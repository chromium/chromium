// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_DISPLAY_OUTPUT_PROTECTION_DELEGATE_H_
#define ASH_DISPLAY_OUTPUT_PROTECTION_DELEGATE_H_

#include <stdint.h>

#include <memory>

#include "ash/ash_export.h"
#include "base/macros.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"
#include "ui/display/display_observer.h"
#include "ui/display/types/display_constants.h"

namespace ash {

// Proxies output protection requests for an associated window, and renews them
// when the window is reparented to another display.
class ASH_EXPORT OutputProtectionDelegate : public aura::WindowObserver,
                                            public display::DisplayObserver {
 public:
  using QueryStatusCallback = base::OnceCallback<
      void(bool success, uint32_t connection_mask, uint32_t protection_mask)>;
  using SetProtectionCallback = base::OnceCallback<void(bool success)>;

  explicit OutputProtectionDelegate(aura::Window* window);
  ~OutputProtectionDelegate() override;

  void QueryStatus(QueryStatusCallback callback);
  void SetProtection(uint32_t protection_mask, SetProtectionCallback callback);

 private:
  // display::DisplayObserver:
  void OnDisplayMetricsChanged(const display::Display& display,
                               uint32_t changed_metrics) override;

  // aura::WindowObserver:
  void OnWindowHierarchyChanged(
      const aura::WindowObserver::HierarchyChangeParams& params) override;
  void OnWindowDestroying(aura::Window* window) override;

  void OnWindowMayHaveMovedToAnotherDisplay();

  bool RegisterClientIfNecessary();

  // Native window being observed.
  aura::Window* window_ = nullptr;

  // Display ID of the observed window.
  int64_t display_id_;

  // Last requested ContentProtectionMethod bitmask, applied when the observed
  // window moves to another display.
  uint32_t protection_mask_ = display::CONTENT_PROTECTION_METHOD_NONE;

  // RAII wrapper to register/unregister ContentProtectionManager client.
  struct ClientIdHolder;
  std::unique_ptr<ClientIdHolder> client_;

  DISALLOW_COPY_AND_ASSIGN(OutputProtectionDelegate);
};

}  // namespace ash

#endif  // ASH_DISPLAY_OUTPUT_PROTECTION_DELEGATE_H_
