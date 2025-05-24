// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <optional>
#include <set>
#include <string>
#include <vector>

#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/logging.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"
#include "base/uuid.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "components/data_sharing/public/group_data.h"
#include "components/sync/base/data_type.h"
#include "components/sync/base/time.h"
#include "components/sync/model/data_type_sync_bridge.h"
#include "components/sync/model/in_memory_metadata_change_list.h"
#include "components/sync/model/mutable_data_batch.h"
#include "components/sync/protocol/collaboration_group_specifics.pb.h"
#include "components/sync/protocol/entity_data.h"
#include "components/sync/protocol/entity_specifics.pb.h"
#include "components/sync/protocol/sync_entity.pb.h"
#include "components/sync/protocol/sync_enums.pb.h"
#include "components/sync/service/sync_service_impl.h"
#include "components/sync/test/bookmark_entity_builder.h"
#include "components/sync/test/collaboration_group_util.h"
#include "components/sync/test/entity_builder_factory.h"
#include "components/sync/test/fake_server.h"
#include "components/sync/test/fake_server_network_resources.h"
#include "components/sync/test/fake_server_nigori_helper.h"
#include "components/sync/test/fake_server_verifier.h"
#include "components/sync/test/nigori_test_utils.h"
#include "components/sync_device_info/device_info_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/android/gurl_android.h"
#include "url/gurl.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/android/test_support_jni_headers/FakeServerHelper_jni.h"

using base::android::JavaParamRef;

namespace {

void DeserializeEntity(JNIEnv* env,
                       jbyteArray serialized_entity,
                       sync_pb::SyncEntity* entity) {
  int bytes_length = env->GetArrayLength(serialized_entity);
  jbyte* bytes = env->GetByteArrayElements(serialized_entity, nullptr);
  std::string string(reinterpret_cast<char*>(bytes), bytes_length);

  bool success = entity->ParseFromString(string);
  DCHECK(success) << "Could not deserialize Entity";
}

void DeserializeEntitySpecifics(
    JNIEnv* env,
    const JavaParamRef<jbyteArray>& serialized_entity_specifics,
    sync_pb::EntitySpecifics* entity_specifics) {
  std::string specifics_string;
  base::android::JavaByteArrayToString(env, serialized_entity_specifics,
                                       &specifics_string);

  bool success = entity_specifics->ParseFromString(specifics_string);
  DCHECK(success) << "Could not deserialize EntitySpecifics";
}

std::unique_ptr<syncer::LoopbackServerEntity> CreateBookmarkEntity(
    JNIEnv* env,
    std::string title,
    const base::android::JavaRef<jobject>& url,
    std::optional<std::string> guid,
    std::string parent_id,
    std::string parent_guid) {
  GURL gurl = url::GURLAndroid::ToNativeGURL(env, url);
  DCHECK(gurl.is_valid()) << "The given string ("
                          << gurl.possibly_invalid_spec()
                          << ") is not a valid URL.";

  fake_server::EntityBuilderFactory entity_builder_factory;
  base::Uuid converted_guid = base::Uuid::GenerateRandomV4();
  if (guid) {
    converted_guid = base::Uuid::ParseCaseInsensitive(guid.value());
  }
  fake_server::BookmarkEntityBuilder bookmark_builder =
      entity_builder_factory.NewBookmarkEntityBuilder(title, converted_guid);
  bookmark_builder.SetParentId(parent_id);
  bookmark_builder.SetParentGuid(base::Uuid::ParseLowercase(parent_guid));
  return bookmark_builder.BuildBookmark(gurl);
}

syncer::SyncServiceImpl* GetSyncServiceImpl() {
  DCHECK(g_browser_process && g_browser_process->profile_manager());
  return SyncServiceFactory::GetAsSyncServiceImplForProfileForTesting(
      ProfileManager::GetLastUsedProfile());
}

}  // namespace

static jlong JNI_FakeServerHelper_CreateFakeServer(JNIEnv* env) {
  auto* fake_server = new fake_server::FakeServer();
  GetSyncServiceImpl()->OverrideNetworkForTest(
      fake_server::CreateFakeServerHttpPostProviderFactory(
          fake_server->AsWeakPtr()));
  return reinterpret_cast<intptr_t>(fake_server);
}

static void JNI_FakeServerHelper_DeleteFakeServer(JNIEnv* env,
                                                  jlong fake_server) {
  GetSyncServiceImpl()->OverrideNetworkForTest(
      syncer::CreateHttpPostProviderFactory());
  delete reinterpret_cast<fake_server::FakeServer*>(fake_server);
}

