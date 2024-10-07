// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROFILES_PROFILE_STATISTICS_H_
#define CHROME_BROWSER_PROFILES_PROFILE_STATISTICS_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/profiles/profile_statistics_common.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/password_manager/core/browser/password_store/password_store_interface.h"
#include "components/user_annotations/user_annotations_service.h"
#include "device/fido/platform_credential_store.h"

class PrefService;
class ProfileStatisticsAggregator;

namespace autofill {
class PersonalDataManager;
}  // namespace autofill

namespace bookmarks {
class BookmarkModel;
}  // namespace bookmarks

namespace history {
class HistoryService;
}  // namespace history

// Instances of ProfileStatistics should be created directly. Use
// ProfileStatisticsFactory instead.
class ProfileStatistics : public KeyedService {
 public:
  // Profile Statistics --------------------------------------------------------

  // This function collects statistical information about |profile|, also
  // returns the information via |callback| if |callback| is not null.
  // Currently bookmarks, history, logins and autofill forms are counted. The
  // callback function will probably be called more than once, so binding
  // parameters with bind::Passed() is prohibited.
  void GatherStatistics(profiles::ProfileStatisticsCallback callback);

 private:
  friend class ProfileStatisticsFactory;

  ProfileStatistics(
      scoped_refptr<autofill::AutofillWebDataService> autofill_web_data_service,
      autofill::PersonalDataManager* personal_data_manager,
      bookmarks::BookmarkModel* bookmark_model,
      history::HistoryService* history_service,
      scoped_refptr<password_manager::PasswordStoreInterface>
          profile_password_store,
      PrefService* pref_service,
      user_annotations::UserAnnotationsService* user_annotations_service,
      std::unique_ptr<device::fido::PlatformCredentialStore>
          platform_credential_store);
  ~ProfileStatistics() override;
  void DeregisterAggregator();

  const scoped_refptr<autofill::AutofillWebDataService>
      autofill_web_data_service_;
  const raw_ptr<autofill::PersonalDataManager> personal_data_manager_;
  const raw_ptr<bookmarks::BookmarkModel> bookmark_model_;
  const raw_ptr<history::HistoryService> history_service_;
  const scoped_refptr<password_manager::PasswordStoreInterface>
      profile_password_store_;
  const raw_ptr<PrefService> pref_service_;
  const raw_ptr<user_annotations::UserAnnotationsService>
      user_annotations_service_;

  std::unique_ptr<device::fido::PlatformCredentialStore>
      platform_credential_store_;
  std::unique_ptr<ProfileStatisticsAggregator> aggregator_;
  base::WeakPtrFactory<ProfileStatistics> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_PROFILES_PROFILE_STATISTICS_H_
