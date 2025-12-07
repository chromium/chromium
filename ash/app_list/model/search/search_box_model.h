// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_MODEL_SEARCH_SEARCH_BOX_MODEL_H_
#define ASH_APP_LIST_MODEL_SEARCH_SEARCH_BOX_MODEL_H_

#include <optional>
#include <string>

#include "ash/app_list/model/app_list_model_export.h"
#include "ash/public/cpp/app_list/app_list_client.h"
#include "base/observer_list.h"
#include "ui/base/models/image_model.h"
#include "ui/gfx/image/image.h"

namespace ash {

class SearchBoxModelObserver;

// SearchBoxModel provides the user entered text, and the system state that
// influences the search box behavior.
class APP_LIST_MODEL_EXPORT SearchBoxModel {
 public:
  // How the Sunfish-session button should appear in the search box.
  enum class SunfishButtonVisibility {
    kHidden = 0,
    kShownWithScannerIcon = 1,
    kShownWithSunfishIcon = 2,
  };

  // Display text and icon for an icon button in search box. This is currently
  // used only for Gemini button.
  struct SearchBoxIconButton {
    std::string display_name;
    gfx::Image icon;
  };

  SearchBoxModel();
  SearchBoxModel(const SearchBoxModel&) = delete;
  SearchBoxModel& operator=(const SearchBoxModel&) = delete;
  ~SearchBoxModel();

  // TODO: crbug.com/388361414 - Delete.
  bool show_assistant_button() const { return false; }

  // Show gemini button with display name and icon specified in
  // `search_box_icon_button`. Passing `std::nullopt` hides the button.
  void SetGeminiButtonVisibility(
      std::optional<SearchBoxIconButton> search_box_icon_button);
  std::optional<SearchBoxIconButton> gemini_button() const {
    return gemini_search_box_icon_button_;
  }

  void SetSunfishButtonVisibility(SunfishButtonVisibility show);
  SunfishButtonVisibility sunfish_button_visibility() const {
    return sunfish_button_visibility_;
  }

  void SetWouldTriggerIph(bool would_trigger_iph);
  bool would_trigger_iph() const { return would_trigger_iph_; }

  void SetSearchEngineIsGoogle(bool is_google);
  bool search_engine_is_google() const { return search_engine_is_google_; }

  void AddObserver(SearchBoxModelObserver* observer);
  void RemoveObserver(SearchBoxModelObserver* observer);

 private:
  bool search_engine_is_google_ = false;
  std::optional<SearchBoxIconButton> gemini_search_box_icon_button_;
  SunfishButtonVisibility sunfish_button_visibility_ =
      SunfishButtonVisibility::kHidden;

  // `would_trigger_iph_` indicates whether we should START showing an IPH or
  // not. This can be set to false while an IPH is being shown and the IPH
  // should be kept showing.
  bool would_trigger_iph_ = false;

  base::ObserverList<SearchBoxModelObserver> observers_;
};

}  // namespace ash

#endif  // ASH_APP_LIST_MODEL_SEARCH_SEARCH_BOX_MODEL_H_
