// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/gtest_util.h"
#include "base/test/null_task_runner.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "chrome/browser/nearby_sharing/client/nearby_share_client.h"
#include "chrome/browser/nearby_sharing/client/nearby_share_client_impl.h"
#include "chrome/browser/nearby_sharing/client/nearby_share_http_notifier.h"
#include "chrome/browser/nearby_sharing/common/nearby_share_switches.h"
#include "chromeos/ash/components/nearby/common/client/nearby_api_call_flow.h"
#include "chromeos/ash/components/nearby/common/client/nearby_api_call_flow_impl.h"
#include "chromeos/ash/components/nearby/common/client/nearby_http_result.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/nearby/sharing/proto/certificate_rpc.pb.h"
#include "third_party/nearby/sharing/proto/contact_rpc.pb.h"
#include "third_party/nearby/sharing/proto/device_rpc.pb.h"
#include "third_party/nearby/sharing/proto/rpc_resources.pb.h"
#include "url/gurl.h"

namespace {

const char kGet[] = "GET";
const char kPost[] = "POST";
const char kPatch[] = "PATCH";
const char kAccessToken[] = "access_token";
const char kAccountName1[] = "accountname1";
const char kContactId1[] = "contactid1";
const char kDeviceIdPath[] = "users/me/devices/deviceid";
const char kEmail[] = "test@gmail.com";
const char kEncryptedMetadataBytes1[] = "encryptedmetadatabytes1";
const char kImageUrl1[] = "https://example.com/image.jpg";
const char kMetadataEncryptionKey1[] = "metadataencryptionkey1";
const char kMetadataEncryptionKeyTag1[] = "metadataencryptionkeytag1";
const char kObfuscatedGaia1[] = "obfuscatedgaia1";
const char kPageToken1[] = "pagetoken1";
const char kPageToken2[] = "pagetoken2";
const char kPersonName1[] = "personname1";
const char kPhoneNumber1[] = "1231231234";
const char kPublicKey1[] = "publickey1";
const char kSecretId1[] = "secretid1";
const char kSecretId2[] = "secretid2";
const char kSecretId1Encoded[] = "c2VjcmV0aWQx";
const char kSecretId2Encoded[] = "c2VjcmV0aWQy";
const char kSecretKey1[] = "secretkey1";
const char kTestGoogleApisUrl[] = "https://nearbysharing-pa.testgoogleapis.com";
const int32_t kNanos1 = 123123123;
const int32_t kNanos2 = 321321321;
const int32_t kPageSize1 = 1000;
const int64_t kSeconds1 = 1594392109;
const int64_t kSeconds2 = 1623336109;

class FakeNearbyShareApiCallFlow : public ash::nearby::NearbyApiCallFlow {
 public:
  FakeNearbyShareApiCallFlow() = default;
  ~FakeNearbyShareApiCallFlow() override = default;
  FakeNearbyShareApiCallFlow(FakeNearbyShareApiCallFlow&) = delete;
  FakeNearbyShareApiCallFlow& operator=(FakeNearbyShareApiCallFlow&) = delete;

  void StartPostRequest(
      const GURL& request_url,
      const std::string& serialized_request,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const std::string& access_token,
      ResultCallback&& result_callback,
      ErrorCallback&& error_callback) override {
    http_method_ = kPost;
    request_url_ = request_url;
    serialized_request_ = serialized_request;
    url_loader_factory_ = url_loader_factory;
    result_callback_ = std::move(result_callback);
    error_callback_ = std::move(error_callback);
    EXPECT_EQ(kAccessToken, access_token);
  }

  void StartPatchRequest(
      const GURL& request_url,
      const std::string& serialized_request,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const std::string& access_token,
      ResultCallback&& result_callback,
      ErrorCallback&& error_callback) override {
    http_method_ = kPatch;
    request_url_ = request_url;
    serialized_request_ = serialized_request;
    url_loader_factory_ = url_loader_factory;
    result_callback_ = std::move(result_callback);
    error_callback_ = std::move(error_callback);
    EXPECT_EQ(kAccessToken, access_token);
  }

