// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_TRIGGER_CREATOR_H_
#define CHROME_BROWSER_SAFE_BROWSING_TRIGGER_CREATOR_H_

class Profile;

namespace content {
class WebContents;
}

namespace safe_browsing {

// Takes care of creation of individual triggers. This functionality lives in a
// separate class from TriggerManager to avoid circular dependencies.
// TriggerManager need not know about individual trigger classes, while the
// trigger classes needs to know about the TriggerManager in order to fire
// triggers.
class TriggerCreator {
 public:
  static void MaybeCreateTriggersForWebContents(
      Profile* profile,
      content::WebContents* web_contents);
};

}  // namespace safe_browsing
#endif  // CHROME_BROWSER_SAFE_BROWSING_TRIGGER_CREATOR_H_
