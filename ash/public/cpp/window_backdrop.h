// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_WINDOW_BACKDROP_H_
#define ASH_PUBLIC_CPP_WINDOW_BACKDROP_H_

#include "ash/public/cpp/ash_public_export.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "third_party/skia/include/core/SkColor.h"

namespace aura {
class Window;
}

namespace ash {

// The window backdrop property that associates with a window. It's owned by
// a window through its kWindowBackdropKey property. It's not supposed to
// manually clear the kWindowBackdropKey property, as WindowBackdrop::Get()
// will create a new one as soon as it's called. If you want to modify the
// window's backdrop, please do so by modifying its mode/type value to achieve
// that.
class ASH_PUBLIC_EXPORT WindowBackdrop {
 public:
  enum class BackdropMode {
    kAuto,  // The window manager decides if the window should have a backdrop.
    kEnabled,   // The window should always have a backdrop.
    kDisabled,  // The window should never have a backdrop.
  };

  enum class BackdropType {
    kOpaque,      // The backdrop is fully-opaque black
    kSemiOpaque,  // The backdrop is semi-opaque black
  };

  class Observer : public base::CheckedObserver {
   public:
    virtual void OnWindowBackdropPropertyChanged(aura::Window* window) {}
  };

  explicit WindowBackdrop(aura::Window* window);
  ~WindowBackdrop();

  // Returns the WindowBackdrop for |window|. The returned value is owned by
  // |window|.
  static WindowBackdrop* Get(aura::Window* window);

  BackdropMode mode() const { return mode_; }
  BackdropType type() const { return type_; }
  bool temporarily_disabled() const { return temporarily_disabled_; }

  void SetBackdropMode(BackdropMode mode);
  void SetBackdropType(BackdropType type);

  // Disable the backdrop on the window. However, the backdrop mode and type can
  // still be modified even when backdrop is disabled but will have no effect on
  // the backdrop. After backdrop is re-enabled, the mode and type will take
  // effect again.
  void DisableBackdrop();
  void RestoreBackdrop();

  // Returns the backdrop color according to its type.
  SkColor GetBackdropColor() const;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 private:
  WindowBackdrop(const WindowBackdrop&) = delete;
  WindowBackdrop& operator=(const WindowBackdrop&) = delete;

  void NotifyWindowBackdropPropertyChanged();

  // The window that this WindowBackdrop associates with. Will be valid during
  // this WindowBackdrop's lifetime.
  raw_ptr<aura::Window> window_;

  BackdropMode mode_ = BackdropMode::kAuto;
  BackdropType type_ = BackdropType::kOpaque;

  // When this variable is true, the window backdrop is temporarily disabled
  // and changing its backdrop setting above (mode/type) will not have any
  // effect. Only when this variable is reset back to false, the window can have
  // its customized backdrop setting.
  // This can be useful when we need to disable window backdrop temporarily
  // during e.g. window dragging or window animation.
  bool temporarily_disabled_ = false;

  base::ObserverList<Observer> observers_;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_WINDOW_BACKDROP_H_
