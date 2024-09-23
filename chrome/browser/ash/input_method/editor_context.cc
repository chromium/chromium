// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/input_method/editor_context.h"

#include <optional>

#include "chrome/browser/ash/input_method/text_field_contextual_info_fetcher.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "ui/base/ime/text_input_type.h"
#include "url/gurl.h"

namespace ash::input_method {

EditorContext::EditorContext(Observer* observer,
                             System* system,
                             EditorGeolocationProvider* geolocation_provider)
    : observer_(observer),
      system_(system),
      geolocation_provider_(geolocation_provider) {}

EditorContext::~EditorContext() = default;

bool EditorContext::InTabletMode() {
  return tablet_mode_enabled_;
}

std::optional<ukm::SourceId> EditorContext::GetUkmSourceId() {
  return system_->GetUkmSourceId();
}

void EditorContext::OnInputContextUpdated(
    const TextInputMethod::InputContext& input_context,
    const TextFieldContextualInfo& text_field_contextual_info) {
  input_type_ = input_context.type;
  app_type_ = text_field_contextual_info.app_type;
  active_url_ = text_field_contextual_info.tab_url;
  app_id_ = text_field_contextual_info.app_key;
  observer_->OnContextUpdated();
}

void EditorContext::OnActivateIme(std::string_view engine_id) {
  bool ime_did_change = active_engine_id_ != engine_id;

  active_engine_id_ = engine_id;
  observer_->OnContextUpdated();

  if (ime_did_change) {
    observer_->OnImeChange(engine_id);
  }
}

void EditorContext::OnTabletModeUpdated(bool is_enabled) {
  tablet_mode_enabled_ = is_enabled;
  observer_->OnContextUpdated();
}

void EditorContext::OnTextSelectionLengthChanged(size_t text_length) {
  selected_text_length_ = text_length;
  observer_->OnContextUpdated();
}

std::string EditorContext::active_country_code() {
  return geolocation_provider_ != nullptr
             ? geolocation_provider_->GetCountryCode()
             : "";
}

std::string_view EditorContext::active_engine_id() {
  return active_engine_id_;
}

ui::TextInputType EditorContext::input_type() {
  return input_type_;
}

chromeos::AppType EditorContext::app_type() {
  return app_type_;
}

std::string_view EditorContext::app_id() {
  return app_id_;
}

GURL EditorContext::active_url() {
  return active_url_;
}

size_t EditorContext::selected_text_length() {
  return selected_text_length_;
}

}  // namespace ash::input_method
