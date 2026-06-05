// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_GLIC_TAB_RESTORE_DATA_H_
#define CHROME_BROWSER_GLIC_GLIC_TAB_RESTORE_DATA_H_

#include <string>
#include <vector>

#include "content/public/browser/web_contents_user_data.h"

namespace glic {

struct GlicRestoredState {
  GlicRestoredState();
  ~GlicRestoredState();
  GlicRestoredState(const GlicRestoredState&) = delete;
  GlicRestoredState& operator=(const GlicRestoredState&) = delete;
  GlicRestoredState(GlicRestoredState&&);
  GlicRestoredState& operator=(GlicRestoredState&&);

  struct InstanceInfo {
    std::string instance_id;
    std::string conversation_id;
  };

  InstanceInfo bound_instance;
  bool side_panel_open = false;
  std::vector<InstanceInfo> pinned_instances;
};

// Holds Glic restoration data on a WebContents until it is inserted into a
// TabStripModel and processed by GlicTabStripObserver.
class GlicTabRestoreData
    : public content::WebContentsUserData<GlicTabRestoreData> {
 public:
  ~GlicTabRestoreData() override;

  const GlicRestoredState& state() const { return state_; }

 private:
  // Friend required for WebContentsUserData to call the private constructor.
  friend class content::WebContentsUserData<GlicTabRestoreData>;

  GlicTabRestoreData(content::WebContents* contents, GlicRestoredState state);

  GlicRestoredState state_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_GLIC_TAB_RESTORE_DATA_H_
