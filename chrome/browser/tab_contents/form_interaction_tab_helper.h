// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TAB_CONTENTS_FORM_INTERACTION_TAB_HELPER_H_
#define CHROME_BROWSER_TAB_CONTENTS_FORM_INTERACTION_TAB_HELPER_H_

#include <memory>

#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

namespace performance_manager {
class GraphOwned;
}

namespace tabs {
class TabInterface;
}

// Indicates if a tab contains forms that have been interacted with. Owned by
// the tab's TabFeatures.
class FormInteractionTabHelper {
 public:
  DECLARE_USER_DATA(FormInteractionTabHelper);

  explicit FormInteractionTabHelper(tabs::TabInterface& tab);

  ~FormInteractionTabHelper();
  FormInteractionTabHelper(const FormInteractionTabHelper& other) = delete;
  FormInteractionTabHelper& operator=(const FormInteractionTabHelper&) = delete;

  static FormInteractionTabHelper* From(tabs::TabInterface* tab);

  // Must be called once to and passed to the PerformanceManager graph to start
  // maintaining FormInteractionTabHelpers attached to tabs.
  static std::unique_ptr<performance_manager::GraphOwned> CreateGraphObserver();

  // Note: This function will always return false in tests that don't
  // instantiate PerformanceManager.
  bool had_form_interaction() const;

 private:
  class GraphObserver;

  bool had_form_interaction_ = false;

  ui::ScopedUnownedUserData<FormInteractionTabHelper> scoped_unowned_user_data_;
};

#endif  // CHROME_BROWSER_TAB_CONTENTS_FORM_INTERACTION_TAB_HELPER_H_