  void StartGetRequest(
      const GURL& request_url,
      const QueryParameters& request_as_query_parameters,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const std::string& access_token,
      ResultCallback&& result_callback,
      ErrorCallback&& error_callback) override {
    http_method_ = kGet;
    request_url_ = request_url;
    request_as_query_parameters_ = request_as_query_parameters;
    url_loader_factory_ = url_loader_factory;
    result_callback_ = std::move(result_callback);
    error_callback_ = std::move(error_callback);
    EXPECT_EQ(kAccessToken, access_token);
  }

  void SetPartialNetworkTrafficAnnotation(
      const net::PartialNetworkTrafficAnnotationTag& partial_traffic_annotation)
      override {
    // Do nothing
  }

  std::string http_method_;
  GURL request_url_;
  std::string serialized_request_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  ResultCallback result_callback_;
  ErrorCallback error_callback_;
  QueryParameters request_as_query_parameters_;
};

// Return the values associated with |key|, or fail the test if |key| isn't in
// |query_parameters|
std::vector<std::string> ExpectQueryStringValues(
    const ash::nearby::NearbyApiCallFlow::QueryParameters& query_parameters,
    const std::string& key) {
  std::vector<std::string> values;
  for (const std::pair<std::string, std::string>& pair : query_parameters) {
    if (pair.first == key) {
      values.push_back(pair.second);
    }
  }
  EXPECT_TRUE(values.size() > 0);
  return values;
}

// Callback that should never be invoked.
template <class T>
void NotCalled(T type) {
  EXPECT_TRUE(false);
}

// Callback that should never be invoked.
template <class T>
void NotCalledConstRef(const T& type) {
  EXPECT_TRUE(false);
}

}  // namespace

