// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <set>
#include <string>

#include "base/no_destructor.h"
#include "base/optional.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/nearby_sharing/client/fake_nearby_share_client.h"
#include "chrome/browser/nearby_sharing/common/nearby_share_prefs.h"
#include "chrome/browser/nearby_sharing/contacts/fake_nearby_share_contact_downloader.h"
#include "chrome/browser/nearby_sharing/contacts/nearby_share_contact_downloader.h"
#include "chrome/browser/nearby_sharing/contacts/nearby_share_contact_downloader_impl.h"
#include "chrome/browser/nearby_sharing/contacts/nearby_share_contact_manager.h"
#include "chrome/browser/nearby_sharing/contacts/nearby_share_contact_manager_impl.h"
#include "chrome/browser/nearby_sharing/local_device_data/fake_nearby_share_local_device_data_manager.h"
#include "chrome/browser/nearby_sharing/proto/rpc_resources.pb.h"
#include "chrome/browser/nearby_sharing/scheduling/fake_nearby_share_scheduler.h"
#include "chrome/browser/nearby_sharing/scheduling/fake_nearby_share_scheduler_factory.h"
#include "chrome/browser/nearby_sharing/scheduling/nearby_share_scheduler_factory.h"
#include "chrome/browser/ui/webui/nearby_share/public/mojom/nearby_share_settings.mojom-test-utils.h"
#include "chrome/browser/ui/webui/nearby_share/public/mojom/nearby_share_settings.mojom.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// State for whether an upload is started, requested, or neither after a
// successful download of contacts.
enum class UploadAction { kNone, kRequest, kStart };

const char kTestContactIdPrefix[] = "id_";
const char kTestContactEmailPrefix[] = "email_";
const char kTestContactPhonePrefix[] = "phone_";

// From nearby_share_contact_manager_impl.cc.
constexpr base::TimeDelta kContactDownloadPeriod =
    base::TimeDelta::FromHours(1);
constexpr base::TimeDelta kContactDownloadRpcTimeout =
    base::TimeDelta::FromSeconds(60);

std::string GetTestContactId(size_t index) {
  return kTestContactIdPrefix + base::NumberToString(index);
}
std::string GetTestContactEmail(size_t index) {
  return kTestContactEmailPrefix + base::NumberToString(index);
}
std::string GetTestContactPhone(size_t index) {
  return kTestContactPhonePrefix + base::NumberToString(index);
}

std::set<std::string> TestContactIds(size_t num_contacts) {
  std::set<std::string> ids;
  for (size_t i = 0; i < num_contacts; ++i) {
    ids.insert(GetTestContactId(i));
  }
  return ids;
}

std::vector<nearbyshare::proto::ContactRecord> TestContactRecordList(
    size_t num_contacts) {
  std::vector<nearbyshare::proto::ContactRecord> contact_list;
  for (size_t i = 0; i < num_contacts; ++i) {
    nearbyshare::proto::ContactRecord contact;
    contact.set_id(GetTestContactId(i));
    contact.set_image_url("https://google.com");
    contact.set_person_name("John Doe");
    // only one of these fields should be set...
    switch ((i % 3)) {
      case 0:
        contact.add_identifiers()->set_account_name(GetTestContactEmail(i));
        break;
      case 1:
        contact.add_identifiers()->set_phone_number(GetTestContactPhone(i));
        break;
      case 2:
        contact.add_identifiers()->set_obfuscated_gaia("4938tyah");
        break;
    }
    contact_list.push_back(contact);
  }
  return contact_list;
}

// Converts a list of ContactRecord protos, along with the allowlist, into a
// list of Contact protos. From nearby_share_contact_manager_impl.cc.
std::vector<nearbyshare::proto::Contact> ContactRecordsToContacts(
    const std::set<std::string>& allowed_contact_ids,
    const std::vector<nearbyshare::proto::ContactRecord>& contact_records) {
  std::vector<nearbyshare::proto::Contact> contacts;
  for (const auto& contact_record : contact_records) {
    bool is_selected = base::Contains(allowed_contact_ids, contact_record.id());
    for (const auto& identifier : contact_record.identifiers()) {
      nearbyshare::proto::Contact contact;
      contact.mutable_identifier()->CopyFrom(identifier);
      contact.set_is_selected(is_selected);
      contacts.push_back(contact);
    }
  }
  return contacts;
}

