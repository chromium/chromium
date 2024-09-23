// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROME_BROWSER_ASH_INPUT_METHOD_NATIVE_INPUT_METHOD_ENGINE_OBSERVER_H_
#define CHROME_BROWSER_ASH_INPUT_METHOD_NATIVE_INPUT_METHOD_ENGINE_OBSERVER_H_

#include <optional>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ash/input_method/assistive_suggester.h"
#include "chrome/browser/ash/input_method/assistive_suggester_switch.h"
#include "chrome/browser/ash/input_method/autocorrect_manager.h"
#include "chrome/browser/ash/input_method/editor_event_sink.h"
#include "chrome/browser/ash/input_method/grammar_manager.h"
#include "chrome/browser/ash/input_method/input_method_engine.h"
#include "chrome/browser/ash/input_method/pref_change_recorder.h"
#include "chrome/browser/ash/input_method/suggestions_collector.h"
#include "chrome/browser/ui/ash/keyboard/chrome_keyboard_controller_client.h"
#include "chromeos/ash/services/ime/public/cpp/assistive_suggestions.h"
#include "chromeos/ash/services/ime/public/mojom/connection_factory.mojom.h"
#include "chromeos/ash/services/ime/public/mojom/input_engine.mojom.h"
#include "chromeos/ash/services/ime/public/mojom/input_method.mojom.h"
#include "chromeos/ash/services/ime/public/mojom/input_method_host.mojom.h"
#include "chromeos/ash/services/ime/public/mojom/input_method_user_data.mojom.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "ui/base/ime/ash/text_input_method.h"
#include "ui/base/ime/character_composer.h"

