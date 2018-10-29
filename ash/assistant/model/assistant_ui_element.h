// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_MODEL_ASSISTANT_UI_ELEMENT_H_
#define ASH_ASSISTANT_MODEL_ASSISTANT_UI_ELEMENT_H_

#include <string>

#include "base/macros.h"
#include "base/optional.h"
#include "base/unguessable_token.h"

namespace ash {

// AssistantUiElementType ------------------------------------------------------

// Defines possible types of Assistant UI elements.
enum class AssistantUiElementType {
  kCard,  // See AssistantCardElement.
  kText,  // See AssistantTextElement.
};

// AssistantUiElement ----------------------------------------------------------

// Base class for a UI element that will be rendered inside of Assistant UI.
class AssistantUiElement {
 public:
  virtual ~AssistantUiElement() = default;

  AssistantUiElementType GetType() const { return type_; }

 protected:
  explicit AssistantUiElement(AssistantUiElementType type) : type_(type) {}

 private:
  const AssistantUiElementType type_;

  DISALLOW_COPY_AND_ASSIGN(AssistantUiElement);
};

// AssistantCardElement --------------------------------------------------------

// An Assistant UI element that will be rendered as an HTML card.
class AssistantCardElement : public AssistantUiElement {
 public:
  explicit AssistantCardElement(const std::string& html,
                                const std::string& fallback);
  ~AssistantCardElement() override;

  const std::string& html() const { return html_; }

  const std::string& fallback() const { return fallback_; }

  const base::UnguessableToken& id_token() const { return id_token_; }

  const base::Optional<base::UnguessableToken>& embed_token() const {
    return embed_token_;
  }

  void set_embed_token(
      const base::Optional<base::UnguessableToken>& embed_token) {
    embed_token_ = embed_token;
  }

 private:
  const std::string html_;
  const std::string fallback_;
  base::UnguessableToken id_token_;
  base::Optional<base::UnguessableToken> embed_token_ = base::nullopt;

  DISALLOW_COPY_AND_ASSIGN(AssistantCardElement);
};

// AssistantTextElement --------------------------------------------------------

// An Assistant UI element that will be rendered as text.
class AssistantTextElement : public AssistantUiElement {
 public:
  explicit AssistantTextElement(const std::string& text)
      : AssistantUiElement(AssistantUiElementType::kText), text_(text) {}

  ~AssistantTextElement() override = default;

  const std::string& text() const { return text_; }

 private:
  const std::string text_;

  DISALLOW_COPY_AND_ASSIGN(AssistantTextElement);
};

}  // namespace ash

#endif  // ASH_ASSISTANT_MODEL_ASSISTANT_UI_ELEMENT_H_