class TestDownloadContactsObserver
    : public nearby_share::mojom::DownloadContactsObserver {
 public:
  void OnContactsDownloaded(
      const std::vector<std::string>& allowed_contacts,
      std::vector<nearby_share::mojom::ContactRecordPtr> contacts) override {
    allowed_contacts_ = allowed_contacts;
    contacts_ = std::move(contacts);
    on_contacts_downloaded_called_ = true;
  }

  void OnContactsDownloadFailed() override {
    on_contacts_download_failed_called_ = true;
  }

  std::vector<std::string> allowed_contacts_;
  std::vector<nearby_share::mojom::ContactRecordPtr> contacts_;
  bool on_contacts_downloaded_called_ = false;
  bool on_contacts_download_failed_called_ = false;
  mojo::Receiver<nearby_share::mojom::DownloadContactsObserver> receiver_{this};
};

}  // namespace

class NearbyShareContactManagerImplTest
    : public ::testing::Test,
      public NearbyShareContactManager::Observer {
 protected:
  struct AllowlistChangedNotification {
    bool were_contacts_added_to_allowlist;
    bool were_contacts_removed_from_allowlist;
  };
  struct ContactsDownloadedNotification {
    std::set<std::string> allowed_contact_ids;
    std::vector<nearbyshare::proto::ContactRecord> contacts;
  };
  struct ContactsUploadedNotification {
    bool did_contacts_change_since_last_upload;
  };

  NearbyShareContactManagerImplTest() = default;
  ~NearbyShareContactManagerImplTest() override = default;

  void SetUp() override {
    RegisterNearbySharingPrefs(pref_service_.registry());
    NearbyShareSchedulerFactory::SetFactoryForTesting(&scheduler_factory_);
    NearbyShareContactDownloaderImpl::Factory::SetFactoryForTesting(
        &downloader_factory_);

    manager_ = NearbyShareContactManagerImpl::Factory::Create(
        &pref_service_, &http_client_factory_, &local_device_data_manager_);
    manager_awaiter_ =
        std::make_unique<nearby_share::mojom::ContactManagerAsyncWaiter>(
            manager_.get());
    VerifySchedulerInitialization();
    manager_->AddObserver(this);
    manager_->AddDownloadContactsObserver(
        mojo_observer_.receiver_.BindNewPipeAndPassRemote());
  }

  void TearDown() override {
    manager_awaiter_.reset();
    manager_->RemoveObserver(this);
    manager_.reset();
    NearbyShareSchedulerFactory::SetFactoryForTesting(nullptr);
    NearbyShareContactDownloaderImpl::Factory::SetFactoryForTesting(nullptr);
  }

  void StartSchedulers() { manager_->Start(); }

  void DownloadContacts(bool only_download_if_changed) {
    // Manually reset these before each download.
    mojo_observer_.on_contacts_downloaded_called_ = false;
    mojo_observer_.on_contacts_download_failed_called_ = false;

    // Verify that the download scheduler is sent request.
    size_t num_requests = download_scheduler()->num_immediate_requests();
    manager_->DownloadContacts(only_download_if_changed);
    EXPECT_EQ(num_requests + 1, download_scheduler()->num_immediate_requests());
  }

  void SucceedDownload(
      bool did_contacts_change_since_last_upload,
      const base::Optional<std::vector<nearbyshare::proto::ContactRecord>>&
          contacts,
      bool expected_only_download_if_changed,
      bool expected_were_contacts_removed_from_allowlist,
      const std::set<std::string>& expected_allowed_contact_ids,
      UploadAction expected_upload_action) {
    TriggerDownloadScheduler(expected_only_download_if_changed);

    size_t num_handled_results = download_scheduler()->handled_results().size();
    size_t num_allowlist_notifications =
        allowlist_changed_notifications_.size();
    size_t num_download_notifications =
        contacts_downloaded_notifications_.size();
    size_t num_upload_requests = upload_scheduler()->num_immediate_requests();
    size_t num_upload_contacts_calls =
        local_device_data_manager_.upload_contacts_calls().size();

    latest_downloader()->Succeed(did_contacts_change_since_last_upload,
                                 contacts);

    // Verify allowlist notification sent if contacts were removed from the
    // allowlist because they no longer exist in the full contacts list.
    VerifyAllowlistNotificationSentIfNecessary(
        /*initial_num_notifications=*/num_allowlist_notifications,
        /*expected_were_contacts_added_to_allowlist=*/false,
        expected_were_contacts_removed_from_allowlist);

    VerifyDownloadNotificationSentIfNecessary(
        /*initial_num_notifications=*/num_download_notifications,
        expected_allowed_contact_ids, contacts);

    // Verify if any uploads were requested or started.
    if (contacts) {
      switch (expected_upload_action) {
        case UploadAction::kNone:
          break;
        case UploadAction::kRequest:
          ++num_upload_requests;
          break;
        case UploadAction::kStart:
          ++num_upload_contacts_calls;
          break;
      }
    }
    EXPECT_EQ(num_upload_requests,
              upload_scheduler()->num_immediate_requests());
    EXPECT_EQ(num_upload_contacts_calls,
              local_device_data_manager_.upload_contacts_calls().size());

    // Verify that the download success/failure result is sent to the
    // scheduler.
    EXPECT_EQ(num_handled_results + 1,
              download_scheduler()->handled_results().size());
    EXPECT_TRUE(download_scheduler()->handled_results().back());

    // Verify the mojo observer was called if we had contacts.
    mojo_observer_.receiver_.FlushForTesting();
    EXPECT_EQ(contacts.has_value(),
              mojo_observer_.on_contacts_downloaded_called_);
    EXPECT_FALSE(mojo_observer_.on_contacts_download_failed_called_);
    if (contacts) {
      VerifyMojoContacts(contacts.value(), mojo_observer_.contacts_);
    }
  }

  void FailDownload(bool expected_only_download_if_changed) {
    TriggerDownloadScheduler(expected_only_download_if_changed);

    // Fail download and verify that the result is sent to the scheduler.
    size_t num_handled_results = download_scheduler()->handled_results().size();
    latest_downloader()->Fail();
    EXPECT_EQ(num_handled_results + 1,
              download_scheduler()->handled_results().size());
    EXPECT_FALSE(download_scheduler()->handled_results().back());

    // Verify the mojo observer was called as well.
    mojo_observer_.receiver_.FlushForTesting();
    EXPECT_FALSE(mojo_observer_.on_contacts_downloaded_called_);
    EXPECT_TRUE(mojo_observer_.on_contacts_download_failed_called_);
  }

  void TriggerUploadFromScheduler() {
    upload_scheduler()->InvokeRequestCallback();
  }

  void FinishUpload(
      bool success,
      bool expected_did_contacts_change_since_last_upload,
      const std::vector<nearbyshare::proto::Contact>& expected_contacts) {
    FakeNearbyShareLocalDeviceDataManager::UploadContactsCall& call =
        local_device_data_manager_.upload_contacts_calls().back();
    ASSERT_EQ(expected_contacts.size(), call.contacts.size());
    for (size_t i = 0; i < expected_contacts.size(); ++i) {
      EXPECT_EQ(expected_contacts[i].SerializeAsString(),
                call.contacts[i].SerializeAsString());
    }

    // Invoke upload callback from local device data manager, verify that
    // upload notification was sent, and verify that result was sent back to
    // the upload scheduler.
    size_t num_upload_notifications = contacts_uploaded_notifications_.size();
    size_t num_handled_results = upload_scheduler()->handled_results().size();
    std::move(call.callback).Run(success);
    if (success) {
      EXPECT_EQ(num_upload_notifications + 1,
                contacts_uploaded_notifications_.size());
      EXPECT_EQ(expected_did_contacts_change_since_last_upload,
                contacts_uploaded_notifications_.back()
                    .did_contacts_change_since_last_upload);
    } else {
      EXPECT_EQ(num_upload_notifications,
                contacts_uploaded_notifications_.size());
    }
    EXPECT_EQ(num_handled_results + 1,
              upload_scheduler()->handled_results().size());
    EXPECT_EQ(success, upload_scheduler()->handled_results().back());
  }

  void SetAllowedContacts(const std::set<std::string>& allowed_contact_ids,
                          bool expected_were_contacts_added_to_allowlist,
                          bool expected_were_contacts_removed_from_allowlist) {
    size_t num_allowlist_notifications =
        allowlist_changed_notifications_.size();
    size_t num_upload_requests = upload_scheduler()->num_immediate_requests();

    manager_->SetAllowedContacts(allowed_contact_ids);

    // Verify allowlist notification sent and upload requested if contacts were
    // added or removed.
    if (expected_were_contacts_added_to_allowlist ||
        expected_were_contacts_removed_from_allowlist) {
      EXPECT_EQ(num_allowlist_notifications + 1,
                allowlist_changed_notifications_.size());
      EXPECT_EQ(expected_were_contacts_added_to_allowlist,
                allowlist_changed_notifications_.back()
                    .were_contacts_added_to_allowlist);
      EXPECT_EQ(expected_were_contacts_removed_from_allowlist,
                allowlist_changed_notifications_.back()
                    .were_contacts_removed_from_allowlist);
      EXPECT_EQ(num_upload_requests + 1,
                upload_scheduler()->num_immediate_requests());
    } else {
      EXPECT_EQ(num_allowlist_notifications,
                allowlist_changed_notifications_.size());
      EXPECT_EQ(num_upload_requests,
                upload_scheduler()->num_immediate_requests());
    }
  }

 private:
  // NearbyShareContactManager::Observer:
  void OnAllowlistChanged(bool were_contacts_added_to_allowlist,
                          bool were_contacts_removed_from_allowlist) override {
    AllowlistChangedNotification notification;
    notification.were_contacts_added_to_allowlist =
        were_contacts_added_to_allowlist;
    notification.were_contacts_removed_from_allowlist =
        were_contacts_removed_from_allowlist;
    allowlist_changed_notifications_.push_back(notification);
  }
  void OnContactsDownloaded(
      const std::set<std::string>& allowed_contact_ids,
      const std::vector<nearbyshare::proto::ContactRecord>& contacts) override {
    ContactsDownloadedNotification notification;
    notification.allowed_contact_ids = allowed_contact_ids;
    notification.contacts = contacts;
    contacts_downloaded_notifications_.push_back(notification);
  }
  void OnContactsUploaded(bool did_contacts_change_since_last_upload) override {
    ContactsUploadedNotification notification;
    notification.did_contacts_change_since_last_upload =
        did_contacts_change_since_last_upload;
    contacts_uploaded_notifications_.push_back(notification);
  }

  FakeNearbyShareContactDownloader* latest_downloader() {
    return downloader_factory_.instances().back();
  }

  FakeNearbyShareScheduler* download_scheduler() {
    return scheduler_factory_.pref_name_to_periodic_instance()
        .at(prefs::kNearbySharingSchedulerContactDownloadPrefName)
        .fake_scheduler;
  }

  FakeNearbyShareScheduler* upload_scheduler() {
    return scheduler_factory_.pref_name_to_on_demand_instance()
        .at(prefs::kNearbySharingSchedulerContactUploadPrefName)
        .fake_scheduler;
  }

  void VerifySchedulerInitialization() {
    // Verify download scheduler input parameters.
    FakeNearbyShareSchedulerFactory::PeriodicInstance
        download_scheduler_instance =
            scheduler_factory_.pref_name_to_periodic_instance().at(
                prefs::kNearbySharingSchedulerContactDownloadPrefName);
    EXPECT_TRUE(download_scheduler_instance.fake_scheduler);
    EXPECT_EQ(kContactDownloadPeriod,
              download_scheduler_instance.request_period);
    EXPECT_TRUE(download_scheduler_instance.retry_failures);
    EXPECT_TRUE(download_scheduler_instance.require_connectivity);
    EXPECT_EQ(&pref_service_, download_scheduler_instance.pref_service);

    // Verify upload scheduler input parameters.
    FakeNearbyShareSchedulerFactory::OnDemandInstance
        upload_scheduler_instance =
            scheduler_factory_.pref_name_to_on_demand_instance().at(
                prefs::kNearbySharingSchedulerContactUploadPrefName);
    EXPECT_TRUE(upload_scheduler_instance.fake_scheduler);
    EXPECT_TRUE(upload_scheduler_instance.retry_failures);
    EXPECT_TRUE(upload_scheduler_instance.require_connectivity);
    EXPECT_EQ(&pref_service_, upload_scheduler_instance.pref_service);
  }

  void TriggerDownloadScheduler(bool expected_only_download_if_changed) {
    // Fire download scheduler and verify downloader creation.
    size_t num_downloaders = downloader_factory_.instances().size();
    download_scheduler()->InvokeRequestCallback();
    EXPECT_EQ(num_downloaders + 1, downloader_factory_.instances().size());
    EXPECT_EQ(kContactDownloadRpcTimeout, downloader_factory_.latest_timeout());
    EXPECT_EQ(&http_client_factory_,
              downloader_factory_.latest_client_factory());
    EXPECT_EQ(local_device_data_manager_.GetId(),
              latest_downloader()->device_id());
    EXPECT_EQ(expected_only_download_if_changed,
              latest_downloader()->only_download_if_changed());
  }

  void VerifyAllowlistNotificationSentIfNecessary(
      size_t initial_num_notifications,
      bool expected_were_contacts_added_to_allowlist,
      bool expected_were_contacts_removed_from_allowlist) {
    if (expected_were_contacts_removed_from_allowlist) {
      EXPECT_EQ(initial_num_notifications + 1,
                allowlist_changed_notifications_.size());
      EXPECT_FALSE(allowlist_changed_notifications_.back()
                       .were_contacts_added_to_allowlist);
      EXPECT_TRUE(allowlist_changed_notifications_.back()
                      .were_contacts_removed_from_allowlist);
    } else {
      EXPECT_EQ(initial_num_notifications,
                allowlist_changed_notifications_.size());
    }
  }

  void VerifyDownloadNotificationSentIfNecessary(
      size_t initial_num_notifications,
      const std::set<std::string>& expected_allowed_contact_ids,
      const base::Optional<std::vector<nearbyshare::proto::ContactRecord>>&
          contacts) {
    // Notification should only be sent if contact list is available.
    if (contacts) {
      EXPECT_EQ(initial_num_notifications + 1,
                contacts_downloaded_notifications_.size());
      EXPECT_EQ(expected_allowed_contact_ids,
                contacts_downloaded_notifications_.back().allowed_contact_ids);
      EXPECT_EQ(contacts->size(),
                contacts_downloaded_notifications_.back().contacts.size());
      for (size_t i = 0; i < contacts->size(); ++i) {
        EXPECT_EQ(contacts->at(i).SerializeAsString(),
                  contacts_downloaded_notifications_.back()
                      .contacts[i]
                      .SerializeAsString());
      }
    } else {
      EXPECT_EQ(initial_num_notifications,
                contacts_downloaded_notifications_.size());
    }
  }

  void VerifyMojoContacts(
      const std::vector<nearbyshare::proto::ContactRecord>& proto_list,
      const std::vector<nearby_share::mojom::ContactRecordPtr>& mojo_list) {
    ASSERT_EQ(proto_list.size(), mojo_list.size());
    int i = 0;
    for (auto& proto_contact : proto_list) {
      auto& mojo_contact = mojo_list.at(i++);
      EXPECT_EQ(proto_contact.id(), mojo_contact->id);
      EXPECT_EQ(proto_contact.person_name(), mojo_contact->person_name);
      EXPECT_EQ(GURL(proto_contact.image_url()), mojo_contact->image_url);
      ASSERT_EQ((size_t)proto_contact.identifiers().size(),
                mojo_contact->identifiers.size());
      int j = 0;
      for (auto& proto_identifier : proto_contact.identifiers()) {
        auto& mojo_identifier = mojo_contact->identifiers.at(j++);
        switch (proto_identifier.identifier_case()) {
          case nearbyshare::proto::Contact_Identifier::IdentifierCase::
              kAccountName:
            EXPECT_EQ(proto_identifier.account_name(),
                      mojo_identifier->get_account_name());
            break;
          case nearbyshare::proto::Contact_Identifier::IdentifierCase::
              kObfuscatedGaia:
            EXPECT_EQ(proto_identifier.obfuscated_gaia(),
                      mojo_identifier->get_obfuscated_gaia());
            break;
          case nearbyshare::proto::Contact_Identifier::IdentifierCase::
              kPhoneNumber:
            EXPECT_EQ(proto_identifier.phone_number(),
                      mojo_identifier->get_phone_number());
            break;
          case nearbyshare::proto::Contact_Identifier::IdentifierCase::
              IDENTIFIER_NOT_SET:
            NOTREACHED();
            break;
        }
      }
    }
  }

  TestDownloadContactsObserver mojo_observer_;
  std::vector<AllowlistChangedNotification> allowlist_changed_notifications_;
  std::vector<ContactsDownloadedNotification>
      contacts_downloaded_notifications_;
  std::vector<ContactsUploadedNotification> contacts_uploaded_notifications_;
  sync_preferences::TestingPrefServiceSyncable pref_service_;
  FakeNearbyShareClientFactory http_client_factory_;
  FakeNearbyShareLocalDeviceDataManager local_device_data_manager_;
  FakeNearbyShareSchedulerFactory scheduler_factory_;
  FakeNearbyShareContactDownloader::Factory downloader_factory_;
  std::unique_ptr<NearbyShareContactManager> manager_;
  std::unique_ptr<nearby_share::mojom::ContactManagerAsyncWaiter>
      manager_awaiter_;
  content::BrowserTaskEnvironment task_environment_;
};

