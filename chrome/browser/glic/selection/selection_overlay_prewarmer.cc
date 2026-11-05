// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/selection/selection_overlay_prewarmer.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/time/time.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "ui/color/color_provider_source.h"

namespace glic {

namespace {

// Long enough to cover a hover that ends in a click, short enough not to keep
// an unused renderer around.
constexpr base::TimeDelta kPrewarmTimeout = base::Seconds(10);

}  // namespace

SelectionOverlayPrewarmer::SelectionOverlayPrewarmer() = default;

SelectionOverlayPrewarmer::~SelectionOverlayPrewarmer() = default;

void SelectionOverlayPrewarmer::Start(
    std::unique_ptr<content::WebContents> contents,
    ui::ColorProviderSource& color_source,
    selection::InteractionOptionsPtr interaction_options) {
  contents_ = std::move(contents);
  Observe(contents_.get());
  contents_->SetColorProviderSource(&color_source);
  interaction_options_ = std::move(interaction_options);
  Extend();
}

void SelectionOverlayPrewarmer::Extend() {
  // Unretained is safe: `timeout_` is owned by this.
  timeout_.Start(FROM_HERE, kPrewarmTimeout,
                 base::BindOnce(&SelectionOverlayPrewarmer::Cancel,
                                base::Unretained(this)));
}

void SelectionOverlayPrewarmer::Cancel() {
  timeout_.Stop();
  interaction_options_.reset();
  Observe(nullptr);
  contents_.reset();
}

bool SelectionOverlayPrewarmer::IsUsable(
    const selection::InteractionOptionsPtr& interaction_options) const {
  // A page that is still loading is fine: its handler request is held until
  // the session binds it.
  return contents_ && interaction_options_ == interaction_options &&
         !contents_->GetPrimaryMainFrame()->IsErrorDocument();
}

bool SelectionOverlayPrewarmer::Contains(
    const content::WebContents* contents) const {
  return contents_ && contents_.get() == contents;
}

selection::InteractionOptionsPtr
SelectionOverlayPrewarmer::GetInteractionOptions() const {
  return interaction_options_.Clone();
}

std::unique_ptr<content::WebContents> SelectionOverlayPrewarmer::Take() {
  timeout_.Stop();
  interaction_options_.reset();
  Observe(nullptr);
  return std::move(contents_);
}

void SelectionOverlayPrewarmer::PrimaryMainFrameRenderProcessGone(
    base::TerminationStatus status) {
  // WebContents allows its deletion from this observer method.
  Cancel();
}

}  // namespace glic
