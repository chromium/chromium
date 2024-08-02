// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/nearby_sharing/contacts/nearby_share_contact_manager_impl.h"

#include <algorithm>
#include <memory>
#include <optional>
#include <random>
#include <set>
#include <string>

#include "base/containers/contains.h"
#include "base/containers/flat_set.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/nearby_sharing/client/fake_nearby_share_client.h"
#include "chrome/browser/nearby_sharing/common/fake_nearby_share_profile_info_provider.h"
#include "chrome/browser/nearby_sharing/common/nearby_share_prefs.h"
#include "chrome/browser/nearby_sharing/contacts/fake_nearby_share_contact_downloader.h"
#include "chrome/browser/nearby_sharing/contacts/nearby_share_contact_downloader.h"
#include "chrome/browser/nearby_sharing/contacts/nearby_share_contact_downloader_impl.h"
#include "chrome/browser/nearby_sharing/contacts/nearby_share_contact_manager.h"
#include "chrome/browser/nearby_sharing/contacts/nearby_share_contacts_sorter.h"
#include "chrome/browser/nearby_sharing/local_device_data/fake_nearby_share_local_device_data_manager.h"
#include "chromeos/ash/components/nearby/common/scheduling/fake_nearby_scheduler.h"
#include "chromeos/ash/components/nearby/common/scheduling/fake_nearby_scheduler_factory.h"
#include "chromeos/ash/components/nearby/common/scheduling/nearby_scheduler_factory.h"
#include "chromeos/ash/services/nearby/public/mojom/nearby_share_settings.mojom-test-utils.h"
#include "chromeos/ash/services/nearby/public/mojom/nearby_share_settings.mojom.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/nearby/sharing/proto/rpc_resources.pb.h"

