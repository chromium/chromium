// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_OVERVIEW_BIRCH_BIRCH_CHIP_LOADER_VIEW_H_
#define ASH_WM_OVERVIEW_BIRCH_BIRCH_CHIP_LOADER_VIEW_H_

#include "ash/wm/overview/birch/birch_chip_button_base.h"
#include "base/time/time.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/animation/animation_builder.h"
#include "ui/views/metadata/view_factory.h"

namespace ash {

// When entering an informed restore session and customizing suggestion types to
// show, the birch bar needs to fetch data from the model. While waiting for the
// data, the loader views will show on the birch bar with corresponding fading
// in and out animations.
class BirchChipLoaderView : public BirchChipButtonBase {
  METADATA_HEADER(BirchChipLoaderView, BirchChipButtonBase)

 public:
  enum class Type {
    kInit,    // Used during loading for informed restore.
    kReload,  // Used when suggestions are customized by user.
    kNone,
  };

  BirchChipLoaderView();
  BirchChipLoaderView(const BirchChipLoaderView&) = delete;
  BirchChipLoaderView& operator=(const BirchChipLoaderView&) = delete;
  ~BirchChipLoaderView() override;

  // Sets the delay for loading animation.
  void SetDelay(const base::TimeDelta& delay);

  // Sets the loading type.
  void SetType(Type type);

  // Adds the loading animation to given animation builder.
  void AddAnimationToBuilder(views::AnimationBuilder& builder);

  // BirchChipButtonBase:
  void Init(BirchItem* item) override;
  const BirchItem* GetItem() const override;
  BirchItem* GetItem() override;
  void Shutdown() override;

 private:
  base::TimeDelta delay_;
  Type type_ = Type::kNone;
};

BEGIN_VIEW_BUILDER(/*no export*/, BirchChipLoaderView, BirchChipButtonBase)
VIEW_BUILDER_PROPERTY(const base::TimeDelta&, Delay)
VIEW_BUILDER_PROPERTY(BirchChipLoaderView::Type, Type)
END_VIEW_BUILDER

}  // namespace ash

DEFINE_VIEW_BUILDER(/*no export*/, ash::BirchChipLoaderView)

#endif  // ASH_WM_OVERVIEW_BIRCH_BIRCH_CHIP_LOADER_VIEW_H_