TEST_F(NearbyShareContactManagerImplTest, SetAllowlist) {
  // Add initial allowed contacts.
  SetAllowedContacts(TestContactIds(/*num_contacts=*/3u),
                     /*expected_were_contacts_added_to_allowlist=*/true,
                     /*expected_were_contacts_removed_from_allowlist=*/false);
  // Remove last allowed contact.
  SetAllowedContacts(TestContactIds(/*num_contacts=*/2u),
                     /*expected_were_contacts_added_to_allowlist=*/false,
                     /*expected_were_contacts_removed_from_allowlist=*/true);
  // Add back last allowed contact.
  SetAllowedContacts(TestContactIds(/*num_contacts=*/3u),
                     /*expected_were_contacts_added_to_allowlist=*/true,
                     /*expected_were_contacts_removed_from_allowlist=*/false);
  // Set list without any changes.
  SetAllowedContacts(TestContactIds(/*num_contacts=*/3u),
                     /*expected_were_contacts_added_to_allowlist=*/false,
                     /*expected_were_contacts_removed_from_allowlist=*/false);
}

TEST_F(NearbyShareContactManagerImplTest,
       DownloadContacts_Success_ChangedContactListSent_AllowlistUnchanged) {
  DownloadContacts(/*only_download_if_changed=*/false);

  // Because contacts changed since last upload, a subsequent upload should be
  // requested.
  SucceedDownload(
      /*did_contacts_change_since_last_upload=*/true,
      TestContactRecordList(/*num_contacts=*/3u),
      /*expected_only_download_if_changed=*/false,
      /*expected_were_contacts_removed_from_allowlist=*/false,
      /*expected_allowed_contact_ids=*/std::set<std::string>(),
      /*expected_upload_action=*/UploadAction::kRequest);
}

