// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/window_backdrop.h"

#include "ash/public/cpp/window_properties.h"
#include "ui/aura/window.h"

namespace ash {

namespace {

constexpr SkColor kSemiOpaqueBackdropColor =
    SkColorSetARGB(0x99, 0x20, 0x21, 0x24);

}  // namespace

WindowBackdrop::WindowBackdrop(aura::Window* window) : window_(window) {}
WindowBackdrop::~WindowBackdrop() = default;

// static
WindowBackdrop* WindowBackdrop::Get(aura::Window* window) {
  DCHECK(window);

  WindowBackdrop* window_backdrop = window->GetProperty(kWindowBackdropKey);
  if (window_backdrop)
    return window_backdrop;

  window_backdrop = new WindowBackdrop(window);
  window->SetProperty(kWindowBackdropKey, window_backdrop);
  return window_backdrop;
}

void WindowBackdrop::SetBackdropMode(WindowBackdrop::BackdropMode mode) {
  if (mode_ == mode)
    return;

  mode_ = mode;
  NotifyWindowBackdropPropertyChanged();
}

void WindowBackdrop::SetBackdropType(WindowBackdrop::BackdropType type) {
  if (type_ == type)
    return;

  type_ = type;
  NotifyWindowBackdropPropertyChanged();
}

void WindowBackdrop::DisableBackdrop() {
  if (temporarily_disabled_)
    return;

  temporarily_disabled_ = true;
  NotifyWindowBackdropPropertyChanged();
}

void WindowBackdrop::RestoreBackdrop() {
  if (!temporarily_disabled_)
    return;

  temporarily_disabled_ = false;
  NotifyWindowBackdropPropertyChanged();
}

SkColor WindowBackdrop::GetBackdropColor() const {
  if (type_ == BackdropType::kSemiOpaque)
    return kSemiOpaqueBackdropColor;

  DCHECK(type_ == BackdropType::kOpaque);
  return SK_ColorBLACK;
}

void WindowBackdrop::AddObserver(WindowBackdrop::Observer* observer) {
  observers_.AddObserver(observer);
}

void WindowBackdrop::RemoveObserver(WindowBackdrop::Observer* observer) {
  observers_.RemoveObserver(observer);
}

void WindowBackdrop::NotifyWindowBackdropPropertyChanged() {
  for (auto& observer : observers_)
    observer.OnWindowBackdropPropertyChanged(window_);
}

}  // namespace ash