static jboolean JNI_FakeServerHelper_VerifyEntityCountByTypeAndName(
    JNIEnv* env,
    jlong fake_server,
    jint count,
    jint data_type,
    std::string& name) {
  fake_server::FakeServer* fake_server_ptr =
      reinterpret_cast<fake_server::FakeServer*>(fake_server);
  fake_server::FakeServerVerifier fake_server_verifier(fake_server_ptr);
  testing::AssertionResult result =
      fake_server_verifier.VerifyEntityCountByTypeAndName(
          count, static_cast<syncer::DataType>(data_type), name);

  if (!result)
    LOG(WARNING) << result.message();

  return result;
}

static jboolean JNI_FakeServerHelper_VerifySessions(
    JNIEnv* env,
    jlong fake_server,
    const JavaParamRef<jobjectArray>& url_array) {
  std::multiset<std::string> tab_urls;
  for (auto j_string : url_array.ReadElements<jstring>()) {
    tab_urls.insert(base::android::ConvertJavaStringToUTF8(env, j_string));
  }
  fake_server::SessionsHierarchy expected_sessions;
  expected_sessions.AddWindow(tab_urls);

  fake_server::FakeServer* fake_server_ptr =
      reinterpret_cast<fake_server::FakeServer*>(fake_server);
  fake_server::FakeServerVerifier fake_server_verifier(fake_server_ptr);
  testing::AssertionResult result =
      fake_server_verifier.VerifySessions(expected_sessions);

  if (!result)
    LOG(WARNING) << result.message();

  return result;
}

static base::android::ScopedJavaLocalRef<jobjectArray>
JNI_FakeServerHelper_GetSyncEntitiesByDataType(JNIEnv* env,
                                               jlong fake_server,
                                               jint data_type) {
  fake_server::FakeServer* fake_server_ptr =
      reinterpret_cast<fake_server::FakeServer*>(fake_server);
  std::vector<sync_pb::SyncEntity> entities =
      fake_server_ptr->GetSyncEntitiesByDataType(
          static_cast<syncer::DataType>(data_type));

  std::vector<std::string> entity_strings;
  for (const sync_pb::SyncEntity& entity : entities) {
    std::string s;
    entity.SerializeToString(&s);
    entity_strings.push_back(s);
  }
  return base::android::ToJavaArrayOfByteArray(env, entity_strings);
}

static void JNI_FakeServerHelper_InjectUniqueClientEntity(
    JNIEnv* env,
    jlong fake_server,
    std::string& non_unique_name,
    std::string& client_tag,
    const JavaParamRef<jbyteArray>& serialized_entity_specifics) {
  fake_server::FakeServer* fake_server_ptr =
      reinterpret_cast<fake_server::FakeServer*>(fake_server);

  sync_pb::EntitySpecifics entity_specifics;
  DeserializeEntitySpecifics(env, serialized_entity_specifics,
                             &entity_specifics);

  int64_t now = syncer::TimeToProtoTime(base::Time::Now());
  fake_server_ptr->InjectEntity(
      syncer::PersistentUniqueClientEntity::CreateFromSpecificsForTesting(
          non_unique_name, client_tag, entity_specifics, /*creation_time=*/now,
          /*last_modified_time=*/now));
}

static void JNI_FakeServerHelper_SetWalletData(
    JNIEnv* env,
    jlong fake_server,
    const JavaParamRef<jbyteArray>& serialized_entity) {
  fake_server::FakeServer* fake_server_ptr =
      reinterpret_cast<fake_server::FakeServer*>(fake_server);

  sync_pb::SyncEntity entity;
  DeserializeEntity(env, serialized_entity, &entity);

  fake_server_ptr->SetWalletData({entity});
}

static void JNI_FakeServerHelper_ModifyEntitySpecifics(
    JNIEnv* env,
    jlong fake_server,
    std::string& id,
    const JavaParamRef<jbyteArray>& serialized_entity_specifics) {
  fake_server::FakeServer* fake_server_ptr =
      reinterpret_cast<fake_server::FakeServer*>(fake_server);

  sync_pb::EntitySpecifics entity_specifics;
  DeserializeEntitySpecifics(env, serialized_entity_specifics,
                             &entity_specifics);

  fake_server_ptr->ModifyEntitySpecifics(id, entity_specifics);
}