TEST_F(NearbyShareContactManagerImplTest,
       DownloadContacts_Success_UnchangedContactListSent_AllowlistUnchanged) {
  DownloadContacts(/*only_download_if_changed=*/false);

  // Because neither the contact list nor the allowlist changed, a subsequent
  // upload is not needed.
  SucceedDownload(
      /*did_contacts_change_since_last_upload=*/false,
      TestContactRecordList(/*num_contacts=*/3u),
      /*expected_only_download_if_changed=*/false,
      /*expected_were_contacts_removed_from_allowlist=*/false,
      /*expected_allowed_contact_ids=*/std::set<std::string>(),
      /*expected_upload_action=*/UploadAction::kNone);
}

TEST_F(NearbyShareContactManagerImplTest,
       DownloadContacts_Success_UnchangedContactListSent_AllowlistChanged) {
  // Add initial allowed contacts.
  SetAllowedContacts(TestContactIds(/*num_contacts=*/3u),
                     /*expected_were_contacts_added_to_allowlist=*/true,
                     /*expected_were_contacts_removed_from_allowlist=*/false);

  DownloadContacts(/*only_download_if_changed=*/false);

  // Because a contact will be removed from the allowlist because it doesn't
  // exist in the returned contact list, a subsequent upload should be
  // requested.
  SucceedDownload(
      /*did_contacts_change_since_last_upload=*/false,
      TestContactRecordList(/*num_contacts=*/2u),
      /*expected_only_download_if_changed=*/false,
      /*expected_were_contacts_removed_from_allowlist=*/true,
      /*expected_allowed_contact_ids=*/TestContactIds(/*num_contacts=*/2u),
      /*expected_upload_action=*/UploadAction::kRequest);
}