namespace {

const uint32_t kTestNumUnreachableContactsFilteredOut = 123;
const char kTestContactIdPrefix[] = "id_";
const char kTestContactEmailPrefix[] = "email_";
const char kTestContactPhonePrefix[] = "phone_";
const char kTestDefaultDeviceName[] = "Josh's Chromebook";
const char kTestProfileUserName[] = "test@google.com";
const char* kTestPersonNames[] = {"BBB BBB", "CCC CCC", "AAA AAA"};

// From nearby_share_contact_manager_impl.cc.
constexpr base::TimeDelta kContactUploadPeriod = base::Hours(24);
constexpr base::TimeDelta kContactDownloadPeriod = base::Hours(12);
constexpr base::TimeDelta kContactDownloadRpcTimeout = base::Seconds(60);

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

std::vector<nearby::sharing::proto::ContactRecord> TestContactRecordList(
    size_t num_contacts) {
  std::vector<nearby::sharing::proto::ContactRecord> contact_list;
  for (size_t i = 0; i < num_contacts; ++i) {
    nearby::sharing::proto::ContactRecord contact;
    contact.set_id(GetTestContactId(i));
    contact.set_image_url("https://www.google.com/");
    contact.set_person_name(kTestPersonNames[i % 3]);
    contact.set_is_reachable(true);
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
// list of Contact protos. To enable self-sharing across devices, we expect the
// local device to include itself in the contact list as an allowed contact.
// Partially from nearby_share_contact_manager_impl.cc.
std::vector<nearby::sharing::proto::Contact> BuildContactListToUpload(
    const std::set<std::string>& allowed_contact_ids,
    const std::vector<nearby::sharing::proto::ContactRecord>& contact_records) {
  std::vector<nearby::sharing::proto::Contact> contacts;
  for (const auto& contact_record : contact_records) {
    bool is_selected = base::Contains(allowed_contact_ids, contact_record.id());
    for (const auto& identifier : contact_record.identifiers()) {
      nearby::sharing::proto::Contact contact;
      contact.mutable_identifier()->CopyFrom(identifier);
      contact.set_is_selected(is_selected);
      contacts.push_back(contact);
    }
  }

  // Add self to list of contacts.
  nearby::sharing::proto::Contact contact;
  contact.mutable_identifier()->set_account_name(kTestProfileUserName);
  contact.set_is_selected(true);
  contacts.push_back(contact);

  return contacts;
}

std::vector<nearby::sharing::proto::ContactRecord> MojoContactsToProto(
    const std::vector<nearby_share::mojom::ContactRecordPtr>& mojo_contacts) {
  std::vector<nearby::sharing::proto::ContactRecord> proto_contacts;
  proto_contacts.reserve(mojo_contacts.size());
  for (const nearby_share::mojom::ContactRecordPtr& mojo_contact :
       mojo_contacts) {
    nearby::sharing::proto::ContactRecord proto_contact;
    proto_contact.set_id(mojo_contact->id);
    proto_contact.set_person_name(mojo_contact->person_name);
    proto_contact.set_image_url(mojo_contact->image_url.spec());
    proto_contact.set_is_reachable(true);
    for (const auto& identifier : mojo_contact->identifiers) {
      auto* proto_identifier = proto_contact.add_identifiers();
      switch (identifier->which()) {
        case nearby_share::mojom::ContactIdentifier::Tag::kAccountName:
          proto_identifier->set_account_name(identifier->get_account_name());
          break;
        case nearby_share::mojom::ContactIdentifier::Tag::kObfuscatedGaia:
          proto_identifier->set_obfuscated_gaia(
              identifier->get_obfuscated_gaia());
          break;
        case nearby_share::mojom::ContactIdentifier::Tag::kPhoneNumber:
          proto_identifier->set_phone_number(identifier->get_phone_number());
          break;
      }
    }
    proto_contacts.push_back(proto_contact);
  }
  return proto_contacts;
}

void VerifyDownloadNotificationContacts(
    const std::set<std::string>& expected_allowed_contact_ids,
    const std::vector<nearby::sharing::proto::ContactRecord>&
        expected_unordered_contacts,
    const std::set<std::string>& notification_allowed_contact_ids,
    const std::vector<nearby::sharing::proto::ContactRecord>&
        notification_contacts) {
  EXPECT_EQ(expected_allowed_contact_ids, notification_allowed_contact_ids);
  EXPECT_EQ(expected_unordered_contacts.size(), notification_contacts.size());

  // Verify that observers receive contacts in sorted order.
  std::vector<nearby::sharing::proto::ContactRecord> expected_ordered_contacts =
      expected_unordered_contacts;
  SortNearbyShareContactRecords(&expected_ordered_contacts);
  for (size_t i = 0; i < expected_ordered_contacts.size(); ++i) {
    EXPECT_EQ(expected_ordered_contacts[i].SerializeAsString(),
              notification_contacts[i].SerializeAsString());
  }
}

class TestDownloadContactsObserver
    : public nearby_share::mojom::DownloadContactsObserver {
 public:
  void OnContactsDownloaded(
      const std::vector<std::string>& allowed_contacts,
      std::vector<nearby_share::mojom::ContactRecordPtr> contacts,
      uint32_t num_unreachable_contacts_filtered_out) override {
    allowed_contacts_ = allowed_contacts;
    contacts_ = std::move(contacts);
    num_unreachable_contacts_filtered_out_ =
        num_unreachable_contacts_filtered_out;
    on_contacts_downloaded_called_ = true;
  }

  void OnContactsDownloadFailed() override {
    on_contacts_download_failed_called_ = true;
  }

  std::vector<std::string> allowed_contacts_;
  std::vector<nearby_share::mojom::ContactRecordPtr> contacts_;
  uint32_t num_unreachable_contacts_filtered_out_;
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
    std::vector<nearby::sharing::proto::ContactRecord> contacts;
    uint32_t num_unreachable_contacts_filtered_out;
  };
  struct ContactsUploadedNotification {
    bool did_contacts_change_since_last_upload;
  };

  NearbyShareContactManagerImplTest()
      : local_device_data_manager_(kTestDefaultDeviceName) {}

  ~NearbyShareContactManagerImplTest() override = default;

  void SetUp() override {
    RegisterNearbySharingPrefs(pref_service_.registry());
    ash::nearby::NearbySchedulerFactory::SetFactoryForTesting(
        &scheduler_factory_);
    NearbyShareContactDownloaderImpl::Factory::SetFactoryForTesting(
        &downloader_factory_);
    profile_info_provider_.set_profile_user_name(kTestProfileUserName);

    manager_ = NearbyShareContactManagerImpl::Factory::Create(
        &pref_service_, &http_client_factory_, &local_device_data_manager_,
        &profile_info_provider_);
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
    ash::nearby::NearbySchedulerFactory::SetFactoryForTesting(nullptr);
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
      const std::vector<nearby::sharing::proto::ContactRecord>& contacts,
      const std::set<std::string>& expected_allowed_contact_ids,
      bool expect_upload) {
    TriggerDownloadScheduler();

    size_t num_handled_results =
        download_and_upload_scheduler()->handled_results().size();
    size_t num_download_notifications =
        contacts_downloaded_notifications_.size();
    size_t num_upload_contacts_calls =
        local_device_data_manager_.upload_contacts_calls().size();

    latest_downloader()->Succeed(contacts,
                                 kTestNumUnreachableContactsFilteredOut);

    VerifyDownloadNotificationSent(
        /*initial_num_notifications=*/num_download_notifications,
        expected_allowed_contact_ids, contacts);

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
    TriggerDownloadScheduler();

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

  void MakePeriodicUploadRequest() {
    periodic_upload_scheduler()->InvokeRequestCallback();
    periodic_upload_scheduler()->SetIsWaitingForResult(true);
  }

  void FinishUpload(
      bool success,
      const std::vector<nearby::sharing::proto::Contact>& expected_contacts) {
    FakeNearbyShareLocalDeviceDataManager::UploadContactsCall& call =
        local_device_data_manager_.upload_contacts_calls().back();

    // Ordering doesn't matter. Otherwise, because of internal sorting,
    // comparison would be difficult.
    ASSERT_EQ(expected_contacts.size(), call.contacts.size());
    base::flat_set<std::string> expected_contacts_set;
    base::flat_set<std::string> call_contacts_set;
    for (size_t i = 0; i < expected_contacts.size(); ++i) {
      expected_contacts_set.insert(expected_contacts[i].SerializeAsString());
      call_contacts_set.insert(call.contacts[i].SerializeAsString());
    }

    // Invoke upload callback from local device data manager.
    size_t num_upload_notifications = contacts_uploaded_notifications_.size();
    size_t num_download_and_upload_handled_results =
        download_and_upload_scheduler()->handled_results().size();
    size_t num_periodic_upload_handeled_results =
        periodic_upload_scheduler()->handled_results().size();
    std::move(call.callback).Run(success);

    // Verify upload notification was sent on success.
    EXPECT_EQ(num_upload_notifications + (success ? 1 : 0),
              contacts_uploaded_notifications_.size());
    if (success) {
      // We only expect uploads to occur if contacts have changed since the last
      // upload or is a periodic upload was requested.
      EXPECT_TRUE(contacts_uploaded_notifications_.back()
                      .did_contacts_change_since_last_upload ||
                  periodic_upload_scheduler()->IsWaitingForResult());

      if (periodic_upload_scheduler()->IsWaitingForResult()) {
        EXPECT_EQ(num_periodic_upload_handeled_results + 1,
                  periodic_upload_scheduler()->handled_results().size());
        EXPECT_TRUE(periodic_upload_scheduler()->handled_results().back());
        periodic_upload_scheduler()->SetIsWaitingForResult(false);
      } else {
        EXPECT_EQ(num_periodic_upload_handeled_results,
                  periodic_upload_scheduler()->handled_results().size());
      }
    }

    // Verify that result is sent to download/upload scheduler.
    EXPECT_EQ(num_download_and_upload_handled_results + 1,
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
      const std::vector<nearby::sharing::proto::ContactRecord>& contacts,
      uint32_t num_unreachable_contacts_filtered_out) override {
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

  ash::nearby::FakeNearbyScheduler* periodic_upload_scheduler() {
    return scheduler_factory_.pref_name_to_periodic_instance()
        .at(prefs::kNearbySharingSchedulerPeriodicContactUploadPrefName)
        .fake_scheduler;
  }

  ash::nearby::FakeNearbyScheduler* download_and_upload_scheduler() {
    return scheduler_factory_.pref_name_to_periodic_instance()
        .at(prefs::kNearbySharingSchedulerContactDownloadAndUploadPrefName)
        .fake_scheduler;
  }

  // Verify scheduler input parameters.
  void VerifySchedulerInitialization() {
    ash::nearby::FakeNearbySchedulerFactory::PeriodicInstance
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

    ash::nearby::FakeNearbySchedulerFactory::PeriodicInstance
        periodic_upload_scheduler_instance =
            scheduler_factory_.pref_name_to_periodic_instance().at(
                prefs::kNearbySharingSchedulerPeriodicContactUploadPrefName);
    EXPECT_TRUE(periodic_upload_scheduler_instance.fake_scheduler);
    EXPECT_EQ(kContactUploadPeriod,
              periodic_upload_scheduler_instance.request_period);
    EXPECT_FALSE(periodic_upload_scheduler_instance.retry_failures);
    EXPECT_TRUE(periodic_upload_scheduler_instance.require_connectivity);
    EXPECT_EQ(&pref_service_, periodic_upload_scheduler_instance.pref_service);
  }

  void TriggerDownloadScheduler() {
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
      const std::vector<nearby::sharing::proto::ContactRecord>&
          expected_unordered_contacts) {
    EXPECT_EQ(initial_num_notifications + 1,
              contacts_downloaded_notifications_.size());

    // Verify notification sent to regular (not mojo) observers.
    VerifyDownloadNotificationContacts(
        expected_allowed_contact_ids, expected_unordered_contacts,
        contacts_downloaded_notifications_.back().allowed_contact_ids,
        contacts_downloaded_notifications_.back().contacts);

    // Verify notification sent to mojo observers.
    mojo_observer_.receiver_.FlushForTesting();
    EXPECT_TRUE(mojo_observer_.on_contacts_downloaded_called_);
    EXPECT_FALSE(mojo_observer_.on_contacts_download_failed_called_);
    VerifyDownloadNotificationContacts(
        expected_allowed_contact_ids, expected_unordered_contacts,
        std::set<std::string>(mojo_observer_.allowed_contacts_.begin(),
                              mojo_observer_.allowed_contacts_.end()),
        MojoContactsToProto(mojo_observer_.contacts_));
  }

  TestDownloadContactsObserver mojo_observer_;
  std::vector<AllowlistChangedNotification> allowlist_changed_notifications_;
  std::vector<ContactsDownloadedNotification>
      contacts_downloaded_notifications_;
  std::vector<ContactsUploadedNotification> contacts_uploaded_notifications_;
  sync_preferences::TestingPrefServiceSyncable pref_service_;
  FakeNearbyShareClientFactory http_client_factory_;
  FakeNearbyShareLocalDeviceDataManager local_device_data_manager_;
  FakeNearbyShareProfileInfoProvider profile_info_provider_;
  ash::nearby::FakeNearbySchedulerFactory scheduler_factory_;
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

TEST_F(NearbyShareContactManagerImplTest, DownloadContacts_WithFirstUpload) {
  std::vector<nearby::sharing::proto::ContactRecord> contact_records =
      TestContactRecordList(/*num_contacts=*/4u);
  std::set<std::string> allowlist = TestContactIds(/*num_contacts=*/2u);
  SetAllowedContacts(allowlist, /*expect_allowlist_changed=*/true);

  // Because contacts have never been uploaded, a subsequent upload should be
  // requested, which succeeds.
  DownloadContacts();
  SucceedDownload(contact_records, allowlist, /*expect_upload=*/true);
  FinishUpload(/*success=*/true,
               /*expected_contacts=*/BuildContactListToUpload(allowlist,
                                                              contact_records));

  // When contacts are downloaded again, we decect that contacts have not
  // changed, so no upload should be made
  DownloadContacts();
  SucceedDownload(contact_records, allowlist, /*expect_upload=*/false);
}

TEST_F(NearbyShareContactManagerImplTest,
       DownloadContacts_DetectContactListChanged) {
  std::vector<nearby::sharing::proto::ContactRecord> contact_records =
      TestContactRecordList(/*num_contacts=*/3u);
  std::set<std::string> allowlist = TestContactIds(/*num_contacts=*/2u);
  SetAllowedContacts(allowlist, /*expect_allowlist_changed=*/true);

  // Because contacts have never been uploaded, a subsequent upload is
  // requested, which succeeds.
  DownloadContacts();
  SucceedDownload(contact_records, allowlist, /*expect_upload=*/true);
  FinishUpload(/*success=*/true,
               /*expected_contacts=*/BuildContactListToUpload(allowlist,
                                                              contact_records));

  // When contacts are downloaded again, we decect that contacts have changed
  // since the last upload.
  contact_records = TestContactRecordList(/*num_contacts=*/4u);
  DownloadContacts();
  SucceedDownload(contact_records, allowlist, /*expect_upload=*/true);
  FinishUpload(/*success=*/true,
               /*expected_contacts=*/BuildContactListToUpload(allowlist,
                                                              contact_records));
}

TEST_F(NearbyShareContactManagerImplTest,
       DownloadContacts_DetectAllowlistChanged) {
  std::vector<nearby::sharing::proto::ContactRecord> contact_records =
      TestContactRecordList(/*num_contacts=*/3u);
  std::set<std::string> allowlist = TestContactIds(/*num_contacts=*/2u);
  SetAllowedContacts(allowlist, /*expect_allowlist_changed=*/true);

  // Because contacts have never been uploaded, a subsequent upload is
  // requested, which succeeds.
  DownloadContacts();
  SucceedDownload(contact_records, allowlist, /*expect_upload=*/true);
  FinishUpload(/*success=*/true,
               /*expected_contacts=*/BuildContactListToUpload(allowlist,
                                                              contact_records));

  // When contacts are downloaded again, we decect that the allowlist has
  // changed since the last upload.
  allowlist = TestContactIds(/*num_contacts=*/1u);
  SetAllowedContacts(allowlist, /*expect_allowlist_changed=*/true);
  DownloadContacts();
  SucceedDownload(contact_records, allowlist, /*expect_upload=*/true);
  FinishUpload(/*success=*/true,
               /*expected_contacts=*/BuildContactListToUpload(allowlist,
                                                              contact_records));
}

TEST_F(NearbyShareContactManagerImplTest,
       DownloadContacts_PeriodicUploadRequest) {
  std::vector<nearby::sharing::proto::ContactRecord> contact_records =
      TestContactRecordList(/*num_contacts=*/3u);
  std::set<std::string> allowlist = TestContactIds(/*num_contacts=*/2u);
  SetAllowedContacts(allowlist, /*expect_allowlist_changed=*/true);

  // Because contacts have never been uploaded, a subsequent upload is
  // requested, which succeeds.
  DownloadContacts();
  SucceedDownload(contact_records, allowlist, /*expect_upload=*/true);
  FinishUpload(/*success=*/true, /*expected_contacts=*/BuildContactListToUpload(
                   allowlist, contact_records));

  // Because device records on the server will be removed after a few days if
  // the device does not contact the server, we ensure that contacts are
  // uploaded periodically. Make that request now. Contacts will be uploaded
  // after the next contact download. It will not force a download now, however.
  MakePeriodicUploadRequest();

  // When contacts are downloaded again, we decect that contacts have not
  // changed. However, we expect an upload because a periodic request was made.
  DownloadContacts();
  SucceedDownload(contact_records, allowlist, /*expect_upload=*/true);
}

TEST_F(NearbyShareContactManagerImplTest, DownloadContacts_FailDownload) {
  DownloadContacts();
  FailDownload();
}

TEST_F(NearbyShareContactManagerImplTest, DownloadContacts_RetryFailedUpload) {
  std::vector<nearby::sharing::proto::ContactRecord> contact_records =
      TestContactRecordList(/*num_contacts=*/3u);
  std::set<std::string> allowlist = TestContactIds(/*num_contacts=*/2u);
  SetAllowedContacts(allowlist, /*expect_allowlist_changed=*/true);

  // Because contacts have never been uploaded, a subsequent upload is
  // requested, which succeeds.
  DownloadContacts();
  SucceedDownload(contact_records, allowlist, /*expect_upload=*/true);
  FinishUpload(/*success=*/true,
               /*expected_contacts=*/BuildContactListToUpload(allowlist,
                                                              contact_records));

  // When contacts are downloaded again, we decect that contacts have changed
  // since the last upload. Fail this upload.
  contact_records = TestContactRecordList(/*num_contacts=*/4u);
  DownloadContacts();
  SucceedDownload(contact_records, allowlist, /*expect_upload=*/true);
  FinishUpload(/*success=*/false,
               /*expected_contacts=*/BuildContactListToUpload(allowlist,
                                                              contact_records));

  // When contacts are downloaded again, we should continue to indicate that
  // contacts have changed since the last upload, and attempt another upload.
  // (In other words, this tests that the contact-upload hash isn't updated
  // prematurely.)
  DownloadContacts();
  SucceedDownload(contact_records, allowlist, /*expect_upload=*/true);
  FinishUpload(/*success=*/true,
               /*expected_contacts=*/BuildContactListToUpload(allowlist,
                                                              contact_records));
}

TEST_F(NearbyShareContactManagerImplTest, ContactUploadHash) {
  EXPECT_EQ(std::string(), pref_service()->GetString(
                               prefs::kNearbySharingContactUploadHashPrefName));

  std::vector<nearby::sharing::proto::ContactRecord> contact_records =
      TestContactRecordList(/*num_contacts=*/10u);
  std::set<std::string> allowlist = TestContactIds(/*num_contacts=*/2u);
  SetAllowedContacts(allowlist, /*expect_allowlist_changed=*/true);
  DownloadContacts();
  SucceedDownload(contact_records, allowlist, /*expect_upload=*/true);
  FinishUpload(/*success=*/true,
               /*expected_contacts=*/BuildContactListToUpload(allowlist,
                                                              contact_records));

  // Hardcode expected contact upload hash to ensure that hashed value is
  // consistent across process starts. If this test starts to fail, check one of
  // the following:
  //   1. Did the test data change? No worries; just update this hash value.
  //   2. Did the hashing function change? As long as the function is stable
  //      across (most) process starts, then everything is okay; just update
  //      this hash value. A changed hash value will result in an extra server
  //      call, so as long as the value is stable for the most part, it's okay.
  const char kExpectedHash[] =
      "2355E1D4DF5B40CE03373D1EE590FE28D2A47B8B6F6EC4567CB770668A2DFC07";
  EXPECT_EQ(kExpectedHash, pref_service()->GetString(
                               prefs::kNearbySharingContactUploadHashPrefName));

  // Try a few different permutations of contacts to ensure that the hash is
  // invariant under ordering.
  auto rng = std::default_random_engine{};
  for (size_t i = 0; i < 10u; ++i) {
    DownloadContacts();

    // We do not expect an upload because the contacts did not change in any way
    // other than ordering.
    std::vector<nearby::sharing::proto::ContactRecord> shuffled_contacts =
        contact_records;
    std::shuffle(shuffled_contacts.begin(), shuffled_contacts.end(), rng);
    SucceedDownload(shuffled_contacts, allowlist, /*expect_upload=*/false);
    EXPECT_EQ(kExpectedHash,
              pref_service()->GetString(
                  prefs::kNearbySharingContactUploadHashPrefName));
  }
}
