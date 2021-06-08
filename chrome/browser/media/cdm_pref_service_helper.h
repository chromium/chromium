// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_CDM_PREF_SERVICE_HELPER_H_
#define CHROME_BROWSER_MEDIA_CDM_PREF_SERVICE_HELPER_H_

#include "url/origin.h"

class PrefService;
class PrefRegistrySimple;

// Manages reads and writes to the user prefs service related to CDM usage.
// Updates to the CDM Origin ID dictionary will be infrequent (ie. every time
// the Media Foundation CDM is used for a new origin). Origin ID are only stored
// for origins serving hardware security protected contents and as such the size
// of the CDM Origin ID dictionary should only contain a handful of items.
class CdmPrefServiceHelper {
 public:
  CdmPrefServiceHelper();
  CdmPrefServiceHelper(const CdmPrefServiceHelper&) = delete;
  CdmPrefServiceHelper& operator=(const CdmPrefServiceHelper&) = delete;
  ~CdmPrefServiceHelper();

  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  static base::UnguessableToken GetCdmOriginId(PrefService* user_prefs,
                                               const url::Origin& origin);
};

#endif  // CHROME_BROWSER_MEDIA_CDM_PREF_SERVICE_HELPER_H_
