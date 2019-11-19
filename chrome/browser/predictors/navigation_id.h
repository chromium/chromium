// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PREDICTORS_NAVIGATION_ID_H_
#define CHROME_BROWSER_PREDICTORS_NAVIGATION_ID_H_

#include <stddef.h>

#include "base/feature_list.h"
#include "base/time/time.h"
#include "components/sessions/core/session_id.h"
#include "url/gurl.h"

namespace content {
class WebContents;
}

namespace predictors {

// Represents a single navigation for a render frame.
struct NavigationID {
  NavigationID();
  explicit NavigationID(content::WebContents* web_contents);
  NavigationID(content::WebContents* web_contents,
               const GURL& main_frame_url,
               const base::TimeTicks& creation_time);
  NavigationID(const NavigationID& other);

  bool operator<(const NavigationID& rhs) const;
  bool operator==(const NavigationID& rhs) const;

  // Returns true iff the tab_id is valid and the Main frame URL is set.
  bool is_valid() const;

  SessionID tab_id;
  GURL main_frame_url;

  // NOTE: Even though we store the creation time here, it is not used during
  // comparison of two NavigationIDs because it cannot always be determined
  // correctly.
  base::TimeTicks creation_time;
};

}  // namespace predictors

#endif  // CHROME_BROWSER_PREDICTORS_NAVIGATION_ID_H_
