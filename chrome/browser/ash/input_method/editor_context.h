// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_INPUT_METHOD_EDITOR_CONTEXT_H_
#define CHROME_BROWSER_ASH_INPUT_METHOD_EDITOR_CONTEXT_H_

#include <optional>

#include "chrome/browser/ash/input_method/editor_geolocation_provider.h"
#include "chrome/browser/ash/input_method/text_field_contextual_info_fetcher.h"
#include "chromeos/ui/base/app_types.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "ui/base/ime/ash/text_input_method.h"
#include "ui/base/ime/text_input_type.h"
#include "url/gurl.h"

namespace ash::input_method {

// Holds any "interesting" context for the Editor feature. This includes; the
// currently active input method, size of the currently selected text, among
// other tidbits.
class EditorContext {
 public:
  class Observer {
   public:
    virtual ~Observer() = default;
    virtual void OnContextUpdated() = 0;
    virtual void OnImeChange(std::string_view engine_id) = 0;
  };

  class System {
   public:
    virtual ~System() = default;
    virtual std::optional<ukm::SourceId> GetUkmSourceId() = 0;
  };

  EditorContext(Observer* observer,
                System* system,
                EditorGeolocationProvider* geolocation_provider);
  ~EditorContext();

  bool InTabletMode();

  std::optional<ukm::SourceId> GetUkmSourceId();

  // Event listeners
  void OnInputContextUpdated(
      const TextInputMethod::InputContext& input_context,
      const TextFieldContextualInfo& text_field_contextual_info);
  void OnActivateIme(std::string_view engine_id);
  void OnTabletModeUpdated(bool tablet_mode_enabled);
  void OnTextSelectionLengthChanged(size_t new_length);

  // Getters
  std::string active_country_code();
  std::string_view active_engine_id();
  ui::TextInputType input_type();
  chromeos::AppType app_type();
  std::string_view app_id();
  GURL active_url();
  size_t selected_text_length();

 private:
  // Not owned by this class
  raw_ptr<Observer> observer_;
  raw_ptr<System> system_;
  raw_ptr<EditorGeolocationProvider> geolocation_provider_;

  std::string active_country_code_;
  std::string active_engine_id_;
  ui::TextInputType input_type_ = ui::TEXT_INPUT_TYPE_NONE;
  chromeos::AppType app_type_ = chromeos::AppType::NON_APP;
  std::string app_id_;
  GURL active_url_;
  bool tablet_mode_enabled_ = false;
  size_t selected_text_length_ = 0;
};

}  // namespace ash::input_method

#endif  // CHROME_BROWSER_ASH_INPUT_METHOD_EDITOR_CONTEXT_H_