class NearbyShareClientImplTest : public testing::Test,
                                  public NearbyShareHttpNotifier::Observer {
 protected:
  NearbyShareClientImplTest() {
    shared_factory_ =
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            base::BindOnce([]() -> network::mojom::URLLoaderFactory* {
              ADD_FAILURE() << "Did not expect this to actually be used";
              return nullptr;
            }));
  }

  void SetUp() override {
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        switches::kNearbyShareHTTPHost, kTestGoogleApisUrl);

    identity_test_environment_.MakePrimaryAccountAvailable(
        kEmail, signin::ConsentLevel::kSignin);

    std::unique_ptr<FakeNearbyShareApiCallFlow> api_call_flow =
        std::make_unique<FakeNearbyShareApiCallFlow>();
    api_call_flow_ = api_call_flow.get();

    client_ = std::make_unique<NearbyShareClientImpl>(
        std::move(api_call_flow), identity_test_environment_.identity_manager(),
        shared_factory_, &notifier_);

    notifier_.AddObserver(this);
  }

  void TearDown() override { notifier_.RemoveObserver(this); }

  // NearbyShareHttpNotifier::Observer:
  void OnUpdateDeviceRequest(
      const nearby::sharing::proto::UpdateDeviceRequest& request) override {
    update_device_request_from_notifier_ = request;
  }

  void OnUpdateDeviceResponse(
      const nearby::sharing::proto::UpdateDeviceResponse& response) override {
    update_device_response_from_notifier_ = response;
  }

  void OnListContactPeopleRequest(
      const nearby::sharing::proto::ListContactPeopleRequest& request)
      override {
    list_contact_people_request_from_notifier_ = request;
  }

  void OnListContactPeopleResponse(
      const nearby::sharing::proto::ListContactPeopleResponse& response)
      override {
    list_contact_people_response_from_notifier_ = response;
  }

  void OnListPublicCertificatesRequest(
      const nearby::sharing::proto::ListPublicCertificatesRequest& request)
      override {
    list_public_certificate_request_from_notifier_ = request;
  }

  void OnListPublicCertificatesResponse(
      const nearby::sharing::proto::ListPublicCertificatesResponse& response)
      override {
    list_public_certificate_response_from_notifier_ = response;
  }

  const std::string& http_method() { return api_call_flow_->http_method_; }
  const GURL& request_url() { return api_call_flow_->request_url_; }
  const std::string& serialized_request() {
    return api_call_flow_->serialized_request_;
  }
  const ash::nearby::NearbyApiCallFlow::QueryParameters&
  request_as_query_parameters() {
    return api_call_flow_->request_as_query_parameters_;
  }

  // Returns |response_proto| as the result to the current API request.
  void FinishApiCallFlow(const google::protobuf::MessageLite* response_proto) {
    std::move(api_call_flow_->result_callback_)
        .Run(response_proto->SerializeAsString());
  }

  // Returns |serialized_proto| as the result to the current API request.
  void FinishApiCallFlowRaw(const std::string& serialized_proto) {
    std::move(api_call_flow_->result_callback_).Run(serialized_proto);
  }

  // Ends the current API request with |error|.
  void FailApiCallFlow(ash::nearby::NearbyHttpError error) {
    std::move(api_call_flow_->error_callback_).Run(error);
  }

  void VerifyRequestNotification(
      const nearby::sharing::proto::UpdateDeviceRequest& expected_request)
      const {
    ASSERT_TRUE(update_device_request_from_notifier_);
    EXPECT_EQ(expected_request.SerializeAsString(),
              update_device_request_from_notifier_->SerializeAsString());
  }

  void VerifyResponseNotification(
      const nearby::sharing::proto::UpdateDeviceResponse& expected_response)
      const {
    ASSERT_TRUE(update_device_response_from_notifier_);
    EXPECT_EQ(expected_response.SerializeAsString(),
              update_device_response_from_notifier_->SerializeAsString());
  }

  void VerifyRequestNotification(
      const nearby::sharing::proto::ListContactPeopleRequest& expected_request)
      const {
    ASSERT_TRUE(list_contact_people_request_from_notifier_);
    EXPECT_EQ(expected_request.SerializeAsString(),
              list_contact_people_request_from_notifier_->SerializeAsString());
  }

  void VerifyResponseNotification(
      const nearby::sharing::proto::ListContactPeopleResponse&
          expected_response) const {
    ASSERT_TRUE(list_contact_people_response_from_notifier_);
    EXPECT_EQ(expected_response.SerializeAsString(),
              list_contact_people_response_from_notifier_->SerializeAsString());
  }

  void VerifyRequestNotification(
      const nearby::sharing::proto::ListPublicCertificatesRequest&
          expected_request) const {
    ASSERT_TRUE(list_public_certificate_request_from_notifier_);
    EXPECT_EQ(
        expected_request.SerializeAsString(),
        list_public_certificate_request_from_notifier_->SerializeAsString());
  }

  void VerifyResponseNotification(
      const nearby::sharing::proto::ListPublicCertificatesResponse&
          expected_response) const {
    ASSERT_TRUE(list_public_certificate_response_from_notifier_);
    EXPECT_EQ(
        expected_response.SerializeAsString(),
        list_public_certificate_response_from_notifier_->SerializeAsString());
  }

 protected:
  std::optional<nearby::sharing::proto::UpdateDeviceRequest>
      update_device_request_from_notifier_;
  std::optional<nearby::sharing::proto::UpdateDeviceResponse>
      update_device_response_from_notifier_;
  std::optional<nearby::sharing::proto::ListContactPeopleRequest>
      list_contact_people_request_from_notifier_;
  std::optional<nearby::sharing::proto::ListContactPeopleResponse>
      list_contact_people_response_from_notifier_;
  std::optional<nearby::sharing::proto::ListPublicCertificatesRequest>
      list_public_certificate_request_from_notifier_;
  std::optional<nearby::sharing::proto::ListPublicCertificatesResponse>
      list_public_certificate_response_from_notifier_;
  base::test::TaskEnvironment task_environment_;
  signin::IdentityTestEnvironment identity_test_environment_;
  raw_ptr<FakeNearbyShareApiCallFlow, DanglingUntriaged> api_call_flow_;
  scoped_refptr<network::SharedURLLoaderFactory> shared_factory_;
  NearbyShareHttpNotifier notifier_;
  std::unique_ptr<NearbyShareClient> client_;
};

