// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_INTENT_HELPER_INTENT_PICKER_AUTO_DISPLAY_SERVICE_H_
#define CHROME_BROWSER_APPS_INTENT_HELPER_INTENT_PICKER_AUTO_DISPLAY_SERVICE_H_

#include "base/macros.h"
#include "chrome/browser/apps/intent_helper/intent_picker_auto_display_pref.h"
#include "components/keyed_service/core/keyed_service.h"
#include "url/gurl.h"

class Profile;

// TODO(crbug.com/902660): Tie IntentPickerAutoDisplayPref to this class so
// their life cycle is the same and they both refer to the same url or origin,
// this way passing url as a param nor creating a new Pref every time will be
// necessary.
class IntentPickerAutoDisplayService : public KeyedService {
 public:
  static IntentPickerAutoDisplayService* Get(Profile* profile);

  explicit IntentPickerAutoDisplayService(Profile* profile);

  // Returns whether or not a likely |url| has triggered the UI 2+ times without
  // the user engaging.
  bool ShouldAutoDisplayUi(const GURL& url);

  // Keep track of the |url| repetitions.
  void IncrementCounter(const GURL& url);

  // Returns the last platform selected by the user to handle |url|.
  // If it has not been checked then it will return |Platform::kNone|
  // for devices of tablet form factor.
  IntentPickerAutoDisplayPref::Platform GetLastUsedPlatformForTablets(
      const GURL& url);

  // Updates the Platform to |platform| for |url| for devices of
  // tablet form factor.
  void UpdatePlatformForTablets(const GURL& url,
                                IntentPickerAutoDisplayPref::Platform platform);

 private:
  Profile* profile_;

  DISALLOW_COPY_AND_ASSIGN(IntentPickerAutoDisplayService);
};

#endif  // CHROME_BROWSER_APPS_INTENT_HELPER_INTENT_PICKER_AUTO_DISPLAY_SERVICE_H_