TEST_F(NearbyShareContactManagerImplTest,
       DownloadContacts_Success_ContactListNotSent) {
  // Add initial allowed contacts to make sure they're not removed.
  SetAllowedContacts(TestContactIds(/*num_contacts=*/3u),
                     /*expected_were_contacts_added_to_allowlist=*/true,
                     /*expected_were_contacts_removed_from_allowlist=*/false);

  DownloadContacts(/*only_download_if_changed=*/true);

  // No contacts were downloaded (in practice because contact didn't change and
  // we didn't request a full download).
  SucceedDownload(
      /*did_contacts_change_since_last_upload=*/false,
      /*contacts=*/base::nullopt,
      /*expected_only_download_if_changed=*/true,
      /*expected_were_contacts_removed_from_allowlist=*/false,
      /*expected_allowed_contact_ids=*/TestContactIds(/*num_contacts=*/3u),
      /*expected_upload_action=*/UploadAction::kNone);
}

TEST_F(NearbyShareContactManagerImplTest,
       DownloadContacts_Success_OverrideOnlyDownloadIfChanged) {
  // Do not force a contacts download if the list hasn't changed since the last
  // upload.
  DownloadContacts(/*only_download_if_changed=*/true);

  // Before the first request can run, request a forced contacts download even
  // if contacts haven't changed.
  DownloadContacts(/*only_download_if_changed=*/false);

  // Now, request a download only if contacts have changed.
  DownloadContacts(/*only_download_if_changed=*/true);

  // Because there was an outstanding request for a forced download when a
  // non-forced download was requested, the forced download request will take
  // priority.
  SucceedDownload(
      /*did_contacts_change_since_last_upload=*/false,
      TestContactRecordList(/*num_contacts=*/3u),
      /*expected_only_download_if_changed=*/false,
      /*expected_were_contacts_removed_from_allowlist=*/false,
      /*expected_allowed_contact_ids=*/std::set<std::string>(),
      /*expected_upload_action=*/UploadAction::kNone);

  // Now, because the request to force a contact download was fulfilled, we can
  // request a download only if contacts have changed without being trumped by a
  // previous forced-download request.
  DownloadContacts(/*only_download_if_changed=*/true);
  SucceedDownload(
      /*did_contacts_change_since_last_upload=*/false,
      /*contacts=*/base::nullopt,
      /*expected_only_download_if_changed=*/true,
      /*expected_were_contacts_removed_from_allowlist=*/false,
      /*expected_allowed_contact_ids=*/TestContactIds(/*num_contacts=*/3u),
      /*expected_upload_action=*/UploadAction::kNone);
}