TEST_F(NearbyShareClientImplTest, UpdateDeviceSuccess) {
  base::test::TestFuture<const nearby::sharing::proto::UpdateDeviceResponse&>
      future;
  nearby::sharing::proto::UpdateDeviceRequest request_proto;
  request_proto.mutable_device()->set_name(kDeviceIdPath);
  client_->UpdateDevice(
      request_proto, future.GetCallback(),
      base::BindOnce(&NotCalled<ash::nearby::NearbyHttpError>));
  identity_test_environment_
      .WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
          kAccessToken, base::Time::Max());

  VerifyRequestNotification(request_proto);

  EXPECT_EQ(kPatch, http_method());
  EXPECT_EQ(request_url(), GURL(std::string(kTestGoogleApisUrl) + "/v1/" +
                                std::string(kDeviceIdPath)));

  nearby::sharing::proto::UpdateDeviceRequest expected_request;
  EXPECT_TRUE(expected_request.ParseFromString(serialized_request()));
  EXPECT_EQ(kDeviceIdPath, expected_request.device().name());

  nearby::sharing::proto::UpdateDeviceResponse response_proto;
  nearby::sharing::proto::Device& device = *response_proto.mutable_device();

  device.set_name(kDeviceIdPath);
  device.add_contacts();
  device.mutable_contacts(0)->mutable_identifier()->set_phone_number(
      kPhoneNumber1);
  device.mutable_contacts(0)->set_is_selected(false);
  device.add_contacts();
  device.mutable_contacts(1)->mutable_identifier()->set_account_name(
      kAccountName1);
  device.mutable_contacts(1)->set_is_selected(true);
  device.add_contacts();
  device.mutable_contacts(2)->mutable_identifier()->set_obfuscated_gaia(
      kObfuscatedGaia1);
  device.mutable_contacts(2)->set_is_selected(true);

  FinishApiCallFlow(&response_proto);
  VerifyResponseNotification(response_proto);

  // Check that the result received in callback is the same as the response.
  nearby::sharing::proto::UpdateDeviceResponse result_proto = future.Take();
  ASSERT_EQ(3, result_proto.device().contacts_size());
  EXPECT_EQ(nearby::sharing::proto::Contact::Identifier::kPhoneNumber,
            result_proto.device().contacts(0).identifier().identifier_case());
  EXPECT_EQ(kPhoneNumber1,
            result_proto.device().contacts(0).identifier().phone_number());
  EXPECT_EQ(nearby::sharing::proto::Contact::Identifier::kAccountName,
            result_proto.device().contacts(1).identifier().identifier_case());
  EXPECT_EQ(kAccountName1,
            result_proto.device().contacts(1).identifier().account_name());
  EXPECT_EQ(nearby::sharing::proto::Contact::Identifier::kObfuscatedGaia,
            result_proto.device().contacts(2).identifier().identifier_case());
  EXPECT_EQ(kObfuscatedGaia1,
            result_proto.device().contacts(2).identifier().obfuscated_gaia());
}

TEST_F(NearbyShareClientImplTest, UpdateDeviceFailure) {
  nearby::sharing::proto::UpdateDeviceRequest request;
  request.mutable_device()->set_name(kDeviceIdPath);

  base::test::TestFuture<ash::nearby::NearbyHttpError> future;
  client_->UpdateDevice(
      request,
      base::BindOnce(
          &NotCalledConstRef<nearby::sharing::proto::UpdateDeviceResponse>),
      future.GetCallback());
  identity_test_environment_
      .WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
          kAccessToken, base::Time::Max());

  EXPECT_EQ(kPatch, http_method());
  EXPECT_EQ(request_url(), GURL(std::string(kTestGoogleApisUrl) + "/v1/" +
                                std::string(kDeviceIdPath)));

  FailApiCallFlow(ash::nearby::NearbyHttpError::kInternalServerError);
  EXPECT_EQ(ash::nearby::NearbyHttpError::kInternalServerError, future.Get());
}

