// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_CDM_PREF_SERVICE_HELPER_H_
#define CHROME_BROWSER_MEDIA_CDM_PREF_SERVICE_HELPER_H_

#include <map>
#include <optional>
#include <vector>

#include "base/functional/callback.h"
#include "base/time/time.h"
#include "base/unguessable_token.h"
#include "url/gurl.h"
#include "url/origin.h"

class PrefService;
class PrefRegistrySimple;

class CdmPrefData {
 public:
  CdmPrefData(const base::UnguessableToken& origin_id,
              base::Time origin_id_time);

  CdmPrefData(const base::UnguessableToken& origin_id,
              base::Time origin_id_time,
              std::vector<base::Time> hw_secure_decryption_disable_times);

  ~CdmPrefData();

  const base::UnguessableToken& origin_id() const;
  base::Time origin_id_creation_time() const;
  const std::optional<std::vector<uint8_t>> client_token() const;
  base::Time client_token_creation_time() const;
  std::vector<base::Time> hw_secure_decryption_disable_times() const;

  void SetClientToken(const std::vector<uint8_t>& client_token,
                      const base::Time creation_time);

 private:
  base::UnguessableToken origin_id_;
  base::Time origin_id_creation_time_;
  std::vector<base::Time> hw_secure_decryption_disable_times_;

  std::optional<std::vector<uint8_t>> client_token_;
  base::Time client_token_creation_time_;
};

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
  static void ClearCdmPreferenceData(
      PrefService* user_prefs,
      base::Time start,
      base::Time end,
      const base::RepeatingCallback<bool(const GURL&)>& filter);

  // Gets the CDM preference data associated with the current origin. If no
  // preference data exist for the current origin, an entry is created with a
  // new origin id and an empty client token. Returns nullptr if the preference
  // could not be retrieved.
  static std::unique_ptr<CdmPrefData> GetCdmPrefData(
      PrefService* user_prefs,
      const url::Origin& cdm_origin);

  // Sets the client token for the origin associated with the CDM. The token is
  // set by the CDM. If no entry exist for the current origin, the client token
  // will not be saved.
  static void SetCdmClientToken(PrefService* user_prefs,
                                const url::Origin& cdm_origin,
                                const std::vector<uint8_t>& client_token);

  // Return a mapping of Origin ID to url::Origin. The string representation
  // for the origin id is used in the mapping so that it can be more easily used
  // to map a directory name to its origin.
  static std::map<std::string, url::Origin> GetOriginIdMapping(
      PrefService* user_prefs);
};

#endif  // CHROME_BROWSER_MEDIA_CDM_PREF_SERVICE_HELPER_H_
