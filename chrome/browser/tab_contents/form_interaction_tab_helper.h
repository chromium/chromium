// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TAB_CONTENTS_FORM_INTERACTION_TAB_HELPER_H_
#define CHROME_BROWSER_TAB_CONTENTS_FORM_INTERACTION_TAB_HELPER_H_

#include "content/public/browser/web_contents_user_data.h"

namespace performance_manager {
class GraphOwned;
}

// Tab helper that indicates if a tab contains forms that have been interacted
// with.
class FormInteractionTabHelper
    : public content::WebContentsUserData<FormInteractionTabHelper> {
 public:
  // Must be called once to and passed to the PerformanceManager graph to start
  // maintaining FormInteractionTabHelpers attached to WebContents.
  static std::unique_ptr<performance_manager::GraphOwned> CreateGraphObserver();

  ~FormInteractionTabHelper() override;
  FormInteractionTabHelper(const FormInteractionTabHelper& other) = delete;
  FormInteractionTabHelper& operator=(const FormInteractionTabHelper&) = delete;

  // Note: This function will always return false in tests that don't
  // instantiate PerformanceManager.
  bool had_form_interaction() const;

  void OnHadFormInteractionChangedForTesting(bool had_form_interaction) {
    had_form_interaction_ = had_form_interaction;
  }

 private:
  friend class content::WebContentsUserData<FormInteractionTabHelper>;
  class GraphObserver;

  explicit FormInteractionTabHelper(content::WebContents* contents);

  bool had_form_interaction_ = false;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

#endif  // CHROME_BROWSER_TAB_CONTENTS_FORM_INTERACTION_TAB_HELPER_H_
