// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_MODEL_SEARCH_SEARCH_BOX_MODEL_H_
#define ASH_APP_LIST_MODEL_SEARCH_SEARCH_BOX_MODEL_H_

#include "ash/app_list/model/app_list_model_export.h"
#include "ash/public/cpp/app_list/app_list_client.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"

namespace ash {

class SearchBoxModelObserver;

// SearchBoxModel provides the user entered text, and the system state that
// influences the search box behavior.
class APP_LIST_MODEL_EXPORT SearchBoxModel {
 public:
  SearchBoxModel();
  SearchBoxModel(const SearchBoxModel&) = delete;
  SearchBoxModel& operator=(const SearchBoxModel&) = delete;
  ~SearchBoxModel();

  void SetShowAssistantButton(bool show);
  bool show_assistant_button() const { return show_assistant_button_; }

  void SetWouldTriggerIph(bool would_trigger_iph);
  bool would_trigger_iph() const { return would_trigger_iph_; }

  void SetSearchEngineIsGoogle(bool is_google);
  bool search_engine_is_google() const { return search_engine_is_google_; }

  void AddObserver(SearchBoxModelObserver* observer);
  void RemoveObserver(SearchBoxModelObserver* observer);

 private:
  bool search_engine_is_google_ = false;
  bool show_assistant_button_ = false;

  // `would_trigger_iph_` indicates whether we should START showing an IPH or
  // not. This can be set to false while an IPH is being shown and the IPH
  // should be kept showing.
  bool would_trigger_iph_ = false;

  base::ObserverList<SearchBoxModelObserver> observers_;
};

}  // namespace ash

#endif  // ASH_APP_LIST_MODEL_SEARCH_SEARCH_BOX_MODEL_H_