TEST_F(NearbyShareClientImplTest, ListContactPeopleSuccess) {
  base::test::TestFuture<
      const nearby::sharing::proto::ListContactPeopleResponse&>
      future;
  nearby::sharing::proto::ListContactPeopleRequest request_proto;
  request_proto.set_page_size(kPageSize1);
  request_proto.set_page_token(kPageToken1);

  client_->ListContactPeople(
      request_proto, future.GetCallback(),
      base::BindOnce(&NotCalled<ash::nearby::NearbyHttpError>));
  identity_test_environment_
      .WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
          kAccessToken, base::Time::Max());

  VerifyRequestNotification(request_proto);

  EXPECT_EQ(kGet, http_method());
  EXPECT_EQ(request_url(),
            std::string(kTestGoogleApisUrl) + "/v1/contactRecords");

  EXPECT_EQ(
      std::vector<std::string>{base::NumberToString(kPageSize1)},
      ExpectQueryStringValues(request_as_query_parameters(), "page_size"));
  EXPECT_EQ(
      std::vector<std::string>{kPageToken1},
      ExpectQueryStringValues(request_as_query_parameters(), "page_token"));

  nearby::sharing::proto::ListContactPeopleResponse response_proto;
  response_proto.add_contact_records();
  response_proto.mutable_contact_records(0)->set_id(kContactId1);
  response_proto.mutable_contact_records(0)->set_person_name(kPersonName1);
  response_proto.mutable_contact_records(0)->set_image_url(kImageUrl1);
  response_proto.mutable_contact_records(0)->add_identifiers();
  response_proto.mutable_contact_records(0)
      ->mutable_identifiers(0)
      ->set_obfuscated_gaia(kObfuscatedGaia1);
  response_proto.set_next_page_token(kPageToken2);
  FinishApiCallFlow(&response_proto);
  VerifyResponseNotification(response_proto);

  nearby::sharing::proto::ListContactPeopleResponse result_proto =
      future.Take();
  EXPECT_EQ(1, result_proto.contact_records_size());
  EXPECT_EQ(kContactId1, result_proto.contact_records(0).id());
  EXPECT_EQ(kPersonName1, result_proto.contact_records(0).person_name());
  EXPECT_EQ(kImageUrl1, result_proto.contact_records(0).image_url());
  EXPECT_EQ(1, result_proto.contact_records(0).identifiers_size());
  EXPECT_EQ(nearby::sharing::proto::Contact::Identifier::IdentifierCase::
                kObfuscatedGaia,
            result_proto.contact_records(0).identifiers(0).identifier_case());
  EXPECT_EQ(kObfuscatedGaia1,
            result_proto.contact_records(0).identifiers(0).obfuscated_gaia());
}

