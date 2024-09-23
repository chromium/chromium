// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELF_LOGIN_SHELF_BUTTON_H_
#define ASH_SHELF_LOGIN_SHELF_BUTTON_H_

#include <string>

#include "ash/ash_export.h"
#include "ash/public/cpp/shelf_types.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_observer.h"
#include "ash/style/pill_button.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ref.h"
#include "base/scoped_observation.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/vector_icon_types.h"

namespace ash {

class ASH_EXPORT LoginShelfButton : public PillButton, public ShelfObserver {
  METADATA_HEADER(LoginShelfButton, PillButton)

 public:
  LoginShelfButton(PressedCallback callback,
                   int text_resource_id,
                   const gfx::VectorIcon& icon);

  LoginShelfButton(const LoginShelfButton&) = delete;
  LoginShelfButton& operator=(const LoginShelfButton&) = delete;

  ~LoginShelfButton() override;

  int text_resource_id() const;

  // PillButton:
  std::u16string GetTooltipText(const gfx::Point& p) const override;
  void OnFocus() override;
  void AddedToWidget() override;

  // ShelfObserver:
  void OnBackgroundTypeChanged(ShelfBackgroundType background_type,
                               AnimationChangeType change_type) override;

  void OnActiveChanged();

  void SetIsActive(bool is_active);
  bool GetIsActive() const;

 private:
  base::ScopedObservation<Shelf, ShelfObserver> shelf_observer_{this};

  const raw_ref<const gfx::VectorIcon> icon_;
  const int text_resource_id_;

  ShelfBackgroundType background_type_ = ShelfBackgroundType::kDefaultBg;
  bool is_active_ = false;
};

}  // namespace ash

#endif  // ASH_SHELF_LOGIN_SHELF_BUTTON_H_
