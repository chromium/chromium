// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/model/assistant_ui_element.h"

#include "ash/assistant/ui/assistant_ui_constants.h"
#include "base/base64.h"

namespace ash {

namespace {

// Navigable contents.
constexpr char kDataUriPrefix[] = "data:text/html;base64,";

}  // namespace

// AssistantCardElement --------------------------------------------------------

AssistantCardElement::AssistantCardElement(const std::string& html,
                                           const std::string& fallback)
    : AssistantUiElement(AssistantUiElementType::kCard),
      html_(html),
      fallback_(fallback) {}

AssistantCardElement::~AssistantCardElement() = default;

void AssistantCardElement::Process(
    content::mojom::NavigableContentsFactory* contents_factory,
    ProcessingCallback callback) {
  processor_ =
      std::make_unique<Processor>(*this, contents_factory, std::move(callback));
  processor_->Process();
}

// AssistantCardElement::Processor ---------------------------------------------

AssistantCardElement::Processor::Processor(
    AssistantCardElement& card_element,
    content::mojom::NavigableContentsFactory* contents_factory,
    ProcessingCallback callback)
    : card_element_(card_element),
      contents_factory_(contents_factory),
      callback_(std::move(callback)) {}

AssistantCardElement::Processor::~Processor() {
  if (contents_)
    contents_->RemoveObserver(this);

  if (callback_)
    std::move(callback_).Run(/*success=*/false);
}

void AssistantCardElement::Processor::Process() {
  // TODO(dmblack): Find a better way of determining desired card size.
  const int width_dip = kPreferredWidthDip - 2 * kUiElementHorizontalMarginDip;

  // Configure parameters for the card.
  auto contents_params = content::mojom::NavigableContentsParams::New();
  contents_params->enable_view_auto_resize = true;
  contents_params->auto_resize_min_size = gfx::Size(width_dip, 1);
  contents_params->auto_resize_max_size = gfx::Size(width_dip, INT_MAX);
  contents_params->suppress_navigations = true;

  contents_ = std::make_unique<content::NavigableContents>(
      contents_factory_, std::move(contents_params));

  // Observe |contents_| so that we are notified when loading is complete.
  contents_->AddObserver(this);

  // Navigate to the data URL which represents the card.
  std::string encoded_html;
  base::Base64Encode(card_element_.html(), &encoded_html);
  contents_->Navigate(GURL(kDataUriPrefix + encoded_html));
}

void AssistantCardElement::Processor::DidStopLoading() {
  contents_->RemoveObserver(this);

  // Pass ownership of |contents_| to the card element that was being processed
  // and notify our |callback_| of success.
  card_element_.set_contents(std::move(contents_));
  std::move(callback_).Run(/*success=*/true);
}

}  // namespace ash
