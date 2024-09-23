// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_SYSTEM_TOAST_MANAGER_H_
#define ASH_PUBLIC_CPP_SYSTEM_TOAST_MANAGER_H_

#include <memory>
#include <string>

#include "ash/public/cpp/ash_public_export.h"

namespace ash {

struct ToastData;
class ScopedToastPause;

// Public interface to show toasts.
class ASH_PUBLIC_EXPORT ToastManager {
 public:
  static ToastManager* Get();

  // Show a toast. If there are queued toasts, succeeding toasts are queued as
  // well, and are shown one by one.
  virtual void Show(ToastData data) = 0;

  // Cancels a toast with the provided ID.
  virtual void Cancel(std::string_view id) = 0;

  // Requests focus on the active toast's dismiss button. Returns false if
  // dismiss button did not take focus.
  virtual bool RequestFocusOnActiveToastDismissButton(std::string_view id) = 0;

  // Tells if a toast with the provided ID is currently being shown.
  virtual bool IsToastShown(std::string_view id) const = 0;

  // Tells if the toast with the provided ID has a dismiss button that is
  // currently being focsued. Returns false if the toast is not shown,
  // does not have a dismiss button, or the dismiss button is not focused.
  virtual bool IsToastDismissButtonFocused(std::string_view id) const = 0;

  // Creates a `ScopedToastPause`.
  virtual std::unique_ptr<ScopedToastPause> CreateScopedPause() = 0;

 protected:
  ToastManager();
  virtual ~ToastManager();

 private:
  friend class ScopedToastPause;

  // `Pause()` will stop all the toasts from showing up, until `Resume()` is
  // called.
  virtual void Pause() = 0;
  virtual void Resume() = 0;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_SYSTEM_TOAST_MANAGER_H_