static void JNI_FakeServerHelper_InjectDeviceInfoEntity(
    JNIEnv* env,
    jlong fake_server,
    std::string& cache_guid,
    std::string& client_name,
    jlong creation_timestamp,
    jlong last_updated_timestamp) {
  CHECK_LE(creation_timestamp, last_updated_timestamp);

  sync_pb::EntitySpecifics entity_specifics;
  sync_pb::DeviceInfoSpecifics* specifics =
      entity_specifics.mutable_device_info();
  specifics->set_cache_guid(cache_guid);
  specifics->set_client_name(client_name);
  specifics->set_last_updated_timestamp(syncer::TimeToProtoTime(
      base::Time::FromMillisecondsSinceUnixEpoch(last_updated_timestamp)));
  // Every client supports send-tab-to-self these days.
  specifics->mutable_feature_fields()->set_send_tab_to_self_receiving_enabled(
      true);
  specifics->mutable_feature_fields()->set_send_tab_to_self_receiving_type(
      sync_pb::
          SyncEnums_SendTabReceivingType_SEND_TAB_RECEIVING_TYPE_CHROME_OR_UNSPECIFIED);
  specifics->set_device_type(
      sync_pb::SyncEnums::DeviceType::SyncEnums_DeviceType_TYPE_PHONE);
  specifics->set_sync_user_agent("UserAgent");
  specifics->set_chrome_version("1.0");
  specifics->set_signin_scoped_device_id("id");

  reinterpret_cast<fake_server::FakeServer*>(fake_server)
      ->InjectEntity(
          syncer::PersistentUniqueClientEntity::CreateFromSpecificsForTesting(
              /*non_unique_name=*/specifics->client_name(),
              syncer::DeviceInfoUtil::SpecificsToTag(*specifics),
              entity_specifics,
              syncer::TimeToProtoTime(
                  base::Time::FromMillisecondsSinceUnixEpoch(
                      creation_timestamp)),
              syncer::TimeToProtoTime(
                  base::Time::FromMillisecondsSinceUnixEpoch(
                      last_updated_timestamp))));
}

static void JNI_FakeServerHelper_InjectBookmarkEntity(
    JNIEnv* env,
    jlong fake_server,
    std::string& title,
    const JavaParamRef<jobject>& url,
    std::string& parent_id,
    std::string& parent_guid) {
  fake_server::FakeServer* fake_server_ptr =
      reinterpret_cast<fake_server::FakeServer*>(fake_server);
  fake_server_ptr->InjectEntity(CreateBookmarkEntity(
      env, title, url, /*guid=*/std::nullopt, parent_id, parent_guid));
}

static void JNI_FakeServerHelper_InjectBookmarkFolderEntity(
    JNIEnv* env,
    jlong fake_server,
    std::string& title,
    std::string& parent_id,
    std::string& parent_guid) {
  fake_server::FakeServer* fake_server_ptr =
      reinterpret_cast<fake_server::FakeServer*>(fake_server);

  fake_server::EntityBuilderFactory entity_builder_factory;
  fake_server::BookmarkEntityBuilder bookmark_builder =
      entity_builder_factory.NewBookmarkEntityBuilder(title);
  bookmark_builder.SetParentId(parent_id);
  bookmark_builder.SetParentGuid(base::Uuid::ParseLowercase(parent_guid));

  fake_server_ptr->InjectEntity(bookmark_builder.BuildFolder());
}

static void JNI_FakeServerHelper_ModifyBookmarkEntity(
    JNIEnv* env,
    jlong fake_server,
    std::string& entity_id,
    std::string& guid,
    std::string& title,
    const JavaParamRef<jobject>& url,
    std::string& parent_id,
    std::string& parent_guid) {
  fake_server::FakeServer* fake_server_ptr =
      reinterpret_cast<fake_server::FakeServer*>(fake_server);
  std::unique_ptr<syncer::LoopbackServerEntity> bookmark =
      CreateBookmarkEntity(env, title, url, guid, parent_id, parent_guid);
  sync_pb::SyncEntity proto;
  bookmark->SerializeAsProto(&proto);
  fake_server_ptr->ModifyBookmarkEntity(entity_id, parent_id,
                                        proto.specifics());
}

