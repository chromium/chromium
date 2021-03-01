// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_MODEL_SEARCH_SEARCH_BOX_MODEL_OBSERVER_H_
#define ASH_APP_LIST_MODEL_SEARCH_SEARCH_BOX_MODEL_OBSERVER_H_

#include "ash/app_list/model/app_list_model_export.h"
#include "base/observer_list_types.h"

namespace ash {

class APP_LIST_MODEL_EXPORT SearchBoxModelObserver
    : public base::CheckedObserver {
 public:
  // Invoked when text or voice search flag is changed.
  virtual void Update() = 0;

  // Invoked when the search engine is changed.
  virtual void SearchEngineChanged() = 0;

  // Invoked when whether to show Assistant is changed.
  virtual void ShowAssistantChanged() = 0;

 protected:
  ~SearchBoxModelObserver() override = default;
};

}  // namespace ash

#endif  // ASH_APP_LIST_MODEL_SEARCH_SEARCH_BOX_MODEL_OBSERVER_H_
