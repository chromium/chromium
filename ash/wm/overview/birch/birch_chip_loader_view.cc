// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/overview/birch/birch_chip_loader_view.h"

#include "base/notreached.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/layer.h"
#include "ui/views/accessibility/view_accessibility.h"

namespace ash {

namespace {

constexpr float kSemiTransparent = 0.3f;

// Animation durations of the loader at 6 init stages.
constexpr base::TimeDelta kInitFadeInDuration1 = base::Milliseconds(500);
constexpr base::TimeDelta kInitFadeInDuration2 = base::Milliseconds(300);
constexpr base::TimeDelta kInitFadeInDuration3_6 = base::Milliseconds(400);

// Animation durations of the loader at 5 re-loading stages.
constexpr base::TimeDelta kReloadFadeInDuration1_5 = base::Milliseconds(400);

}  // namespace

BirchChipLoaderView::BirchChipLoaderView() {
  GetViewAccessibility().SetName(u"Birch Chip Loader View");
}

BirchChipLoaderView::~BirchChipLoaderView() = default;

void BirchChipLoaderView::SetDelay(const base::TimeDelta& delay) {
  delay_ = delay;
}

void BirchChipLoaderView::SetType(Type type) {
  type_ = type;
}

void BirchChipLoaderView::AddAnimationToBuilder(
    views::AnimationBuilder& builder) {
  ui::Layer* loader_layer = layer();
  CHECK(loader_layer);

  switch (type_) {
    case Type::kInit:
      loader_layer->SetOpacity(0.0f);
      builder.Once()
          .At(delay_)
          // Stage 1: Up to 100%.
          .SetDuration(kInitFadeInDuration1)
          .SetOpacity(loader_layer, 1.0f)
          // Stage 2: Down from 100% to 30%.
          .Then()
          .SetDuration(kInitFadeInDuration2)
          .SetOpacity(loader_layer, kSemiTransparent)
          // Stage 3: Hold 30%.
          .Offset(kInitFadeInDuration3_6 + kInitFadeInDuration2)
          // Stage 4: Up from 30% to 100%.
          .SetDuration(kInitFadeInDuration3_6)
          .SetOpacity(loader_layer, 1.0f)
          // Stage 5: Hold 100%.
          .Offset(kInitFadeInDuration3_6 * 2)
          // Stage 6: Down from 100% to 0%.
          .SetDuration(kInitFadeInDuration3_6)
          .SetOpacity(loader_layer, 0.0f);
      return;
    case Type::kReload:
      loader_layer->SetOpacity(1.0f);
      builder.Once()
          .At(delay_)
          // Stage 1: Down from 100% to 30%
          .SetDuration(kReloadFadeInDuration1_5)
          .SetOpacity(loader_layer, kSemiTransparent)
          // Stage 2: Hold 30%.
          .Offset(kReloadFadeInDuration1_5 * 2)
          // Stage 3: Up from 30% to 100%.
          .SetDuration(kReloadFadeInDuration1_5)
          .SetOpacity(loader_layer, 1.0f)
          // Stage 4: Down from 100% to 30%.
          .Then()
          .SetDuration(kReloadFadeInDuration1_5)
          .SetOpacity(loader_layer, kSemiTransparent)
          // Stage 5: Down from 100% to 0%.
          .Then()
          .SetDuration(kReloadFadeInDuration1_5)
          .SetOpacity(loader_layer, 0.0f);
      return;
    case Type::kNone:
      NOTREACHED() << "Please set a loading type for birch bar loader";
  }
}

void BirchChipLoaderView::Init(BirchItem* item) {}

const BirchItem* BirchChipLoaderView::GetItem() const {
  return nullptr;
}

BirchItem* BirchChipLoaderView::GetItem() {
  return nullptr;
}

void BirchChipLoaderView::Shutdown() {}

BEGIN_METADATA(BirchChipLoaderView)
END_METADATA

}  // namespace ash
