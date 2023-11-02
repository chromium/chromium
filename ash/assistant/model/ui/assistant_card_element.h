// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_MODEL_UI_ASSISTANT_CARD_ELEMENT_H_
#define ASH_ASSISTANT_MODEL_UI_ASSISTANT_CARD_ELEMENT_H_

#include <memory>
#include <string>
#include <utility>

#include "ash/assistant/model/ui/assistant_ui_element.h"
#include "ash/public/cpp/ash_web_view.h"
#include "base/component_export.h"

namespace ash {

// An Assistant UI element that will be rendered as an HTML card.
class COMPONENT_EXPORT(ASSISTANT_MODEL) AssistantCardElement
    : public AssistantUiElement {
 public:
  AssistantCardElement(const std::string& html,
                       const std::string& fallback,
                       int viewport_width);

  AssistantCardElement(const AssistantCardElement&) = delete;
  AssistantCardElement& operator=(const AssistantCardElement&) = delete;

  ~AssistantCardElement() override;

  // AssistantUiElement:
  void Process(ProcessingCallback callback) override;

  bool has_contents_view() const;
  const std::string& html() const { return html_; }
  const std::string& fallback() const { return fallback_; }
  int viewport_width() const { return viewport_width_; }
  std::unique_ptr<AshWebView> MoveContentsView() {
    return std::move(contents_view_);
  }

  void set_contents_view(std::unique_ptr<AshWebView> contents_view) {
    contents_view_ = std::move(contents_view);
  }

 private:
  class Processor;

  const std::string html_;
  const std::string fallback_;
  const int viewport_width_;
  std::unique_ptr<AshWebView> contents_view_;

  std::unique_ptr<Processor> processor_;

  // AssistantUiElement:
  bool Compare(const AssistantUiElement& other) const override;
};

}  // namespace ash

#endif  // ASH_ASSISTANT_MODEL_UI_ASSISTANT_CARD_ELEMENT_H_
