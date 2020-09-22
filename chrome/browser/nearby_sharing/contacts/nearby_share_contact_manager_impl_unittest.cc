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

const char kTestContactIdPrefix[] = "id_";
const char kTestContactEmailPrefix[] = "email_";
const char kTestContactPhonePrefix[] = "phone_";
const char kTestDefaultDeviceName[] = "Josh's Chromebook";

// From nearby_share_contact_manager_impl.cc.
constexpr base::TimeDelta kContactDownloadPeriod =
    base::TimeDelta::FromHours(12);
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

  NearbyShareContactManagerImplTest()
      : local_device_data_manager_(kTestDefaultDeviceName) {}

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

  void DownloadContacts() {
    // Manually reset these before each download.
    mojo_observer_.on_contacts_downloaded_called_ = false;
    mojo_observer_.on_contacts_download_failed_called_ = false;

    // Verify that the scheduler is sent a request.
    size_t num_requests =
        download_and_upload_scheduler()->num_immediate_requests();
    manager_->DownloadContacts();
    EXPECT_EQ(num_requests + 1,
              download_and_upload_scheduler()->num_immediate_requests());
  }

  void SucceedDownload(
      const std::vector<nearbyshare::proto::ContactRecord>& contacts,
      const std::set<std::string>& expected_allowed_contact_ids,
      bool expect_upload) {
    TriggerScheduler();

    size_t num_handled_results =
        download_and_upload_scheduler()->handled_results().size();
    size_t num_download_notifications =
        contacts_downloaded_notifications_.size();
    size_t num_upload_contacts_calls =
        local_device_data_manager_.upload_contacts_calls().size();

    latest_downloader()->Succeed(contacts);

    VerifyDownloadNotificationSent(
        /*initial_num_notifications=*/num_download_notifications,
        expected_allowed_contact_ids, contacts);

    // Verify the mojo observer was called.
    mojo_observer_.receiver_.FlushForTesting();
    EXPECT_TRUE(mojo_observer_.on_contacts_downloaded_called_);
    EXPECT_FALSE(mojo_observer_.on_contacts_download_failed_called_);
    VerifyMojoContacts(contacts, mojo_observer_.contacts_);

    // Verify that contacts start uploading if the needed.
    EXPECT_EQ(num_upload_contacts_calls + (expect_upload ? 1 : 0),
              local_device_data_manager_.upload_contacts_calls().size());

    // Verify that the success result is sent to the download/upload scheduler
    // if a subsequent upload isn't required.
    EXPECT_EQ(num_handled_results + (expect_upload ? 0 : 1),
              download_and_upload_scheduler()->handled_results().size());
    if (!expect_upload) {
      EXPECT_TRUE(download_and_upload_scheduler()->handled_results().back());
    }
  }

  void FailDownload() {
    TriggerScheduler();

    // Fail download and verify that the result is sent to the scheduler.
    size_t num_handled_results =
        download_and_upload_scheduler()->handled_results().size();
    latest_downloader()->Fail();
    EXPECT_EQ(num_handled_results + 1,
              download_and_upload_scheduler()->handled_results().size());
    EXPECT_FALSE(download_and_upload_scheduler()->handled_results().back());

    // Verify the mojo observer was called as well.
    mojo_observer_.receiver_.FlushForTesting();
    EXPECT_FALSE(mojo_observer_.on_contacts_downloaded_called_);
    EXPECT_TRUE(mojo_observer_.on_contacts_download_failed_called_);
  }

  void FinishUpload(
      bool success,
      const std::vector<nearbyshare::proto::Contact>& expected_contacts) {
    FakeNearbyShareLocalDeviceDataManager::UploadContactsCall& call =
        local_device_data_manager_.upload_contacts_calls().back();
    ASSERT_EQ(expected_contacts.size(), call.contacts.size());
    for (size_t i = 0; i < expected_contacts.size(); ++i) {
      EXPECT_EQ(expected_contacts[i].SerializeAsString(),
                call.contacts[i].SerializeAsString());
    }

    // Invoke upload callback from local device data manager.
    size_t num_upload_notifications = contacts_uploaded_notifications_.size();
    size_t num_handled_results =
        download_and_upload_scheduler()->handled_results().size();
    std::move(call.callback).Run(success);

    // Verify upload notification was sent on success.
    EXPECT_EQ(num_upload_notifications + (success ? 1 : 0),
              contacts_uploaded_notifications_.size());
    if (success) {
      // We only expect uploads to occur if contacts have changed since the last
      // upload.
      EXPECT_TRUE(contacts_uploaded_notifications_.back()
                      .did_contacts_change_since_last_upload);
    }

    // Verify that result is sent to download/upload scheduler.
    EXPECT_EQ(num_handled_results + 1,
              download_and_upload_scheduler()->handled_results().size());
    EXPECT_EQ(success,
              download_and_upload_scheduler()->handled_results().back());
  }

  void SetAllowedContacts(const std::set<std::string>& allowed_contact_ids,
                          bool expect_allowlist_changed) {
    size_t num_download_and_upload_requests =
        download_and_upload_scheduler()->num_immediate_requests();

    manager_->SetAllowedContacts(allowed_contact_ids);

    // Verify download/upload requested if allowlist changed.
    EXPECT_EQ(
        num_download_and_upload_requests + (expect_allowlist_changed ? 1 : 0),
        download_and_upload_scheduler()->num_immediate_requests());
  }

  PrefService* pref_service() { return &pref_service_; }

 private:
  // NearbyShareContactManager::Observer:
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

  FakeNearbyShareScheduler* download_and_upload_scheduler() {
    return scheduler_factory_.pref_name_to_periodic_instance()
        .at(prefs::kNearbySharingSchedulerContactDownloadAndUploadPrefName)
        .fake_scheduler;
  }

  void VerifySchedulerInitialization() {
    // Verify scheduler input parameters.
    FakeNearbyShareSchedulerFactory::PeriodicInstance
        download_and_upload_scheduler_instance =
            scheduler_factory_.pref_name_to_periodic_instance().at(
                prefs::kNearbySharingSchedulerContactDownloadAndUploadPrefName);
    EXPECT_TRUE(download_and_upload_scheduler_instance.fake_scheduler);
    EXPECT_EQ(kContactDownloadPeriod,
              download_and_upload_scheduler_instance.request_period);
    EXPECT_TRUE(download_and_upload_scheduler_instance.retry_failures);
    EXPECT_TRUE(download_and_upload_scheduler_instance.require_connectivity);
    EXPECT_EQ(&pref_service_,
              download_and_upload_scheduler_instance.pref_service);
  }

  void TriggerScheduler() {
    // Fire scheduler and verify downloader creation.
    size_t num_downloaders = downloader_factory_.instances().size();
    download_and_upload_scheduler()->InvokeRequestCallback();
    EXPECT_EQ(num_downloaders + 1, downloader_factory_.instances().size());
    EXPECT_EQ(kContactDownloadRpcTimeout, downloader_factory_.latest_timeout());
    EXPECT_EQ(&http_client_factory_,
              downloader_factory_.latest_client_factory());
    EXPECT_EQ(local_device_data_manager_.GetId(),
              latest_downloader()->device_id());
  }

  void VerifyDownloadNotificationSent(
      size_t initial_num_notifications,
      const std::set<std::string>& expected_allowed_contact_ids,
      const std::vector<nearbyshare::proto::ContactRecord>& contacts) {
    EXPECT_EQ(initial_num_notifications + 1,
              contacts_downloaded_notifications_.size());
    EXPECT_EQ(expected_allowed_contact_ids,
              contacts_downloaded_notifications_.back().allowed_contact_ids);
    EXPECT_EQ(contacts.size(),
              contacts_downloaded_notifications_.back().contacts.size());
    for (size_t i = 0; i < contacts.size(); ++i) {
      EXPECT_EQ(contacts[i].SerializeAsString(),
                contacts_downloaded_notifications_.back()
                    .contacts[i]
                    .SerializeAsString());
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
                     /*expect_allowlist_changed=*/true);
  // Remove last allowed contact.
  SetAllowedContacts(TestContactIds(/*num_contacts=*/2u),
                     /*expect_allowlist_changed=*/true);
  // Add back last allowed contact.
  SetAllowedContacts(TestContactIds(/*num_contacts=*/3u),
                     /*expect_allowlist_changed=*/true);
  // Set list without any changes.
  SetAllowedContacts(TestContactIds(/*num_contacts=*/3u),
                     /*expect_allowlist_changed=*/false);
}

