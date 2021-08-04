// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_CDM_PREF_SERVICE_HELPER_H_
#define CHROME_BROWSER_MEDIA_CDM_PREF_SERVICE_HELPER_H_

#include "media/cdm/cdm_preference_data.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
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

  // Gets the CDM preference data associated with the current origin. If no
  // preference data exist for the current origin, an entry is created with a
  // new origin id and an empty client token. Returns nullptr if the preference
  // could not be retrieved.
  static std::unique_ptr<media::CdmPreferenceData> GetCdmPreferenceData(
      PrefService* user_prefs,
      const url::Origin& cdm_origin);

  // Sets the client token for the origin associated with the CDM. The token is
  // set by the CDM. If no entry exist for the current origin, the client token
  // will not be saved.
  static void SetCdmClientToken(PrefService* user_prefs,
                                const url::Origin& cdm_origin,
                                const std::vector<uint8_t>& client_token);
};

#endif  // CHROME_BROWSER_MEDIA_CDM_PREF_SERVICE_HELPER_H_