TEST_F(NearbyShareContactManagerImplTest, DownloadContacts_Failure) {
  DownloadContacts(/*only_download_if_changed=*/false);
  FailDownload(/*expected_only_download_if_changed=*/false);

  // Fail twice to ensure that the downloader is reset properly.
  DownloadContacts(/*only_download_if_changed=*/false);
  FailDownload(/*expected_only_download_if_changed=*/false);
}

TEST_F(NearbyShareContactManagerImplTest,
       UploadContacts_Success_FromContactListChanged) {
  // During a regular download, notice that the contact list has changed since
  // the last upload.
  DownloadContacts(/*only_download_if_changed=*/true);
  SucceedDownload(
      /*did_contacts_change_since_last_upload=*/true,
      TestContactRecordList(/*num_contacts=*/2u),
      /*expected_only_download_if_changed=*/true,
      /*expected_were_contacts_removed_from_allowlist=*/false,
      /*expected_allowed_contact_ids=*/std::set<std::string>(),
      /*expected_upload_action=*/UploadAction::kRequest);

  // Before contacts can be uploaded, we need to first (force) re-download the
  // complete contact list.
  TriggerUploadFromScheduler();
  SucceedDownload(
      /*did_contacts_change_since_last_upload=*/true,
      TestContactRecordList(/*num_contacts=*/2u),
      /*expected_only_download_if_changed=*/false,
      /*expected_were_contacts_removed_from_allowlist=*/false,
      /*expected_allowed_contact_ids=*/std::set<std::string>(),
      /*expected_upload_action=*/UploadAction::kStart);

  // Finish the upload successfully.
  FinishUpload(
      /*success=*/true,
      /*expected_did_contacts_change_since_last_upload=*/true,
      /*expected_contacts=*/
      ContactRecordsToContacts(TestContactIds(/*num_contacts=*/0u),
                               TestContactRecordList(/*num_contacts=*/2u)));
}