TEST_F(NearbyShareClientImplTest, ListPublicCertificatesSuccess) {
  base::test::TestFuture<
      const nearby::sharing::proto::ListPublicCertificatesResponse&>
      future;
  nearby::sharing::proto::ListPublicCertificatesRequest request_proto;
  request_proto.set_parent(kDeviceIdPath);
  request_proto.set_page_size(kPageSize1);
  request_proto.set_page_token(kPageToken1);
  request_proto.add_secret_ids();
  request_proto.set_secret_ids(0, kSecretId1);
  request_proto.add_secret_ids();
  request_proto.set_secret_ids(1, kSecretId2);

  client_->ListPublicCertificates(
      request_proto, future.GetCallback(),
      base::BindOnce(&NotCalled<ash::nearby::NearbyHttpError>));
  identity_test_environment_
      .WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
          kAccessToken, base::Time::Max());

  VerifyRequestNotification(request_proto);

  EXPECT_EQ(kGet, http_method());
  EXPECT_EQ(request_url(), std::string(kTestGoogleApisUrl) + "/v1/" +
                               std::string(kDeviceIdPath) +
                               "/publicCertificates");

  EXPECT_EQ(
      std::vector<std::string>{base::NumberToString(kPageSize1)},
      ExpectQueryStringValues(request_as_query_parameters(), "page_size"));
  EXPECT_EQ(
      std::vector<std::string>{kPageToken1},
      ExpectQueryStringValues(request_as_query_parameters(), "page_token"));
  EXPECT_EQ(
      (std::vector<std::string>{kSecretId1Encoded, kSecretId2Encoded}),
      ExpectQueryStringValues(request_as_query_parameters(), "secret_ids"));

  nearby::sharing::proto::ListPublicCertificatesResponse response_proto;
  response_proto.set_next_page_token(kPageToken2);
  response_proto.add_public_certificates();
  response_proto.mutable_public_certificates(0)->set_secret_id(kSecretId1);
  response_proto.mutable_public_certificates(0)->set_secret_key(kSecretKey1);
  response_proto.mutable_public_certificates(0)->set_public_key(kPublicKey1);
  response_proto.mutable_public_certificates(0)
      ->mutable_start_time()
      ->set_seconds(kSeconds1);
  response_proto.mutable_public_certificates(0)
      ->mutable_start_time()
      ->set_nanos(kNanos1);
  response_proto.mutable_public_certificates(0)
      ->mutable_end_time()
      ->set_seconds(kSeconds2);
  response_proto.mutable_public_certificates(0)->mutable_end_time()->set_nanos(
      kNanos2);
  response_proto.mutable_public_certificates(0)->set_for_selected_contacts(
      false);
  response_proto.mutable_public_certificates(0)->set_metadata_encryption_key(
      kMetadataEncryptionKey1);
  response_proto.mutable_public_certificates(0)->set_encrypted_metadata_bytes(
      kEncryptedMetadataBytes1);
  response_proto.mutable_public_certificates(0)
      ->set_metadata_encryption_key_tag(kMetadataEncryptionKeyTag1);
  FinishApiCallFlow(&response_proto);
  VerifyResponseNotification(response_proto);

  nearby::sharing::proto::ListPublicCertificatesResponse result_proto =
      future.Take();
  EXPECT_EQ(kPageToken2, result_proto.next_page_token());
  EXPECT_EQ(1, result_proto.public_certificates_size());
  EXPECT_EQ(kSecretId1, result_proto.public_certificates(0).secret_id());
  EXPECT_EQ(kSecretKey1, result_proto.public_certificates(0).secret_key());
  EXPECT_EQ(kSeconds1,
            result_proto.public_certificates(0).start_time().seconds());
  EXPECT_EQ(kNanos1, result_proto.public_certificates(0).start_time().nanos());
  EXPECT_EQ(kSeconds2,
            result_proto.public_certificates(0).end_time().seconds());
  EXPECT_EQ(kNanos2, result_proto.public_certificates(0).end_time().nanos());
  EXPECT_EQ(false, result_proto.public_certificates(0).for_selected_contacts());
  EXPECT_EQ(kMetadataEncryptionKey1,
            result_proto.public_certificates(0).metadata_encryption_key());
  EXPECT_EQ(kEncryptedMetadataBytes1,
            result_proto.public_certificates(0).encrypted_metadata_bytes());
  EXPECT_EQ(kMetadataEncryptionKeyTag1,
            result_proto.public_certificates(0).metadata_encryption_key_tag());
}

TEST_F(NearbyShareClientImplTest, FetchAccessTokenFailure) {
  base::test::TestFuture<ash::nearby::NearbyHttpError> future;
  client_->UpdateDevice(
      nearby::sharing::proto::UpdateDeviceRequest(),
      base::BindOnce(
          &NotCalledConstRef<nearby::sharing::proto::UpdateDeviceResponse>),
      future.GetCallback());
  identity_test_environment_
      .WaitForAccessTokenRequestIfNecessaryAndRespondWithError(
          GoogleServiceAuthError(GoogleServiceAuthError::SERVICE_UNAVAILABLE));

  EXPECT_EQ(ash::nearby::NearbyHttpError::kAuthenticationError, future.Get());
}

TEST_F(NearbyShareClientImplTest, ParseResponseProtoFailure) {
  nearby::sharing::proto::UpdateDeviceRequest request_proto;
  request_proto.mutable_device()->set_name(kDeviceIdPath);

  base::test::TestFuture<ash::nearby::NearbyHttpError> future;
  client_->UpdateDevice(
      request_proto,
      base::BindOnce(
          &NotCalledConstRef<nearby::sharing::proto::UpdateDeviceResponse>),
      future.GetCallback());
  identity_test_environment_
      .WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
          kAccessToken, base::Time::Max());

  EXPECT_EQ(kPatch, http_method());
  EXPECT_EQ(request_url(), std::string(kTestGoogleApisUrl) + "/v1/" +
                               std::string(kDeviceIdPath));

  FinishApiCallFlowRaw("Not a valid serialized response message.");
  EXPECT_EQ(ash::nearby::NearbyHttpError::kResponseMalformed, future.Get());
}