TEST_F(NearbyShareContactManagerImplTest, DownloadContacts_WithUpload) {
  std::vector<nearbyshare::proto::ContactRecord> contact_records =
      TestContactRecordList(/*num_contacts=*/3u);
  std::set<std::string> allowlist = TestContactIds(/*num_contacts=*/2u);
  SetAllowedContacts(allowlist, /*expect_allowlist_changed=*/true);

  // Because contacts have never been uploaded, a subsequent upload should be
  // requested, which succeeds.
  DownloadContacts();
  SucceedDownload(contact_records, allowlist, /*expect_upload=*/true);
  FinishUpload(/*success=*/true, /*expected_contacts=*/ContactRecordsToContacts(
                   allowlist, contact_records));

  // When contacts are downloaded again, we decect that contacts have not
  // changed, so no upload should be made
  DownloadContacts();
  SucceedDownload(contact_records, allowlist, /*expect_upload=*/false);
}

TEST_F(NearbyShareContactManagerImplTest,
       DownloadContacts_DetectContactListChanged) {
  std::vector<nearbyshare::proto::ContactRecord> contact_records =
      TestContactRecordList(/*num_contacts=*/3u);
  std::set<std::string> allowlist = TestContactIds(/*num_contacts=*/2u);
  SetAllowedContacts(allowlist, /*expect_allowlist_changed=*/true);

  // Because contacts have never been uploaded, a subsequent upload is
  // requested, which succeeds.
  DownloadContacts();
  SucceedDownload(contact_records, allowlist, /*expect_upload=*/true);
  FinishUpload(/*success=*/true, /*expected_contacts=*/ContactRecordsToContacts(
                   allowlist, contact_records));

  // When contacts are downloaded again, we decect that contacts have changed
  // since the last upload.
  contact_records = TestContactRecordList(/*num_contacts=*/4u);
  DownloadContacts();
  SucceedDownload(contact_records, allowlist, /*expect_upload=*/true);
  FinishUpload(/*success=*/true, /*expected_contacts=*/ContactRecordsToContacts(
                   allowlist, contact_records));
}

