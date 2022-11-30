// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/providers/wired_display/wired_display_presentation_receiver_factory.h"

#include <utility>

#include "chrome/browser/media/router/media_router_feature.h"
#include "chrome/browser/ui/media_router/presentation_receiver_window_controller.h"

namespace media_router {

namespace {

base::LazyInstance<WiredDisplayPresentationReceiverFactory>::Leaky factory =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

// static
std::unique_ptr<WiredDisplayPresentationReceiver>
WiredDisplayPresentationReceiverFactory::Create(
    Profile* profile,
    const gfx::Rect& bounds,
    base::OnceClosure termination_callback,
    base::RepeatingCallback<void(const std::string&)> title_change_callback) {
  if (GetInstance()->create_receiver_for_testing_) {
    return GetInstance()->create_receiver_for_testing_.Run(
        profile, bounds, std::move(termination_callback),
        std::move(title_change_callback));
  }
  return PresentationReceiverWindowController::CreateFromOriginalProfile(
      profile, bounds, std::move(termination_callback),
      std::move(title_change_callback));
}

// static
void WiredDisplayPresentationReceiverFactory::SetCreateReceiverCallbackForTest(
    WiredDisplayPresentationReceiverFactory::CreateReceiverCallback callback) {
  GetInstance()->create_receiver_for_testing_ = std::move(callback);
}

WiredDisplayPresentationReceiverFactory::
    WiredDisplayPresentationReceiverFactory() = default;

WiredDisplayPresentationReceiverFactory::
    ~WiredDisplayPresentationReceiverFactory() = default;

// static
WiredDisplayPresentationReceiverFactory*
WiredDisplayPresentationReceiverFactory::GetInstance() {
  return &factory.Get();
}

}  // namespace media_router