static void JNI_FakeServerHelper_ModifyBookmarkFolderEntity(
    JNIEnv* env,
    jlong fake_server,
    std::string& entity_id,
    std::string& guid,
    std::string& title,
    std::string& parent_id,
    std::string& parent_guid) {
  fake_server::FakeServer* fake_server_ptr =
      reinterpret_cast<fake_server::FakeServer*>(fake_server);

  fake_server::EntityBuilderFactory entity_builder_factory;
  fake_server::BookmarkEntityBuilder bookmark_builder =
      entity_builder_factory.NewBookmarkEntityBuilder(
          title, base::Uuid::ParseLowercase(guid));
  bookmark_builder.SetParentId(parent_id);
  bookmark_builder.SetParentGuid(base::Uuid::ParseLowercase(parent_guid));

  sync_pb::SyncEntity proto;
  bookmark_builder.BuildFolder()->SerializeAsProto(&proto);
  fake_server_ptr->ModifyBookmarkEntity(entity_id, parent_id,
                                        proto.specifics());
}

static std::string JNI_FakeServerHelper_GetBookmarkBarFolderId(
    JNIEnv* env,
    jlong fake_server) {
  // Rather hard code this here then incur the cost of yet another method.
  // It is very unlikely that this will ever change.
  return "32904_bookmark_bar";
}

static void JNI_FakeServerHelper_DeleteEntity(JNIEnv* env,
                                              jlong fake_server,
                                              std::string& id,
                                              std::string& client_tag_hash) {
  fake_server::FakeServer* fake_server_ptr =
      reinterpret_cast<fake_server::FakeServer*>(fake_server);
  fake_server_ptr->InjectEntity(
      syncer::PersistentTombstoneEntity::CreateNew(id, client_tag_hash));
}

static void JNI_FakeServerHelper_SetCustomPassphraseNigori(
    JNIEnv* env,
    jlong fake_server,
    std::string& passphrase) {
  SetNigoriInFakeServer(
      syncer::BuildCustomPassphraseNigoriSpecifics(
          syncer::Pbkdf2PassphraseKeyParamsForTesting(passphrase)),
      reinterpret_cast<fake_server::FakeServer*>(fake_server));
}

static void JNI_FakeServerHelper_SetTrustedVaultNigori(
    JNIEnv* env,
    jlong fake_server,
    const JavaParamRef<jbyteArray>& trusted_vault_key) {
  std::vector<uint8_t> native_trusted_vault_key;
  base::android::JavaByteArrayToByteVector(env, trusted_vault_key,
                                           &native_trusted_vault_key);
  SetNigoriInFakeServer(
      syncer::BuildTrustedVaultNigoriSpecifics({native_trusted_vault_key}),
      reinterpret_cast<fake_server::FakeServer*>(fake_server));
}

static void JNI_FakeServerHelper_ClearServerData(JNIEnv* env,
                                                 jlong fake_server) {
  fake_server::FakeServer* fake_server_ptr =
      reinterpret_cast<fake_server::FakeServer*>(fake_server);
  fake_server_ptr->ClearServerData();
}

static void JNI_FakeServerHelper_AddCollaboration(
    JNIEnv* env,
    jlong fake_server,
    std::string& collaboration_id) {
  fake_server::FakeServer* fake_server_ptr =
      reinterpret_cast<fake_server::FakeServer*>(fake_server);
  fake_server_ptr->AddCollaboration(collaboration_id);
}

static void JNI_FakeServerHelper_RemoveCollaboration(
    JNIEnv* env,
    jlong fake_server,
    std::string& collaboration_id) {
  fake_server::FakeServer* fake_server_ptr =
      reinterpret_cast<fake_server::FakeServer*>(fake_server);
  fake_server_ptr->RemoveCollaboration(collaboration_id);
}

static void JNI_FakeServerHelper_AddCollaborationGroupToFakeServer(
    JNIEnv* env,
    jlong fake_server,
    std::string& collaboration_id) {
  const data_sharing::GroupId group_id =
      data_sharing::GroupId(collaboration_id);
  const sync_pb::CollaborationGroupSpecifics collab_specifics =
      collaboration_group_utils::MakeCollaborationGroupSpecifics(
          group_id.value());

  sync_pb::EntitySpecifics entity_specifics;
  *entity_specifics.mutable_collaboration_group() = collab_specifics;

  sync_pb::SyncEntity::CollaborationMetadata metadata;
  metadata.set_collaboration_id(collaboration_id);

  std::string client_tag = collab_specifics.collaboration_id();
  int64_t creation_time =
      collab_specifics.changed_at_timestamp_millis_since_unix_epoch();
  int64_t update_time = creation_time;

  fake_server::FakeServer* fake_server_ptr =
      reinterpret_cast<fake_server::FakeServer*>(fake_server);
  fake_server_ptr->InjectEntity(
      syncer::PersistentUniqueClientEntity::CreateFromSharedSpecificsForTesting(
          "non_unique_name", client_tag, entity_specifics, creation_time,
          update_time, metadata));
}
