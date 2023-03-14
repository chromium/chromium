// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_INPUT_METHOD_LONGPRESS_DIACRITICS_SUGGESTER_H_
#define CHROME_BROWSER_ASH_INPUT_METHOD_LONGPRESS_DIACRITICS_SUGGESTER_H_

#include <string>

#include "base/containers/fixed_flat_map.h"
#include "base/strings/string_piece.h"
#include "base/time/time.h"
#include "chrome/browser/ash/input_method/suggester.h"
#include "chrome/browser/ash/input_method/suggestion_enums.h"
#include "chrome/browser/ash/input_method/suggestion_handler_interface.h"
#include "chrome/browser/ash/input_method/ui/assistive_delegate.h"
#include "chromeos/ash/services/ime/public/cpp/assistive_suggestions.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/events/keycodes/dom/dom_code.h"

namespace ash {
namespace input_method {

constexpr base::StringPiece16 kDiacriticsSeperator = u";";
constexpr ui::DomCode kNextDomCode = ui::DomCode::ARROW_RIGHT;
constexpr ui::DomCode kPreviousDomCode = ui::DomCode::ARROW_LEFT;
constexpr ui::DomCode kAcceptDomCode = ui::DomCode::ENTER;
constexpr ui::DomCode kDismissDomCode = ui::DomCode::ESCAPE;
constexpr ui::DomCode kTabDomCode = ui::DomCode::TAB;

// Must match IMEPKLongpressDiacriticAction in
// tools/metrics/histograms/enums.xml
enum class IMEPKLongpressDiacriticAction {
  kShowWindow = 0,
  kAccept = 1,
  kDismiss = 2,
  kAutoRepeatSuppressed = 3,
  kMaxValue = kAutoRepeatSuppressed,
};

class LongpressDiacriticsSuggester : public Suggester {
 public:
  explicit LongpressDiacriticsSuggester(
      SuggestionHandlerInterface* suggestion_handler_);
  ~LongpressDiacriticsSuggester() override;

  bool TrySuggestOnLongpress(char key_character);
  void SetEngineId(const std::string& engine_id);

  // Suggester overrides:
  void OnFocus(int context_id) override;
  void OnBlur() override;
  void OnExternalSuggestionsUpdated(
      const std::vector<ime::AssistiveSuggestion>& suggestions) override;
  SuggestionStatus HandleKeyEvent(const ui::KeyEvent& event) override;
  bool TrySuggestWithSurroundingText(const std::u16string& text,
                                     gfx::Range selection_range) override;
  bool AcceptSuggestion(size_t index) override;
  void DismissSuggestion() override;
  AssistiveType GetProposeActionType() override;
  bool HasSuggestions() override;
  std::vector<ime::AssistiveSuggestion> GetSuggestions() override;

  // Whether or not the given character has possible diacritic suggestion in
  // the current layout engine.
  bool HasDiacriticSuggestions(char c);

 private:
  void SetButtonHighlighted(size_t index, bool highlighted);
  void Reset();
  std::vector<std::u16string> GetCurrentShownDiacritics();
  SuggestionHandlerInterface* const suggestion_handler_;
  absl::optional<int> focused_context_id_;
  // nullopt if no suggestion window shown.
  absl::optional<char> displayed_window_base_character_;
  // Highlighted index can be nullopt even if window displayed.
  absl::optional<size_t> highlighted_index_;
  // Current engine id
  std::string engine_id_ = "";
};

}  // namespace input_method
}  // namespace ash
#endif  // CHROME_BROWSER_ASH_INPUT_METHOD_LONGPRESS_DIACRITICS_SUGGESTER_H_
