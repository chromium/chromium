// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SUPERVISED_USER_KIDS_CHROME_MANAGEMENT_KIDS_MANAGEMENT_SERVICE_H_
#define CHROME_BROWSER_SUPERVISED_USER_KIDS_CHROME_MANAGEMENT_KIDS_MANAGEMENT_SERVICE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/singleton.h"
#include "base/strings/string_piece.h"
#include "base/timer/timer.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "chrome/browser/supervised_user/kids_chrome_management/kids_profile_manager.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/primary_account_change_event.h"
#include "components/supervised_user/core/browser/kids_external_fetcher.h"
#include "components/supervised_user/core/browser/proto/kidschromemanagement_messages.pb.h"
#include "components/supervised_user/core/browser/supervised_user_service.h"
#include "content/public/browser/browser_context.h"
#include "net/base/backoff_entry.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

// A keyed service aggregating services for respective RPCs in
// KidsManagementAPI.
class KidsManagementService : public KeyedService,
                              public signin::IdentityManager::Observer,
                              supervised_user::SupervisedUserService::Delegate {
 public:
  KidsManagementService() = delete;
  KidsManagementService(
      Profile* profile,
      signin::IdentityManager& identity_manager,
      supervised_user::SupervisedUserService& supervised_user_service,
      PrefService& pref_service,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);
  KidsManagementService(const KidsManagementService&) = delete;
  KidsManagementService& operator=(const KidsManagementService&) = delete;
  ~KidsManagementService() override;

  // KeyedService implementation
  void Shutdown() override;

  // signin::IdentityManager::Observer implementation
  void OnPrimaryAccountChanged(
      const signin::PrimaryAccountChangeEvent& event_details) override;
  void OnExtendedAccountInfoUpdated(const AccountInfo& info) override;
  void OnExtendedAccountInfoRemoved(const AccountInfo& info) override;

  // SupervisedUserService::Delegate implementation
  void SetActive(bool active) override;

  // Framework initialization in the profile manager.
  void Init();

  // Initiates the fetch sequence with dependencies, and writes the result onto
  // this instance.
  void StartFetchFamilyMembers();
  void StopFetchFamilyMembers();

  // Returns true iff next fetch of family is scheduled.
  bool IsPendingNextFetchFamilyMembers() const;

  // Returns true if fetching mechanism is started.
  bool IsFetchFamilyMembersStarted() const;

  const std::vector<kids_chrome_management::FamilyMember>& family_members()
      const {
    return family_members_;
  }

  // Responds whether at least one request for child status was successful.
  // And we got answer whether the profile belongs to a child account or not.
  bool IsChildAccountStatusKnown() const;
  // Notifies observers about received status.
  void AddChildStatusReceivedCallback(base::OnceClosure callback);

 private:
  using ListFamilyMembersFetcher =
      KidsExternalFetcher<kids_chrome_management::ListFamilyMembersRequest,
                          kids_chrome_management::ListFamilyMembersResponse>;
  void ConsumeListFamilyMembers(
      KidsExternalFetcherStatus status,
      std::unique_ptr<kids_chrome_management::ListFamilyMembersResponse>
          response);

  void SetIsChildAccount(bool is_child_account);

  CoreAccountId GetAuthAccountId() const;

  raw_ptr<Profile>
      profile_;  // TODO(b/252793687): remove direct uses of the profile.
  const raw_ref<signin::IdentityManager> identity_manager_;
  const raw_ref<supervised_user::SupervisedUserService>
      supervised_user_service_;
  KidsProfileManager profile_manager_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  std::vector<base::OnceClosure> status_received_listeners_;

  // Data members related to fetching the list of family members
  std::vector<kids_chrome_management::FamilyMember> family_members_;
  net::BackoffEntry list_family_members_backoff_;
  std::unique_ptr<ListFamilyMembersFetcher> list_family_members_fetcher_;
  base::OneShotTimer list_family_members_timer_;
};

// The framework binding for the KidsManagementAPI service.
class KidsManagementServiceFactory : public ProfileKeyedServiceFactory {
 public:
  static KidsManagementService* GetForProfile(Profile* profile);
  static KidsManagementServiceFactory* GetInstance();

  KidsManagementServiceFactory(const KidsManagementServiceFactory&) = delete;
  KidsManagementServiceFactory& operator=(const KidsManagementServiceFactory&) =
      delete;

 private:
  friend struct base::DefaultSingletonTraits<KidsManagementServiceFactory>;

  KidsManagementServiceFactory();
  ~KidsManagementServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* profile) const override;
};

#endif  // CHROME_BROWSER_SUPERVISED_USER_KIDS_CHROME_MANAGEMENT_KIDS_MANAGEMENT_SERVICE_H_
