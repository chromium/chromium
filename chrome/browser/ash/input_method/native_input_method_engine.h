// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_INPUT_METHOD_NATIVE_INPUT_METHOD_ENGINE_H_
#define CHROME_BROWSER_ASH_INPUT_METHOD_NATIVE_INPUT_METHOD_ENGINE_H_

#include <optional>

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ash/input_method/assistive_suggester.h"
#include "chrome/browser/ash/input_method/assistive_suggester_switch.h"
#include "chrome/browser/ash/input_method/autocorrect_manager.h"
#include "chrome/browser/ash/input_method/grammar_manager.h"
#include "chrome/browser/ash/input_method/input_method_engine.h"
#include "chrome/browser/ash/input_method/native_input_method_engine_observer.h"
#include "chrome/browser/ash/input_method/suggestions_collector.h"
#include "chrome/browser/ui/ash/keyboard/chrome_keyboard_controller_client.h"
#include "chromeos/ash/services/ime/public/cpp/assistive_suggestions.h"
#include "chromeos/ash/services/ime/public/mojom/connection_factory.mojom.h"
#include "chromeos/ash/services/ime/public/mojom/input_engine.mojom.h"
#include "chromeos/ash/services/ime/public/mojom/input_method.mojom.h"
#include "chromeos/ash/services/ime/public/mojom/input_method_host.mojom.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "ui/base/ime/character_composer.h"

namespace ash {
namespace input_method {

// An InputMethodEngine used for the official Chrome OS build. It's a bridge
// between the Chrome OS input framework and the IME service. Although it
// currently depends on the Chrome extension system, it will not need to in
// the future after all the extensions code are migrated to the IME service.
// The design of this class is not good, mainly because we are inheriting
// from InputMethodEngine, which was designed for extension-based engines.
//
// In the final design, there should be some common interface between
// NativeInputMethodEngine and "ExtensionInputMethodEngine" (which is
// InputMethodEngine in the current design). All extensions-related logic
// will reside in the ExtensionInputMethodEngine inheritance tree. There will
// be no "NativeInputMethodEngineObserver" for the native engine either, as it
// is only used as a way for ExtensionInputMethodEngine to delegate to the
// extensions code, which is not required for the native engine.
class NativeInputMethodEngine
    : public InputMethodEngine,
      public ChromeKeyboardControllerClient::Observer {
 public:
  NativeInputMethodEngine();
  ~NativeInputMethodEngine() override;

  // |use_ime_service| can be |false| in browser tests to avoid connecting to
  // IME service which may try to load libimedecoder.so unsupported in tests.
  // TODO(crbug/1197005): Migrate native_input_method_engine_browsertest suite
  // to e2e Tast tests and unit tests, then dismantle this for-test-only flag.
  static std::unique_ptr<NativeInputMethodEngine> CreateForTesting(
      bool use_ime_service);

  // Used to override deps for testing.
  NativeInputMethodEngine(
      std::unique_ptr<AssistiveSuggesterSwitch> suggester_switch);

  // InputMethodEngine:
  void Initialize(std::unique_ptr<InputMethodEngineObserver> observer,
                  const char* extension_id,
                  Profile* profile) override;
  void CandidateClicked(uint32_t index) override;
  bool IsReadyForTesting() override;

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
  // |use_ime_service| should always be |true| in prod code, and may only be
  // |false| in browser tests that need to avoid connecting to the Mojo IME
  // service which can involve loading libimedecoder.so unsupported in tests.
  // TODO(crbug/1197005): Migrate native_input_method_engine_browsertest suite
  // to e2e Tast tests and unit tests, then dismantle this for-test-only flag.
  explicit NativeInputMethodEngine(bool use_ime_service);

  NativeInputMethodEngineObserver* GetNativeObserver() const;

  bool UpdateMenuItems(const std::vector<InputMethodManager::MenuItem>& items,
                       std::string* error) override;

  void OnInputMethodOptionsChanged() override;

  bool ShouldRouteToNativeMojoEngine(const std::string& engine_id) const;

  raw_ptr<AssistiveSuggester> assistive_suggester_ = nullptr;
  raw_ptr<AutocorrectManager> autocorrect_manager_ = nullptr;
  base::ScopedObservation<ChromeKeyboardControllerClient,
                          ChromeKeyboardControllerClient::Observer>
      chrome_keyboard_controller_client_observer_{this};

  // Optional dependency overrides used in testing.
  std::unique_ptr<AssistiveSuggesterSwitch> suggester_switch_;

  // |use_ime_service| should always be |true| in prod code, and may only be
  // |false| in browser tests that need to avoid connecting to the Mojo IME
  // service (which can involve loading libimedecoder.so unsupported in tests).
  // TODO(crbug/1197005): Migrate native_input_method_engine_browsertest suite
  // to e2e Tast tests and unit tests, then dismantle this for-test-only flag.
  bool use_ime_service_ = true;
};

}  // namespace input_method
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_INPUT_METHOD_NATIVE_INPUT_METHOD_ENGINE_H_