TEST_F(NearbyShareContactManagerImplTest,
       DownloadContacts_DetectAllowlistChanged) {
  std::vector<nearbyshare::proto::ContactRecord> contact_records =
      TestContactRecordList(/*num_contacts=*/3u);
  std::set<std::string> allowlist = TestContactIds(/*num_contacts=*/2u);
  SetAllowedContacts(allowlist, /*expect_allowlist_changed=*/true);

  // Because contacts have never been uploaded, a subsequent upload is
  // requested, which succeeds.
  DownloadContacts();
  SucceedDownload(contact_records, allowlist, /*expect_upload=*/true);
  FinishUpload(/*success=*/true, /*expected_contacts=*/ContactRecordsToContacts(
                   allowlist, contact_records));

  // When contacts are downloaded again, we decect that the allowlist has
  // changed since the last upload.
  allowlist = TestContactIds(/*num_contacts=*/1u);
  SetAllowedContacts(allowlist, /*expect_allowlist_changed=*/true);
  DownloadContacts();
  SucceedDownload(contact_records, allowlist, /*expect_upload=*/true);
  FinishUpload(/*success=*/true, /*expected_contacts=*/ContactRecordsToContacts(
                   allowlist, contact_records));
}

TEST_F(NearbyShareContactManagerImplTest, DownloadContacts_FailDownload) {
  DownloadContacts();
  FailDownload();
}

TEST_F(NearbyShareContactManagerImplTest, DownloadContacts_RetryFailedUpload) {
  std::vector<nearbyshare::proto::ContactRecord> contact_records =
      TestContactRecordList(/*num_contacts=*/3u);
  std::set<std::string> allowlist = TestContactIds(/*num_contacts=*/2u);
  SetAllowedContacts(allowlist, /*expect_allowlist_changed=*/true);

  // Because contacts have never been uploaded, a subsequent upload is
  // requested, which succeeds.
  DownloadContacts();
  SucceedDownload(contact_records, allowlist, /*expect_upload=*/true);
  FinishUpload(/*success=*/true, /*expected_contacts=*/ContactRecordsToContacts(
                   allowlist, contact_records));

  // When contacts are downloaded again, we decect that contacts have changed
  // since the last upload. Fail this upload.
  contact_records = TestContactRecordList(/*num_contacts=*/4u);
  DownloadContacts();
  SucceedDownload(contact_records, allowlist, /*expect_upload=*/true);
  FinishUpload(/*success=*/false,
               /*expected_contacts=*/ContactRecordsToContacts(allowlist,
                                                              contact_records));

  // When contacts are downloaded again, we should continue to indicate that
  // contacts have changed since the last upload, and attempt another upload.
  // (In other words, this tests that the contact-upload hash isn't updated
  // prematurely.)
  DownloadContacts();
  SucceedDownload(contact_records, allowlist, /*expect_upload=*/true);
  FinishUpload(/*success=*/true,
               /*expected_contacts=*/ContactRecordsToContacts(allowlist,
                                                              contact_records));
}

TEST_F(NearbyShareContactManagerImplTest, ContactUploadHash) {
  EXPECT_EQ(std::string(), pref_service()->GetString(
                               prefs::kNearbySharingContactUploadHashPrefName));

  std::vector<nearbyshare::proto::ContactRecord> contact_records =
      TestContactRecordList(/*num_contacts=*/3u);
  std::set<std::string> allowlist = TestContactIds(/*num_contacts=*/2u);
  SetAllowedContacts(allowlist, /*expect_allowlist_changed=*/true);
  DownloadContacts();
  SucceedDownload(contact_records, allowlist, /*expect_upload=*/true);
  FinishUpload(/*success=*/true, /*expected_contacts=*/ContactRecordsToContacts(
                   allowlist, contact_records));

  // Hardcode expected contact upload hash to ensure that hashed value is
  // consistent across process starts.
  EXPECT_EQ("DB408F2F01561308A97E9B6A1DB28536BEE33283D3E5B3842EFADAD4034E79DE",
            pref_service()->GetString(
                prefs::kNearbySharingContactUploadHashPrefName));
}