TEST_F(NearbyShareContactManagerImplTest,
       UploadContacts_Success_FromAllowlistChanged) {
  // We need to manually start the schedulers first.
  StartSchedulers();
  // Add contacts to the allowlist, resulting in an upload request.
  std::set<std::string> allowlist = TestContactIds(/*num_contacts=*/1u);
  SetAllowedContacts(allowlist,
                     /*expected_were_contacts_added_to_allowlist=*/true,
                     /*expected_were_contacts_removed_from_allowlist=*/false);

  // Before contacts can be uploaded, we need to first (force) download the
  // complete contact list.
  TriggerUploadFromScheduler();
  SucceedDownload(
      /*did_contacts_change_since_last_upload=*/false,
      TestContactRecordList(/*num_contacts=*/2u),
      /*expected_only_download_if_changed=*/false,
      /*expected_were_contacts_removed_from_allowlist=*/false,
      /*expected_allowed_contact_ids=*/allowlist,
      /*expected_upload_action=*/UploadAction::kStart);

  // Finish the upload successfully.
  FinishUpload(
      /*success=*/true,
      /*expected_did_contacts_change_since_last_upload=*/false,
      /*expected_contacts=*/
      ContactRecordsToContacts(TestContactIds(/*num_contacts=*/1u),
                               TestContactRecordList(/*num_contacts=*/2u)));
}

