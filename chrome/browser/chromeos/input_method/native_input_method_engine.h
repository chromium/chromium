// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_INPUT_METHOD_NATIVE_INPUT_METHOD_ENGINE_H_
#define CHROME_BROWSER_CHROMEOS_INPUT_METHOD_NATIVE_INPUT_METHOD_ENGINE_H_

#include "base/scoped_observation.h"
#include "chrome/browser/chromeos/input_method/assistive_suggester.h"
#include "chrome/browser/chromeos/input_method/autocorrect_manager.h"
#include "chrome/browser/chromeos/input_method/grammar_manager.h"
#include "chrome/browser/chromeos/input_method/input_method_engine.h"
#include "chrome/browser/chromeos/input_method/suggestions_collector.h"
#include "chrome/browser/ui/ash/keyboard/chrome_keyboard_controller_client.h"
#include "chromeos/services/ime/public/cpp/suggestions.h"
#include "chromeos/services/ime/public/mojom/input_engine.mojom.h"
#include "chromeos/services/ime/public/mojom/input_method.mojom.h"
#include "chromeos/services/ime/public/mojom/input_method_host.mojom.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "ui/base/ime/character_composer.h"

namespace chromeos {

// An InputMethodEngine used for the official Chrome OS build. It's a bridge
// between the Chrome OS input framework and the IME service. Although it
// currently depends on the Chrome extension system, it will not need to in
// the future after all the extensions code are migrated to the IME service.
// The design of this class is not good, mainly because we are inheriting
// from InputMethodEngineBase, which was designed for extension-based engines.
//
// In the final design, there should be some common interface between
// NativeInputMethodEngine and "ExtensionInputMethodEngine" (which is
// InputMethodEngineBase in the current design). All extensions-related logic
// will reside in the ExtensionInputMethodEngine inheritance tree. There will
// be no "ImeObserver" for the native engine either, as it is only used as
// a way for ExtensionInputMethodEngine to delegate to the extensions code,
// which is not required for the native engine.
class NativeInputMethodEngine
    : public InputMethodEngine,
      public ChromeKeyboardControllerClient::Observer {
 public:
  NativeInputMethodEngine();
  ~NativeInputMethodEngine() override;

  // InputMethodEngine:
  void Initialize(std::unique_ptr<InputMethodEngineBase::Observer> observer,
                  const char* extension_id,
                  Profile* profile) override;

  // ChromeKeyboardControllerClient:
  void OnKeyboardEnabledChanged(bool enabled) override;

  // ProfileObserver:
  void OnProfileWillBeDestroyed(Profile* profile) override;

  // Flush all relevant Mojo pipes.
  void FlushForTesting();

  // Returns whether this is connected to the input engine.
  bool IsConnectedForTesting() const;

  AssistiveSuggester* get_assistive_suggester_for_testing() {
    return assistive_suggester_;
  }

  AutocorrectManager* get_autocorrect_manager_for_testing() {
    return autocorrect_manager_;
  }

  // Used to show special UI to user for interacting with autocorrected text.
  // NOTE: Technically redundant to require client to supply `corrected_word` as
  // it can be retrieved from current text editing state known to IMF. However,
  // due to async situation between browser-process IMF and render-process
  // TextInputClient, it may just be a stale value if obtained that way.
  // TODO(crbug/1194424): Remove technically redundant `corrected_word` param to
  // avoid situation with multiple conflicting sources of truth.
  void OnAutocorrect(const std::u16string& typed_word,
                     const std::u16string& corrected_word,
                     int start_index);

 private:
  class ImeObserver : public InputMethodEngineBase::Observer,
                      public ime::mojom::InputMethodHost {
   public:
    // |ime_base_observer| is to forward events to extension during this
    // migration. It will be removed when the official extension is completely
    // migrated.
    ImeObserver(
        PrefService* prefs,
        std::unique_ptr<InputMethodEngineBase::Observer> ime_base_observer,
        std::unique_ptr<AssistiveSuggester> assistive_suggester,
        std::unique_ptr<AutocorrectManager> autocorrect_manager,
        std::unique_ptr<SuggestionsCollector> suggestions_collector,
        std::unique_ptr<GrammarManager> grammar_manager);
    ~ImeObserver() override;

    // InputMethodEngineBase::Observer:
    void OnActivate(const std::string& engine_id) override;
    void OnFocus(
        const std::string& engine_id,
        int context_id,
        const IMEEngineHandlerInterface::InputContext& context) override;
    void OnBlur(const std::string& engine_id, int context_id) override;
    void OnKeyEvent(
        const std::string& engine_id,
        const ui::KeyEvent& event,
        ui::IMEEngineHandlerInterface::KeyEventDoneCallback callback) override;
    void OnReset(const std::string& engine_id) override;
    void OnDeactivated(const std::string& engine_id) override;
    void OnCompositionBoundsChanged(
        const std::vector<gfx::Rect>& bounds) override;
    void OnSurroundingTextChanged(const std::string& engine_id,
                                  const std::u16string& text,
                                  int cursor_pos,
                                  int anchor_pos,
                                  int offset_pos) override;
    void OnCandidateClicked(
        const std::string& component_id,
        int candidate_id,
        InputMethodEngineBase::MouseButtonEvent button) override;
    void OnAssistiveWindowButtonClicked(
        const ui::ime::AssistiveWindowButton& button) override;
    void OnMenuItemActivated(const std::string& component_id,
                             const std::string& menu_id) override;
    void OnScreenProjectionChanged(bool is_projected) override;
    void OnSuggestionsChanged(
        const std::vector<std::string>& suggestions) override;
    void OnInputMethodOptionsChanged(const std::string& engine_id) override;

    // ime::mojom::InputMethodHost:
    void CommitText(
        const std::u16string& text,
        ime::mojom::CommitTextCursorBehavior cursor_behavior) override;
    void SetComposition(
        const std::u16string& text,
        std::vector<ime::mojom::CompositionSpanPtr> spans) override;
    void SetCompositionRange(uint32_t start_index, uint32_t end_index) override;
    void FinishComposition() override;
    void DeleteSurroundingText(uint32_t num_before_cursor,
                               uint32_t num_after_cursor) override;
    void HandleAutocorrect(
        ime::mojom::AutocorrectSpanPtr autocorrect_span) override;
    void RequestSuggestions(ime::mojom::SuggestionsRequestPtr request,
                            RequestSuggestionsCallback callback) override;
    void DisplaySuggestions(
        const std::vector<ime::TextSuggestion>& suggestions) override;
    void RecordUkm(ime::mojom::UkmEntryPtr entry) override;

    // Called when suggestions are collected from the system via
    // suggestions_collector_.
    void OnSuggestionsGathered(RequestSuggestionsCallback request_callback,
                               ime::mojom::SuggestionsResponsePtr response);

    // Flush all relevant Mojo pipes.
    void FlushForTesting();

    // Returns whether this is connected to the input engine.
    bool IsConnectedForTesting() const { return input_method_.is_bound(); }

    void OnProfileWillBeDestroyed();

   private:
    PrefService* prefs_ = nullptr;

    std::unique_ptr<InputMethodEngineBase::Observer> ime_base_observer_;
    mojo::Remote<ime::mojom::InputEngineManager> remote_manager_;
    mojo::Remote<ime::mojom::InputMethod> input_method_;
    mojo::Receiver<ime::mojom::InputMethodHost> host_receiver_{this};

    std::unique_ptr<AssistiveSuggester> assistive_suggester_;
    std::unique_ptr<AutocorrectManager> autocorrect_manager_;
    std::unique_ptr<SuggestionsCollector> suggestions_collector_;
    std::unique_ptr<GrammarManager> grammar_manager_;

    ui::CharacterComposer character_composer_;
  };

  ImeObserver* GetNativeObserver() const;

  void OnInputMethodOptionsChanged() override;

  AssistiveSuggester* assistive_suggester_ = nullptr;
  AutocorrectManager* autocorrect_manager_ = nullptr;
  base::ScopedObservation<ChromeKeyboardControllerClient,
                          ChromeKeyboardControllerClient::Observer>
      chrome_keyboard_controller_client_observer_{this};
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_INPUT_METHOD_NATIVE_INPUT_METHOD_ENGINE_H_
