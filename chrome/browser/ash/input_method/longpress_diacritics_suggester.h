// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_INPUT_METHOD_LONGPRESS_DIACRITICS_SUGGESTER_H_
#define CHROME_BROWSER_ASH_INPUT_METHOD_LONGPRESS_DIACRITICS_SUGGESTER_H_

#include <optional>
#include <string>
#include <string_view>

#include "base/containers/fixed_flat_map.h"
#include "chrome/browser/ash/input_method/longpress_suggester.h"
#include "chrome/browser/ash/input_method/suggestion_enums.h"
#include "ui/events/event.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/gfx/range/range.h"

namespace ash::input_method {

class SuggestionHandlerInterface;

constexpr std::u16string_view kDiacriticsSeperator = u";";
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

class LongpressDiacriticsSuggester : public LongpressSuggester {
 public:
  explicit LongpressDiacriticsSuggester(
      SuggestionHandlerInterface* suggestion_handler_);
  ~LongpressDiacriticsSuggester() override;

  bool TrySuggestOnLongpress(char key_character);
  void SetEngineId(const std::string& engine_id);

  // Whether or not the given character has possible diacritic suggestion in the
  // current layout engine.
  bool HasDiacriticSuggestions(char c);

  // Suggester overrides:
  SuggestionStatus HandleKeyEvent(const ui::KeyEvent& event) override;
  bool TrySuggestWithSurroundingText(const std::u16string& text,
                                     gfx::Range selection_range) override;
  bool AcceptSuggestion(size_t index) override;
  void DismissSuggestion() override;
  AssistiveType GetProposeActionType() override;

 private:
  void ShowDiacriticsNudge();
  void SetButtonHighlighted(size_t index, bool highlighted);
  std::vector<std::u16string> GetCurrentShownDiacritics();

  // LongpressSuggester:
  void Reset() override;

  // nullopt if no suggestion window shown.
  std::optional<char> displayed_window_base_character_;
  // Highlighted index can be nullopt even if window displayed.
  std::optional<size_t> highlighted_index_;
  // Current engine id
  std::string engine_id_ = "";
};

}  // namespace ash::input_method

#endif  // CHROME_BROWSER_ASH_INPUT_METHOD_LONGPRESS_DIACRITICS_SUGGESTER_H_