TEST_F(NearbyShareClientImplTest, MakeSecondRequestBeforeFirstRequestSucceeds) {
  nearby::sharing::proto::UpdateDeviceRequest request_proto;
  request_proto.mutable_device()->set_name(kDeviceIdPath);

  // Make first request.
  base::test::TestFuture<const nearby::sharing::proto::UpdateDeviceResponse&>
      future;
  client_->UpdateDevice(
      request_proto, future.GetCallback(),
      base::BindOnce(&NotCalled<ash::nearby::NearbyHttpError>));
  identity_test_environment_
      .WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
          kAccessToken, base::Time::Max());

  EXPECT_EQ(kPatch, http_method());
  EXPECT_EQ(request_url(), std::string(kTestGoogleApisUrl) + "/v1/" +
                               std::string(kDeviceIdPath));

  // With request pending, make second request.
  {
    base::test::TestFuture<ash::nearby::NearbyHttpError> future2;
    EXPECT_DCHECK_DEATH(client_->ListPublicCertificates(
        nearby::sharing::proto::ListPublicCertificatesRequest(),
        base::BindOnce(&NotCalledConstRef<
                       nearby::sharing::proto::ListPublicCertificatesResponse>),
        future2.GetCallback()));
  }

  // Complete first request.
  {
    nearby::sharing::proto::UpdateDeviceResponse response_proto;
    response_proto.mutable_device()->set_name(kDeviceIdPath);
    FinishApiCallFlow(&response_proto);
  }

  nearby::sharing::proto::UpdateDeviceResponse result_proto = future.Take();
  EXPECT_EQ(kDeviceIdPath, result_proto.device().name());
}

TEST_F(NearbyShareClientImplTest, MakeSecondRequestAfterFirstRequestSucceeds) {
  // Make first request successfully.
  {
    base::test::TestFuture<const nearby::sharing::proto::UpdateDeviceResponse&>
        future;
    nearby::sharing::proto::UpdateDeviceRequest request_proto;
    request_proto.mutable_device()->set_name(kDeviceIdPath);

    client_->UpdateDevice(
        request_proto, future.GetCallback(),
        base::BindOnce(&NotCalled<ash::nearby::NearbyHttpError>));
    identity_test_environment_
        .WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
            kAccessToken, base::Time::Max());

    EXPECT_EQ(kPatch, http_method());
    EXPECT_EQ(request_url(), std::string(kTestGoogleApisUrl) + "/v1/" +
                                 std::string(kDeviceIdPath));

    nearby::sharing::proto::UpdateDeviceResponse response_proto;
    response_proto.mutable_device()->set_name(kDeviceIdPath);
    FinishApiCallFlow(&response_proto);
    nearby::sharing::proto::UpdateDeviceResponse result_proto = future.Take();
    EXPECT_EQ(kDeviceIdPath, result_proto.device().name());
  }

  // Second request fails.
  {
    base::test::TestFuture<ash::nearby::NearbyHttpError> future;
    EXPECT_DCHECK_DEATH(client_->ListPublicCertificates(
        nearby::sharing::proto::ListPublicCertificatesRequest(),
        base::BindOnce(&NotCalledConstRef<
                       nearby::sharing::proto::ListPublicCertificatesResponse>),
        future.GetCallback()));
  }
}

TEST_F(NearbyShareClientImplTest, GetAccessTokenUsed) {
  EXPECT_TRUE(client_->GetAccessTokenUsed().empty());

  base::test::TestFuture<const nearby::sharing::proto::UpdateDeviceResponse&>
      future;
  nearby::sharing::proto::UpdateDeviceRequest request_proto;
  request_proto.mutable_device()->set_name(kDeviceIdPath);

  client_->UpdateDevice(
      request_proto, future.GetCallback(),
      base::BindOnce(&NotCalled<ash::nearby::NearbyHttpError>));
  identity_test_environment_
      .WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
          kAccessToken, base::Time::Max());

  EXPECT_EQ(kPatch, http_method());
  EXPECT_EQ(request_url(), std::string(kTestGoogleApisUrl) + "/v1/" +
                               std::string(kDeviceIdPath));

  EXPECT_EQ(kAccessToken, client_->GetAccessTokenUsed());
}
