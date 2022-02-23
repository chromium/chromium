// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_STYLE_SYSTEM_SHADOW_H_
#define ASH_STYLE_SYSTEM_SHADOW_H_

#include "ash/ash_export.h"
#include "ui/compositor_extra/shadow.h"

namespace views {
class Widget;
}  // namespace views

namespace aura {
class Window;
}  // namespace aura

namespace ash {

// Shadow for Chrome OS System UI component.
class ASH_EXPORT SystemShadow : public ui::Shadow {
 public:
  // Shadow types of system UI components. The shadows with different elevations
  // have different appearance.
  enum class Type {
    kElevation4,
    kElevation8,
    kElevation12,
    kElevation16,
    kElevation24,
  };

  explicit SystemShadow(Type type);
  SystemShadow(const SystemShadow&) = delete;
  SystemShadow& operator=(const SystemShadow&) = delete;
  ~SystemShadow() override;

  static std::unique_ptr<SystemShadow> CreateShadowForWidget(
      views::Widget* widget,
      Type shadow_type);
  static std::unique_ptr<SystemShadow> CreateShadowForWindow(
      aura::Window* window,
      Type shadow_type);
  // Get shadow elevation according to the given type.
  static int GetElevationFromType(Type type);

  // Change shadow type and update shadow elevation and appearance. Note that to
  // avoid inconsistency of shadow type and elevation. Always change system
  // shadow elevation with `SetType` instead of `SetElevation`.
  void SetType(Type type);
  Type type() const { return type_; }

 private:
  Type type_ = Type::kElevation4;
};

}  // namespace ash

#endif  // ASH_STYLE_SYSTEM_SHADOW_H_
