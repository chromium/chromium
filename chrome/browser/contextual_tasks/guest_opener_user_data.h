// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXTUAL_TASKS_GUEST_OPENER_USER_DATA_H_
#define CHROME_BROWSER_CONTEXTUAL_TASKS_GUEST_OPENER_USER_DATA_H_

#include "content/public/browser/web_contents_user_data.h"

namespace contextual_tasks {

// A generic C++ tag class used to mark a WebContents as a dummy opener
// designed to intercept and route window.open postMessages to guest views.
class GuestOpenerUserData
    : public content::WebContentsUserData<GuestOpenerUserData> {
 public:
  ~GuestOpenerUserData() override;

  static bool IsGuestOpener(const content::WebContents* web_contents);

 private:
  explicit GuestOpenerUserData(content::WebContents* contents);
  friend class content::WebContentsUserData<GuestOpenerUserData>;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace contextual_tasks

#endif  // CHROME_BROWSER_CONTEXTUAL_TASKS_GUEST_OPENER_USER_DATA_H_
