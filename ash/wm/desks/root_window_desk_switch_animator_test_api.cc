// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/desks/root_window_desk_switch_animator_test_api.h"

#include <memory>

#include "ash/wm/desks/root_window_desk_switch_animator.h"
#include "base/strings/string_number_conversions.h"
#include "components/viz/common/frame_sinks/copy_output_result.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "ui/aura/window.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_tree_owner.h"
#include "ui/gfx/color_space.h"

namespace ash {

namespace {

int g_mailbox_id = 0;

// Creates a blank copy output result the size of |root_window|.
std::unique_ptr<viz::CopyOutputResult> CreateCopyOutputResult(
    aura::Window* root_window) {
  std::string mailbox_name =
      "mailboxname" + base::NumberToString(g_mailbox_id++);
  gpu::Mailbox mailbox;
  mailbox.SetName(reinterpret_cast<const int8_t*>(mailbox_name.c_str()));

  std::unique_ptr<viz::CopyOutputResult> copy_result =
      std::make_unique<viz::CopyOutputTextureResult>(
          root_window->bounds(), mailbox, gpu::SyncToken(),
          gfx::ColorSpace::CreateSRGB(),
          viz::SingleReleaseCallback::Create(base::DoNothing()));

  DCHECK(!copy_result->IsEmpty());
  return copy_result;
}

}  // namespace

RootWindowDeskSwitchAnimatorTestApi::RootWindowDeskSwitchAnimatorTestApi(
    RootWindowDeskSwitchAnimator* animator)
    : animator_(animator) {
  DCHECK(animator_);
}

RootWindowDeskSwitchAnimatorTestApi::~RootWindowDeskSwitchAnimatorTestApi() =
    default;

void RootWindowDeskSwitchAnimatorTestApi::OnStartingDeskScreenshotTaken() {
  animator_->OnStartingDeskScreenshotTaken(
      CreateCopyOutputResult(animator_->root_window_));
}

void RootWindowDeskSwitchAnimatorTestApi::OnEndingDeskScreenshotTaken() {
  animator_->OnEndingDeskScreenshotTaken(
      CreateCopyOutputResult(animator_->root_window_));
}

ui::Layer* RootWindowDeskSwitchAnimatorTestApi::GetAnimationLayer() {
  return animator_->animation_layer_owner_->root();
}

ui::Layer*
RootWindowDeskSwitchAnimatorTestApi::GetScreenshotLayerOfDeskWithIndex(
    int desk_index) {
  auto screenshot_layers = animator_->screenshot_layers_;

  DCHECK_GE(desk_index, 0);
  DCHECK_LT(desk_index, int{screenshot_layers.size()});

  ui::Layer* layer = screenshot_layers[desk_index];
  DCHECK(layer);
  return layer;
}

int RootWindowDeskSwitchAnimatorTestApi::GetEndingDeskIndex() const {
  return animator_->ending_desk_index_;
}

}  // namespace ash
