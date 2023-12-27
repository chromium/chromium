// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/model/ui/assistant_card_element.h"

#include <utility>

#include "ash/assistant/ui/assistant_ui_constants.h"
#include "ash/assistant/ui/assistant_view_ids.h"
#include "ash/public/cpp/ash_web_view.h"
#include "ash/public/cpp/ash_web_view_factory.h"
#include "base/base64.h"
#include "base/memory/raw_ptr.h"

namespace ash {

// AssistantCardElement::Processor ---------------------------------------------

class AssistantCardElement::Processor : public AshWebView::Observer {
 public:
  Processor(AssistantCardElement* card_element, ProcessingCallback callback)
      : card_element_(card_element), callback_(std::move(callback)) {}

  Processor(const Processor& copy) = delete;
  Processor& operator=(const Processor& assign) = delete;

  ~Processor() override {
    if (contents_view_)
      contents_view_->RemoveObserver(this);

    if (callback_)
      std::move(callback_).Run();
  }

  void Process() {
    const int width_dip =
        card_element_->viewport_width() - 2 * assistant::ui::kHorizontalMargin;

    // Configure parameters for the card. We want to configure the size as:
    // - width: It should be width_dip.
    // - height: It should be calculated from the content.
    AshWebView::InitParams contents_params;
    contents_params.enable_auto_resize = true;
    contents_params.min_size = gfx::Size(width_dip, 1);
    contents_params.max_size = gfx::Size(width_dip, INT_MAX);
    contents_params.suppress_navigation = true;
    contents_params.fix_zoom_level_to_one = true;

    // Create |contents_view_| and retain ownership until it is added to the
    // view hierarchy. If that never happens, it will be still be cleaned up.
    contents_view_ = AshWebViewFactory::Get()->Create(contents_params);
    contents_view_->SetID(AssistantViewID::kAshWebView);

    // Observe |contents_view_| so that we are notified when loading is
    // complete.
    contents_view_->AddObserver(this);

    // Encode the html string to be URL-safe.
    std::string encoded_html = base::Base64Encode(card_element_->html());

    // Navigate to the data URL which represents the card.
    constexpr char kDataUriPrefix[] = "data:text/html;base64,";
    contents_view_->Navigate(GURL(kDataUriPrefix + encoded_html));
  }

 private:
  // AshWebView::Observer:
  void DidStopLoading() override {
    contents_view_->RemoveObserver(this);

    // Pass ownership of |contents_view_| to the card element that was being
    // processed and notify our |callback_| of the completion.
    card_element_->set_contents_view(std::move(contents_view_));
    std::move(callback_).Run();
  }

  // |card_element_| should outlive the Processor.
  const raw_ptr<AssistantCardElement> card_element_;
  ProcessingCallback callback_;

  std::unique_ptr<AshWebView> contents_view_;
};

// AssistantCardElement --------------------------------------------------------

AssistantCardElement::AssistantCardElement(const std::string& html,
                                           const std::string& fallback,
                                           int viewport_width)
    : AssistantUiElement(AssistantUiElementType::kCard),
      html_(html),
      fallback_(fallback),
      viewport_width_(viewport_width) {}

AssistantCardElement::~AssistantCardElement() {
  // |processor_| should be destroyed before |this| has been deleted.
  processor_.reset();
}

void AssistantCardElement::Process(ProcessingCallback callback) {
  processor_ = std::make_unique<Processor>(this, std::move(callback));
  processor_->Process();
}

bool AssistantCardElement::has_contents_view() const {
  return !!contents_view_;
}

bool AssistantCardElement::Compare(const AssistantUiElement& other) const {
  return other.type() == AssistantUiElementType::kCard &&
         static_cast<const AssistantCardElement&>(other).html() == html_;
}

}  // namespace ash
