// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_INPUT_METHOD_LONGPRESS_DIACRITICS_SUGGESTER_H_
#define CHROME_BROWSER_ASH_INPUT_METHOD_LONGPRESS_DIACRITICS_SUGGESTER_H_

#include <string>

#include "ash/services/ime/public/cpp/suggestions.h"
#include "base/containers/fixed_flat_map.h"
#include "base/strings/string_piece.h"
#include "base/time/time.h"
#include "chrome/browser/ash/input_method/suggester.h"
#include "chrome/browser/ash/input_method/suggestion_enums.h"
#include "chrome/browser/ash/input_method/suggestion_handler_interface.h"
#include "chrome/browser/ash/input_method/ui/assistive_delegate.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/events/keycodes/dom/dom_code.h"

namespace ash {
namespace input_method {

constexpr base::StringPiece16 kDiacriticsSeperator = u";";
constexpr ui::DomCode kNextDomCode = ui::DomCode::ARROW_RIGHT;
constexpr ui::DomCode kPreviousDomCode = ui::DomCode::ARROW_LEFT;
constexpr ui::DomCode kAcceptDomCode = ui::DomCode::ENTER;
constexpr ui::DomCode kDismissDomCode = ui::DomCode::ESCAPE;

// TODO(b/217560706): Replace diacritics with final set after research is
// done (on a per input method engine basis).
constexpr auto kDefaultDiacriticsMap =
    base::MakeFixedFlatMap<char, base::StringPiece16>(
        {{'a', u"à;á;â;ã;ã;ä;å;ā"},
         {'A', u"À;Á;Â;Ã;Ä;Å;Æ;Ā"},
         {'c', u"ç"},
         {'C', u"Ç"},
         {'e', u"è;é;ê;ë;ē"},
         {'E', u"È;É;Ê;Ë;Ē"},
         {'i', u"ì;í;î;ï;ī"},
         {'I', u"Ì;Í;Î;Ï;Ī"},
         {'n', u"ñ"},
         {'N', u"Ñ"},
         {'o', u"ò;ó;ô;õ;ö;ø;ō;œ"},
         {'O', u"Ò;Ò;Ó;Ô;Ö;Ø;Ō;Œ"},
         {'s', u"ß"},
         {'S', u"ẞ"},
         {'u', u"ù;ú;û;ü;ū"},
         {'U', u"Ù;Ú;Û;Ü;Ū"}});

class LongpressDiacriticsSuggester : public Suggester {
 public:
  explicit LongpressDiacriticsSuggester(
      SuggestionHandlerInterface* suggestion_handler_);
  ~LongpressDiacriticsSuggester() override;

  bool TrySuggestOnLongpress(char key_character);

  // Suggester overrides:
  void OnFocus(int context_id) override;
  void OnBlur() override;
  void OnExternalSuggestionsUpdated(
      const std::vector<ime::TextSuggestion>& suggestions) override;
  SuggestionStatus HandleKeyEvent(const ui::KeyEvent& event) override;
  bool TrySuggestWithSurroundingText(const std::u16string& text,
                                     int cursor_pos,
                                     int anchor_pos) override;
  bool AcceptSuggestion(size_t index) override;
  void DismissSuggestion() override;
  AssistiveType GetProposeActionType() override;
  bool HasSuggestions() override;
  std::vector<ime::TextSuggestion> GetSuggestions() override;

 private:
  void SetButtonHighlighted(size_t index);
  void Reset();
  std::vector<std::u16string> GetCurrentShownDiacritics();
  SuggestionHandlerInterface* const suggestion_handler_;
  absl::optional<int> focused_context_id_;
  // nullopt if no suggestion window shown.
  absl::optional<char> displayed_window_base_character_;
  // Highlighted index can be nullopt even if window displayed.
  absl::optional<size_t> highlighted_index_;
  bool text_changed_since_longpress_ = false;
};

}  // namespace input_method
}  // namespace ash
#endif  // CHROME_BROWSER_ASH_INPUT_METHOD_LONGPRESS_DIACRITICS_SUGGESTER_H_