namespace ash {
namespace input_method {

bool CanRouteToNativeMojoEngine(const std::string& engine_id);

class NativeInputMethodEngineObserver : public InputMethodEngineObserver,
                                        public ime::mojom::InputMethodHost {
 public:
  // |ime_base_observer| is to forward events to extension during this
  // migration. It will be removed when the official extension is completely
  // migrated.
  // |use_ime_service| should always be |true| in prod code, and may only be
  // |false| in browser tests that need to avoid connecting to the Mojo IME
  // service which can involve loading libimedecoder.so unsupported in tests.
  // TODO(crbug/1197005): Migrate native_input_method_engine_browsertest suite
  // to e2e Tast tests and unit tests, then dismantle this for-test-only flag.
  NativeInputMethodEngineObserver(
      PrefService* prefs,
      EditorEventSink* editor_event_sink,
      std::unique_ptr<InputMethodEngineObserver> ime_base_observer,
      std::unique_ptr<AssistiveSuggester> assistive_suggester,
      std::unique_ptr<AutocorrectManager> autocorrect_manager,
      std::unique_ptr<SuggestionsCollector> suggestions_collector,
      std::unique_ptr<GrammarManager> grammar_manager,
      bool use_ime_service);
  ~NativeInputMethodEngineObserver() override;

  // InputMethodEngineObserver:
  void OnActivate(const std::string& engine_id) override;
  void OnFocus(const std::string& engine_id,
               int context_id,
               const TextInputMethod::InputContext& context) override;
  void OnBlur(const std::string& engine_id, int context_id) override;
  void OnKeyEvent(const std::string& engine_id,
                  const ui::KeyEvent& event,
                  TextInputMethod::KeyEventDoneCallback callback) override;
  void OnReset(const std::string& engine_id) override;
  void OnDeactivated(const std::string& engine_id) override;
  void OnCaretBoundsChanged(const gfx::Rect& caret_bounds) override;
  void OnSurroundingTextChanged(const std::string& engine_id,
                                const std::u16string& text,
                                gfx::Range selection_range,
                                int offset_pos) override;
  void OnCandidateClicked(const std::string& component_id,
                          int candidate_id,
                          MouseButtonEvent button) override;
  void OnAssistiveWindowButtonClicked(
      const ui::ime::AssistiveWindowButton& button) override;
  void OnAssistiveWindowChanged(
      const ash::ime::AssistiveWindow& window) override;
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
  void DEPRECATED_SetComposition(
      const std::u16string& text,
      std::vector<ime::mojom::CompositionSpanPtr> spans) override;
  void SetComposition(const std::u16string& text,
                      std::vector<ime::mojom::CompositionSpanPtr> spans,
                      uint32_t new_cursor_position) override;
  void SetCompositionRange(uint32_t start_index, uint32_t end_index) override;
  void FinishComposition() override;
  void DeleteSurroundingText(uint32_t num_before_cursor,
                             uint32_t num_after_cursor) override;
  void ReplaceSurroundingText(uint32_t num_before_cursor,
                              uint32_t num_after_cursor,
                              const std::u16string& text) override;
  void HandleAutocorrect(
      ime::mojom::AutocorrectSpanPtr autocorrect_span) override;
  void RequestSuggestions(ime::mojom::SuggestionsRequestPtr request,
                          RequestSuggestionsCallback callback) override;
  void DisplaySuggestions(
      const std::vector<ime::AssistiveSuggestion>& suggestions,
      const std::optional<ime::SuggestionsTextContext>& context) override;
  void UpdateCandidatesWindow(ime::mojom::CandidatesWindowPtr window) override;
  void RecordUkm(ime::mojom::UkmEntryPtr entry) override;
  void DEPRECATED_ReportKoreanAction(ime::mojom::KoreanAction action) override;
  void DEPRECATED_ReportKoreanSettings(
      ime::mojom::KoreanSettingsPtr settings) override;
  void DEPRECATED_ReportSuggestionOpportunity(
      ime::AssistiveSuggestionMode mode) override;
  void ReportHistogramSample(base::Histogram* histogram,
                             uint16_t value) override;
  void UpdateQuickSettings(
      ime::mojom::InputMethodQuickSettingsPtr quick_settings) override;

  // Called when suggestions are collected from the system via
  // suggestions_collector_.
  void OnSuggestionsGathered(RequestSuggestionsCallback request_callback,
                             ime::mojom::SuggestionsResponsePtr response);

  bool IsReadyForTesting();

  // Flush all relevant Mojo pipes.
  void FlushForTesting();

  // Returns whether this is connected to the input engine.
  bool IsConnectedForTesting() { return IsInputMethodBound(); }

  void OnProfileWillBeDestroyed();

 private:
  struct SurroundingText {
    std::u16string text;
    gfx::Range selection_range;
    int offset_pos = 0;
  };

  enum TextClientState {
    kPending = 0,
    kActive = 1,
  };

  struct TextClient {
    int context_id;
    TextClientState state;
  };

  void SendSurroundingTextToNativeMojoEngine(
      const SurroundingText& surrounding_text);

  bool ShouldRouteToRuleBasedEngine(const std::string& engine_id) const;
  bool ShouldRouteToNativeMojoEngine(const std::string& engine_id) const;

  void OnConnectionFactoryBound(bool bound);

  void ConnectToImeService(const std::string& engine_id);

  void SetJapanesePrefsFromLegacyConfig(
      ime::mojom::JapaneseLegacyConfigResponsePtr response);

  void HandleOnFocusAsyncForNativeMojoEngine(
      const std::string& engine_id,
      int context_id,
      const TextInputMethod::InputContext& context,
      const AssistiveSuggesterSwitch::EnabledSuggestions& enabled_suggestions);

  bool IsInputMethodBound();
  bool IsInputMethodConnected();
  bool IsTextClientActive();
  void OnFocusAck(int context_id,
                  bool on_focus_success,
                  ime::mojom::InputMethodMetadataPtr metadata);

  // Not owned by this class.
  raw_ptr<PrefService> prefs_ = nullptr;
  raw_ptr<EditorEventSink> editor_event_sink_;

  std::unique_ptr<InputMethodEngineObserver> ime_base_observer_;
  mojo::Remote<ime::mojom::InputEngineManager> remote_manager_;
  mojo::Remote<ime::mojom::ConnectionFactory> connection_factory_;
  mojo::Remote<ime::mojom::InputMethodUserDataService> user_data_service_;
  mojo::AssociatedRemote<ime::mojom::InputMethod> input_method_;
  mojo::AssociatedReceiver<ime::mojom::InputMethodHost> host_receiver_{this};

  std::unique_ptr<AssistiveSuggester> assistive_suggester_;
  std::unique_ptr<AutocorrectManager> autocorrect_manager_;
  std::unique_ptr<SuggestionsCollector> suggestions_collector_;
  std::unique_ptr<GrammarManager> grammar_manager_;

  std::optional<PrefChangeRecorder> pref_change_recorder_;

  ui::CharacterComposer character_composer_;

  SurroundingText last_surrounding_text_;

  std::optional<TextClient> text_client_;

  // |use_ime_service| should always be |true| in prod code, and may only be
  // |false| in browser tests that need to avoid connecting to the Mojo IME
  // service which can involve loading libimedecoder.so unsupported in tests.
  // TODO(crbug/1197005): Migrate native_input_method_engine_browsertest suite
  // to e2e Tast tests and unit tests, then dismantle this for-test-only flag.
  bool use_ime_service_ = true;

  // Timer used to show candidates with a delay. As rendering candidates is
  // slow, it is better to do it asynchronously.
  base::OneShotTimer update_candidates_timer_;

  base::WeakPtrFactory<NativeInputMethodEngineObserver> weak_ptr_factory_{this};
};

}  // namespace input_method
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_INPUT_METHOD_NATIVE_INPUT_METHOD_ENGINE_OBSERVER_H_