TEST_F(NearbyShareContactManagerImplTest,
       UploadContacts_Success_DownloadRequestedWhileUploadInProgress) {
  // We need to manually start the schedulers here.
  StartSchedulers();
  // Add contacts to the allowlist, resulting in an upload request.
  SetAllowedContacts(TestContactIds(/*num_contacts=*/2u),
                     /*expected_were_contacts_added_to_allowlist=*/true,
                     /*expected_were_contacts_removed_from_allowlist=*/false);

  // Before contacts can be uploaded, we need to first (force) download the
  // complete contact list. Following a successful download, the upload will be
  // started.
  TriggerUploadFromScheduler();
  SucceedDownload(
      /*did_contacts_change_since_last_upload=*/false,
      TestContactRecordList(/*num_contacts=*/2u),
      /*expected_only_download_if_changed=*/false,
      /*expected_were_contacts_removed_from_allowlist=*/false,
      /*expected_allowed_contact_ids=*/TestContactIds(/*num_contacts=*/2u),
      /*expected_upload_action=*/UploadAction::kStart);

  // Make a download request while the upload is in progress. Because a member
  // of the allowlist was removed as a request, another upload should be
  // requested.
  DownloadContacts(/*only_download_if_changed=*/false);
  SucceedDownload(
      /*did_contacts_change_since_last_upload=*/false,
      TestContactRecordList(/*num_contacts=*/1u),
      /*expected_only_download_if_changed=*/false,
      /*expected_were_contacts_removed_from_allowlist=*/true,
      /*expected_allowed_contact_ids=*/TestContactIds(/*num_contacts=*/1u),
      /*expected_upload_action=*/UploadAction::kRequest);

  // Finish the first upload successfully, expecting the old contact list and
  // allowlist to have been used.
  FinishUpload(
      /*success=*/true,
      /*expected_did_contacts_change_since_last_upload=*/false,
      /*expected_contacts=*/
      ContactRecordsToContacts(TestContactIds(/*num_contacts=*/2u),
                               TestContactRecordList(/*num_contacts=*/2u)));

  // Run the second upload with the new contact list and allowlist.
  TriggerUploadFromScheduler();
  SucceedDownload(
      /*did_contacts_change_since_last_upload=*/false,
      TestContactRecordList(/*num_contacts=*/1u),
      /*expected_only_download_if_changed=*/false,
      /*expected_were_contacts_removed_from_allowlist=*/false,
      /*expected_allowed_contact_ids=*/TestContactIds(/*num_contacts=*/1u),
      /*expected_upload_action=*/UploadAction::kStart);
  FinishUpload(
      /*success=*/true,
      /*expected_did_contacts_change_since_last_upload=*/false,
      /*expected_contacts=*/
      ContactRecordsToContacts(TestContactIds(/*num_contacts=*/1u),
                               TestContactRecordList(/*num_contacts=*/1u)));
}

TEST_F(NearbyShareContactManagerImplTest, UploadContacts_Failure) {
  // During a regular download, notice that the contact list has changed since
  // the last upload.
  DownloadContacts(/*only_download_if_changed=*/true);
  SucceedDownload(
      /*did_contacts_change_since_last_upload=*/true,
      TestContactRecordList(/*num_contacts=*/2u),
      /*expected_only_download_if_changed=*/true,
      /*expected_were_contacts_removed_from_allowlist=*/false,
      /*expected_allowed_contact_ids=*/std::set<std::string>(),
      /*expected_upload_action=*/UploadAction::kRequest);

  // Before contacts can be uploaded, we need to first (force) re-download the
  // complete contact list.
  TriggerUploadFromScheduler();
  SucceedDownload(
      /*did_contacts_change_since_last_upload=*/true,
      TestContactRecordList(/*num_contacts=*/2u),
      /*expected_only_download_if_changed=*/false,
      /*expected_were_contacts_removed_from_allowlist=*/false,
      /*expected_allowed_contact_ids=*/std::set<std::string>(),
      /*expected_upload_action=*/UploadAction::kStart);

  // Fail the upload.
  FinishUpload(
      /*success=*/false,
      /*expected_did_contacts_change_since_last_upload=*/true,
      /*expected_contacts=*/
      ContactRecordsToContacts(TestContactIds(/*num_contacts=*/0u),
                               TestContactRecordList(/*num_contacts=*/2u)));
}
