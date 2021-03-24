// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_INPUT_METHOD_INPUT_METHOD_ENGINE_H_
#define CHROME_BROWSER_CHROMEOS_INPUT_METHOD_INPUT_METHOD_ENGINE_H_

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <string>
#include <vector>

#include "chrome/browser/chromeos/input_method/assistive_window_properties.h"
#include "chrome/browser/chromeos/input_method/input_method_engine_base.h"
#include "chrome/browser/chromeos/input_method/suggestion_handler_interface.h"
#include "ui/base/ime/candidate_window.h"
#include "ui/base/ime/chromeos/ime_engine_handler_interface.h"
#include "ui/base/ime/chromeos/input_method_descriptor.h"
#include "ui/base/ime/chromeos/input_method_manager.h"
#include "url/gurl.h"

namespace ui {
struct CompositionText;
class KeyEvent;

namespace ime {
struct AssistiveWindowButton;
struct InputMethodMenuItem;
struct SuggestionDetails;
}  // namespace ime
}  // namespace ui

namespace chromeos {

struct AssistiveWindowProperties;

class InputMethodEngine : public InputMethodEngineBase,
                          public SuggestionHandlerInterface {
 public:
  enum {
    MENU_ITEM_MODIFIED_LABEL = 0x0001,
    MENU_ITEM_MODIFIED_STYLE = 0x0002,
    MENU_ITEM_MODIFIED_VISIBLE = 0x0004,
    MENU_ITEM_MODIFIED_ENABLED = 0x0008,
    MENU_ITEM_MODIFIED_CHECKED = 0x0010,
    MENU_ITEM_MODIFIED_ICON = 0x0020,
  };

  enum CandidateWindowPosition {
    WINDOW_POS_CURSOR,
    WINDOW_POS_COMPOSITTION,
  };

  struct UsageEntry {
    std::string title;
    std::string body;
  };

  struct Candidate {
    Candidate();
    Candidate(const Candidate& other);
    virtual ~Candidate();

    std::string value;
    int id;
    std::string label;
    std::string annotation;
    UsageEntry usage;
    std::vector<Candidate> candidates;
  };

  struct CandidateWindowProperty {
    CandidateWindowProperty();
    virtual ~CandidateWindowProperty();
    CandidateWindowProperty(const CandidateWindowProperty& other);
    int page_size;
    bool is_cursor_visible;
    bool is_vertical;
    bool show_window_at_composition;

    // Auxiliary text is typically displayed in the footer of the candidate
    // window.
    std::string auxiliary_text;
    bool is_auxiliary_text_visible;

    // The index of the current chosen candidate out of total candidates.
    // value is -1 if there is no current chosen candidate.
    int current_candidate_index = -1;
    int total_candidates = 0;
  };

  InputMethodEngine();

  ~InputMethodEngine() override;

  // InputMethodEngineBase overrides.
  void Enable(const std::string& component_id) override;
  bool IsActive() const override;

  // ui::IMEEngineHandlerInterface overrides.
  void FocusIn(const ui::IMEEngineHandlerInterface::InputContext& input_context)
      override;
  void FocusOut() override;
  void PropertyActivate(const std::string& property_name) override;
  void CandidateClicked(uint32_t index) override;
  void AssistiveWindowButtonClicked(
      const ui::ime::AssistiveWindowButton& button) override;
  void SetMirroringEnabled(bool mirroring_enabled) override;
  void SetCastingEnabled(bool casting_enabled) override;
  ui::VirtualKeyboardController* GetVirtualKeyboardController() const override;

  // SuggestionHandlerInterface overrides.
  bool DismissSuggestion(int context_id, std::string* error) override;
  bool SetSuggestion(int context_id,
                     const ui::ime::SuggestionDetails& details,
                     std::string* error) override;
  bool AcceptSuggestion(int context_id, std::string* error) override;
  void OnSuggestionsChanged(
      const std::vector<std::string>& suggestions) override;

  bool SetButtonHighlighted(int context_id,
                            const ui::ime::AssistiveWindowButton& button,
                            bool highlighted,
                            std::string* error) override;

  void ClickButton(const ui::ime::AssistiveWindowButton& button) override;

  bool AcceptSuggestionCandidate(int context_id,
                                 const std::u16string& candidate,
                                 std::string* error) override;

  bool SetAssistiveWindowProperties(
      int context_id,
      const AssistiveWindowProperties& assistive_window,
      std::string* error) override;

  // This function returns the current property of the candidate window of the
  // corresponding engine_id. If the CandidateWindowProperty is not set for the
  // engine_id, a default value is set. The caller can use the returned value as
  // the default property and modify some of specified items.
  const CandidateWindowProperty& GetCandidateWindowProperty(
      const std::string& engine_id);

  // Changes the property of the candidate window of the given engine_id and
  // repaints the candidate window widget.
  void SetCandidateWindowProperty(const std::string& engine_id,
                                  const CandidateWindowProperty& property);

  // Show or hide the candidate window.
  bool SetCandidateWindowVisible(bool visible, std::string* error);

  // Set the list of entries displayed in the candidate window.
  bool SetCandidates(int context_id,
                     const std::vector<Candidate>& candidates,
                     std::string* error);

  // Set the position of the cursor in the candidate window.
  bool SetCursorPosition(int context_id, int candidate_id, std::string* error);

  // Set the list of items that appears in the language menu when this IME is
  // active.
  bool SetMenuItems(
      const std::vector<input_method::InputMethodManager::MenuItem>& items,
      std::string* error);

  // Update the state of the menu items.
  bool UpdateMenuItems(
      const std::vector<input_method::InputMethodManager::MenuItem>& items,
      std::string* error);

  // Hides the input view window (from API call).
  void HideInputView();

  // Sets the autocorrect range to be `range`. The `range` is in bytes.
  // TODO(b/171924748): Improve documentation for this function all the way down
  // the stack.
  bool SetAutocorrectRange(const gfx::Range& range) override;

  gfx::Range GetAutocorrectRange() override;

 private:
  // InputMethodEngineBase:
  void UpdateComposition(const ui::CompositionText& composition_text,
                         uint32_t cursor_pos,
                         bool is_visible) override;
  bool SetCompositionRange(
      uint32_t before,
      uint32_t after,
      const std::vector<ui::ImeTextSpan>& text_spans) override;
  bool SetComposingRange(
      uint32_t start,
      uint32_t end,
      const std::vector<ui::ImeTextSpan>& text_spans) override;


  gfx::Rect GetAutocorrectCharacterBounds() override;


  bool SetSelectionRange(uint32_t start, uint32_t end) override;

  void CommitTextToInputContext(int context_id,
                                const std::u16string& text) override;

  bool SendKeyEvent(const ui::KeyEvent& event, std::string* error) override;

  // Enables overriding input view page to Virtual Keyboard window.
  void EnableInputView();

  // Converts MenuItem to InputMethodMenuItem.
  void MenuItemToProperty(
      const input_method::InputMethodManager::MenuItem& item,
      ui::ime::InputMethodMenuItem* property);

  // The current candidate window.
  ui::CandidateWindow candidate_window_;

  // The candidate window property of the current engine_id.
  std::pair<std::string, CandidateWindowProperty> candidate_window_property_;

  // Indicates whether the candidate window is visible.
  bool window_visible_ = false;

  // Mapping of candidate index to candidate id.
  std::vector<int> candidate_ids_;

  // Mapping of candidate id to index.
  std::map<int, int> candidate_indexes_;

  // Whether the screen is in mirroring mode.
  bool is_mirroring_ = false;

  // Whether the desktop is being casted.
  bool is_casting_ = false;

  DISALLOW_COPY_AND_ASSIGN(InputMethodEngine);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_INPUT_METHOD_INPUT_METHOD_ENGINE_H_
