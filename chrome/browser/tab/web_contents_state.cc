// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tab/web_contents_state.h"

#include <stddef.h>
#include <stdint.h>

#include <limits>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/android/jni_android.h"
#include "base/android/jni_bytebuffer.h"
#include "base/android/jni_string.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/pickle.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/sessions/content/content_serialized_navigation_builder.h"
#include "components/sessions/core/serialized_navigation_entry.h"
#include "components/sessions/core/session_command.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/restore_type.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/referrer.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/tab/jni_headers/WebContentsStateBridge_jni.h"

using base::android::ConvertUTF16ToJavaString;
using base::android::ConvertUTF8ToJavaString;
using base::android::JavaParamRef;
using base::android::MethodID;
using base::android::ScopedJavaLocalRef;
using content::NavigationController;
using content::WebContents;

WebContentsStateByteBuffer::WebContentsStateByteBuffer(
    base::android::ScopedJavaLocalRef<jobject> web_contents_byte_buffer_result,
    int saved_state_version)
    : state_version(saved_state_version) {
  JNIEnv* env = base::android::AttachCurrentThread();
  java_buffer.Reset(web_contents_byte_buffer_result);
  backing_buffer = base::android::JavaByteBufferToSpan(env, java_buffer.obj());
}

WebContentsStateByteBuffer::WebContentsStateByteBuffer(
    base::raw_span<const uint8_t> raw_data,
    int saved_state_version)
    : backing_buffer(raw_data), state_version(saved_state_version) {}

WebContentsStateByteBuffer::~WebContentsStateByteBuffer() = default;

WebContentsStateByteBuffer& WebContentsStateByteBuffer::operator=(
    WebContentsStateByteBuffer&& other) noexcept = default;
WebContentsStateByteBuffer::WebContentsStateByteBuffer(
    WebContentsStateByteBuffer&& other) noexcept = default;

namespace {

ScopedJavaLocalRef<jobject> CreateByteBufferDirect(JNIEnv* env, jint size) {
  ScopedJavaLocalRef<jclass> clazz =
      base::android::GetClass(env, "java/nio/ByteBuffer");
  jmethodID method = MethodID::Get<MethodID::TYPE_STATIC>(
      env, clazz.obj(), "allocateDirect", "(I)Ljava/nio/ByteBuffer;");
  jobject ret = env->CallStaticObjectMethod(clazz.obj(), method, size);
  if (base::android::ClearException(env)) {
    return {};
  }
  return base::android::ScopedJavaLocalRef<jobject>(env, ret);
}

void WriteStateHeaderToPickle(bool off_the_record,
                              int entry_count,
                              int current_entry_index,
                              base::Pickle* pickle) {
  pickle->WriteBool(off_the_record);
  pickle->WriteInt(entry_count);
  pickle->WriteInt(current_entry_index);
}

// Migrates a pickled SerializedNavigationEntry from Android tab version 0 to
// 2 or (Chrome 18->26).
//
// Due to the fact that all SerializedNavigationEntrys were previously stored
// in a single pickle on Android, this function has to read the fields exactly
// how they were written on m18 which is a custom format and different other
// chromes.
//
// This uses the fields from SerializedNavigationEntry/TabNavigation from:
// https://gerrit-int.chromium.org/gitweb?p=clank/internal/apps.git;
//              a=blob;f=native/framework/chrome/tab.cc;hb=refs/heads/m18
//
// 1. For each tab navigation:
//   virtual_url
//   title
//   content_state
//   transition_type
//   type_mask
//
// 2. For each tab navigation:
//   referrer
//   is_overriding_user_agent
//
void UpgradeNavigationFromV0ToV2(
    std::vector<sessions::SerializedNavigationEntry>* navigations,
    int entry_count,
    base::PickleIterator* iterator) {
  for (int i = 0; i < entry_count; ++i) {
    base::Pickle v2_pickle;
    std::string virtual_url_spec;
    std::string str_referrer;
    std::u16string title;
    std::string content_state;
    int transition_type_int;
    if (!iterator->ReadString(&virtual_url_spec) ||
        !iterator->ReadString(&str_referrer) ||
        !iterator->ReadString16(&title) ||
        !iterator->ReadString(&content_state) ||
        !iterator->ReadInt(&transition_type_int)) {
      return;
    }

    // Write back the fields that were just read.
    v2_pickle.WriteInt(i);
    v2_pickle.WriteString(virtual_url_spec);
    v2_pickle.WriteString16(title);
    v2_pickle.WriteString(content_state);
    v2_pickle.WriteInt(transition_type_int);

    // type_mask
    v2_pickle.WriteInt(0);
    // referrer_spec
    v2_pickle.WriteString(str_referrer);
    // policy_int
    v2_pickle.WriteInt(0);
    // original_request_url_spec
    v2_pickle.WriteString(std::string());
    // is_overriding_user_agent
    v2_pickle.WriteBool(false);
    // timestamp_internal_value
    v2_pickle.WriteInt64(0);
    // search_terms
    v2_pickle.WriteString16(std::u16string());

    base::PickleIterator tab_navigation_pickle_iterator(v2_pickle);
    sessions::SerializedNavigationEntry nav;
    if (nav.ReadFromPickle(&tab_navigation_pickle_iterator)) {
      navigations->push_back(nav);
    } else {
      LOG(ERROR) << "Failed to read SerializedNavigationEntry from pickle "
                 << "(index=" << i << ", url=" << virtual_url_spec;
    }
  }

  for (int i = 0; i < entry_count; ++i) {
    std::string initial_url;
    bool user_agent_overridden;
    if (!iterator->ReadString(&initial_url) ||
        !iterator->ReadBool(&user_agent_overridden)) {
      break;
    }
  }
}

// Migrates a pickled SerializedNavigationEntry from Android tab version 0 to 1
// (or Chrome 25->26)
//
// Due to the fact that all SerializedNavigationEntrys were previously stored in
// a single pickle on Android, this function reads all the old fields,
// re-outputs them and appends an empty string16, representing the new
// search_terms field, and ensures that reading a v0 SerializedNavigationEntry
// won't consume bytes from a subsequent SerializedNavigationEntry.
//
// This uses the fields from SerializedNavigationEntry/TabNavigation prior to
// https://chromiumcodereview.appspot.com/11876045 which are:
//
// index
// virtual_url
// title
// content_state
// transition_type
// type_mask
// referrer
// original_request_url
// is_overriding_user_agent
// timestamp
//
// And finally search_terms was added and this function appends it.
void UpgradeNavigationFromV1ToV2(
    std::vector<sessions::SerializedNavigationEntry>* navigations,
    int entry_count,
    base::PickleIterator* iterator) {
  for (int i = 0; i < entry_count; ++i) {
    base::Pickle v2_pickle;

    int index;
    std::string virtual_url_spec;
    std::u16string title;
    std::string content_state;
    int transition_type_int;
    if (!iterator->ReadInt(&index) ||
        !iterator->ReadString(&virtual_url_spec) ||
        !iterator->ReadString16(&title) ||
        !iterator->ReadString(&content_state) ||
        !iterator->ReadInt(&transition_type_int)) {
      return;
    }

    // Write back the fields that were just read.
    v2_pickle.WriteInt(index);
    v2_pickle.WriteString(virtual_url_spec);
    v2_pickle.WriteString16(title);
    v2_pickle.WriteString(content_state);
    v2_pickle.WriteInt(transition_type_int);

    int type_mask = 0;
    if (!iterator->ReadInt(&type_mask)) {
      continue;
    }
    v2_pickle.WriteInt(type_mask);

    std::string referrer_spec;
    if (iterator->ReadString(&referrer_spec)) {
      v2_pickle.WriteString(referrer_spec);
    }

    int policy_int;
    if (iterator->ReadInt(&policy_int)) {
      v2_pickle.WriteInt(policy_int);
    }

    std::string original_request_url_spec;
    if (iterator->ReadString(&original_request_url_spec)) {
      v2_pickle.WriteString(original_request_url_spec);
    }

    bool is_overriding_user_agent;
    if (iterator->ReadBool(&is_overriding_user_agent)) {
      v2_pickle.WriteBool(is_overriding_user_agent);
    }

    int64_t timestamp_internal_value = 0;
    if (iterator->ReadInt64(&timestamp_internal_value)) {
      v2_pickle.WriteInt64(timestamp_internal_value);
    }

    // Force output of search_terms
    v2_pickle.WriteString16(std::u16string());

    base::PickleIterator tab_navigation_pickle_iterator(v2_pickle);
    sessions::SerializedNavigationEntry nav;
    if (nav.ReadFromPickle(&tab_navigation_pickle_iterator)) {
      navigations->push_back(nav);
    } else {
      LOG(ERROR) << "Failed to read SerializedNavigationEntry from pickle "
                 << "(index=" << i << ", url=" << virtual_url_spec;
    }
  }
}

base::Pickle WriteSerializedNavigationsAsPickle(
    bool is_off_the_record,
    const std::vector<sessions::SerializedNavigationEntry>& navigations,
    int current_entry) {
  base::Pickle pickle;
  WriteStateHeaderToPickle(is_off_the_record, navigations.size(), current_entry,
                           &pickle);

  // Write out all of the NavigationEntrys.
  for (const auto& navigation : navigations) {
    // Write each SerializedNavigationEntry as a separate pickle to avoid
    // optional reads of one tab bleeding into the next tab's data.
    base::Pickle tab_navigation_pickle;
    // Max size taken from
    // CommandStorageManager::CreateUpdateTabNavigationCommand.
    static const size_t max_state_size =
        std::numeric_limits<sessions::SessionCommand::size_type>::max() - 1024;
    navigation.WriteToPickle(max_state_size, &tab_navigation_pickle);
    pickle.WriteInt(tab_navigation_pickle.size());
    pickle.WriteBytes(tab_navigation_pickle.data(),
                      tab_navigation_pickle.size());
  }
  return pickle;
}

ScopedJavaLocalRef<jobject> WriteSerializedNavigationsAsByteBuffer(
    JNIEnv* env,
    bool is_off_the_record,
    const std::vector<sessions::SerializedNavigationEntry>& navigations,
    int current_entry) {
  base::Pickle pickle = WriteSerializedNavigationsAsPickle(
      is_off_the_record, navigations, current_entry);
  ScopedJavaLocalRef<jobject> buffer =
      CreateByteBufferDirect(env, static_cast<jint>(pickle.size()));
  if (buffer) {
    memcpy(env->GetDirectBufferAddress(buffer.obj()), pickle.data(),
           pickle.size());
  }
  return buffer;
}

std::vector<sessions::SerializedNavigationEntry> SerializeNavigations(
    const std::vector<content::NavigationEntry*>& navigations) {
  std::vector<sessions::SerializedNavigationEntry> serialized;
  for (size_t i = 0; i < navigations.size(); ++i) {
    serialized.push_back(
        sessions::ContentSerializedNavigationBuilder::FromNavigationEntry(
            i, navigations[i]));
  }
  return serialized;
}

// Common implementation for GetContentsStateAsByteBuffer() and
// CreateContentsStateAsByteBuffer(). Does not assume ownership of the
// navigations.
ScopedJavaLocalRef<jobject> WriteNavigationsAsByteBuffer(
    JNIEnv* env,
    bool is_off_the_record,
    const std::vector<content::NavigationEntry*>& navigations,
    int current_entry) {
  std::vector<sessions::SerializedNavigationEntry> serialized =
      SerializeNavigations(navigations);
  return WriteSerializedNavigationsAsByteBuffer(env, is_off_the_record,
                                                serialized, current_entry);
}

std::unique_ptr<content::NavigationEntry> CreatePendingNavigationEntry(
    JNIEnv* env,
    jstring title,
    jstring url,
    jstring referrer_url,
    jint referrer_policy,
    const base::android::JavaParamRef<jobject>& jinitiator_origin,
    jboolean is_off_the_record) {
  content::Referrer referrer;
  if (referrer_url) {
    referrer = content::Referrer(
        GURL(base::android::ConvertJavaStringToUTF8(env, referrer_url)),
        content::Referrer::ConvertToPolicy(referrer_policy));
  }

  url::Origin initiator_origin;
  if (jinitiator_origin) {
    initiator_origin = url::Origin::FromJavaObject(env, jinitiator_origin);
  }
  // TODO(crbug.com/40062134): Deal with getting initiator_base_url
  // plumbed here too.
  auto navigation_entry = content::NavigationController::CreateNavigationEntry(
      GURL(base::android::ConvertJavaStringToUTF8(env, url)), referrer,
      initiator_origin, /* initiator_base_url= */ std::nullopt,
      ui::PAGE_TRANSITION_LINK,
      true,  // is_renderer_initiated
      "",    // extra_headers
      ProfileManager::GetActiveUserProfile(),
      nullptr /* blob_url_loader_factory */);
  if (title) {
    navigation_entry->SetTitle(
        base::android::ConvertJavaStringToUTF16(env, title));
  }
  return navigation_entry;
}

}  // namespace

ScopedJavaLocalRef<jobject> WebContentsState::GetContentsStateAsByteBuffer(
    JNIEnv* env,
    content::WebContents* web_contents) {
  if (!web_contents) {
    return ScopedJavaLocalRef<jobject>();
  }

  content::NavigationController& controller = web_contents->GetController();
  const int entry_count = controller.GetEntryCount();
  // Don't try to persist initial NavigationEntry, as it is not actually
  // associated with any navigation and will just result in about:blank on
  // session restore.
  if (controller.GetLastCommittedEntry()->IsInitialEntry()) {
    return ScopedJavaLocalRef<jobject>();
  }

  std::vector<content::NavigationEntry*> navigations(entry_count);
  for (int i = 0; i < entry_count; ++i) {
    navigations[i] = controller.GetEntryAtIndex(i);
  }

  return WriteNavigationsAsByteBuffer(
      env, web_contents->GetBrowserContext()->IsOffTheRecord(), navigations,
      controller.GetLastCommittedEntryIndex());
}

ScopedJavaLocalRef<jobject>
WebContentsState::DeleteNavigationEntriesFromByteBuffer(
    JNIEnv* env,
    base::span<const uint8_t> buffer,
    int saved_state_version,
    const DeletionPredicate& predicate) {
  bool is_off_the_record;
  int current_entry_index;
  std::vector<sessions::SerializedNavigationEntry> navigations;
  bool success = WebContentsState::ExtractNavigationEntries(
      buffer, saved_state_version, &is_off_the_record, &current_entry_index,
      &navigations);
  if (!success) {
    return ScopedJavaLocalRef<jobject>();
  }

  std::vector<sessions::SerializedNavigationEntry> new_navigations;
  int deleted_navigations = 0;
  for (auto& navigation : navigations) {
    if (current_entry_index != navigation.index() &&
        predicate.Run(navigation)) {
      deleted_navigations++;
    } else {
      // Adjust indices according to number of deleted navigations.
      if (current_entry_index == navigation.index()) {
        current_entry_index -= deleted_navigations;
      }
      navigation.set_index(navigation.index() - deleted_navigations);
      new_navigations.push_back(std::move(navigation));
    }
  }
  if (deleted_navigations == 0) {
    return ScopedJavaLocalRef<jobject>();
  }

  return WriteSerializedNavigationsAsByteBuffer(
      env, is_off_the_record, new_navigations, current_entry_index);
}

ScopedJavaLocalRef<jstring> WebContentsState::GetDisplayTitleFromByteBuffer(
    JNIEnv* env,
    base::span<const uint8_t> buffer,
    int saved_state_version) {
  bool is_off_the_record;
  int current_entry_index;
  std::vector<sessions::SerializedNavigationEntry> navigations;
  bool success = WebContentsState::ExtractNavigationEntries(
      buffer, saved_state_version, &is_off_the_record, &current_entry_index,
      &navigations);
  if (!success) {
    return ScopedJavaLocalRef<jstring>();
  }

  sessions::SerializedNavigationEntry nav_entry =
      navigations.at(current_entry_index);
  return ConvertUTF16ToJavaString(env, nav_entry.title());
}

ScopedJavaLocalRef<jstring> WebContentsState::GetVirtualUrlFromByteBuffer(
    JNIEnv* env,
    base::span<const uint8_t> buffer,
    int saved_state_version) {
  bool is_off_the_record;
  int current_entry_index;
  std::vector<sessions::SerializedNavigationEntry> navigations;
  bool success = WebContentsState::ExtractNavigationEntries(
      buffer, saved_state_version, &is_off_the_record, &current_entry_index,
      &navigations);
  if (!success) {
    return ScopedJavaLocalRef<jstring>();
  }

  sessions::SerializedNavigationEntry nav_entry =
      navigations.at(current_entry_index);
  return ConvertUTF8ToJavaString(env, nav_entry.virtual_url().spec());
}

ScopedJavaLocalRef<jobject> WebContentsState::RestoreContentsFromByteBuffer(
    JNIEnv* env,
    jobject state,
    jint saved_state_version,
    jboolean initially_hidden,
    jboolean no_renderer) {
  base::span<const uint8_t> span =
      base::android::JavaByteBufferToSpan(env, state);

  WebContents* web_contents =
      WebContentsState::RestoreContentsFromByteBufferImpl(
          span, saved_state_version, initially_hidden, no_renderer)
          .release();

  if (web_contents) {
    return web_contents->GetJavaWebContents();
  } else {
    return ScopedJavaLocalRef<jobject>();
  }
}

std::unique_ptr<WebContents> WebContentsState::RestoreContentsFromByteBuffer(
    const WebContentsStateByteBuffer* byte_buffer,
    bool initially_hidden,
    bool no_renderer) {
  return WebContentsState::RestoreContentsFromByteBufferImpl(
      byte_buffer->backing_buffer, byte_buffer->state_version, initially_hidden,
      no_renderer);
}

std::unique_ptr<WebContents>
WebContentsState::RestoreContentsFromByteBufferImpl(
    base::span<const uint8_t> buffer,
    int saved_state_version,
    bool initially_hidden,
    bool no_renderer) {
  bool is_off_the_record;
  int current_entry_index;
  std::vector<sessions::SerializedNavigationEntry> navigations;
  bool success = WebContentsState::ExtractNavigationEntries(
      buffer, saved_state_version, &is_off_the_record, &current_entry_index,
      &navigations);
  if (!success) {
    return nullptr;
  }

  Profile* profile = ProfileManager::GetActiveUserProfile();
  std::vector<std::unique_ptr<content::NavigationEntry>> entries =
      sessions::ContentSerializedNavigationBuilder::ToNavigationEntries(
          navigations, profile);

  if (is_off_the_record) {
    // Serialization and deserialization related functionalities are only
    // supported for Incognito tabbed Activities and they use primary OTR
    // profile.
    profile = profile->GetPrimaryOTRProfile(/*create_if_needed=*/true);
  }

  WebContents::CreateParams params(profile);

  params.initially_hidden = initially_hidden;
  if (no_renderer) {
    params.desired_renderer_state =
        WebContents::CreateParams::kNoRendererProcess;
  }
  std::unique_ptr<WebContents> web_contents(WebContents::Create(params));
  web_contents->GetController().Restore(
      current_entry_index, content::RestoreType::kRestored, &entries);
  return web_contents;
}

bool WebContentsState::ExtractNavigationEntries(
    base::span<const uint8_t> buffer,
    int saved_state_version,
    bool* is_off_the_record,
    int* current_entry_index,
    std::vector<sessions::SerializedNavigationEntry>* navigations) {
  int entry_count;
  base::Pickle pickle = base::Pickle::WithUnownedBuffer(buffer);
  base::PickleIterator iter(pickle);
  if (!iter.ReadBool(is_off_the_record) || !iter.ReadInt(&entry_count) ||
      !iter.ReadInt(current_entry_index)) {
    LOG(ERROR) << "Failed to restore state from byte array (length="
               << buffer.size() << ").";
    return false;
  }

  // TODO(crbug.com/41493935): Remove this once we have enough data to
  // conclude whether V0 and V1 are still used.
  constexpr size_t kHighestVersion = 3;
  UMA_HISTOGRAM_EXACT_LINEAR("Android.WebContentsState.SavedStateVersion",
                             saved_state_version, kHighestVersion);

  if (!saved_state_version) {
    // When |saved_state_version| is 0, it predates our notion of each tab
    // having a saved version id. For that version of tab serialization, we
    // used a single pickle for all |SerializedNavigationEntry|s.
    UpgradeNavigationFromV0ToV2(navigations, entry_count, &iter);
  } else if (saved_state_version == 1) {
    // When |saved_state_version| is 1, it predates our notion of each tab
    // having a saved version id. For that version of tab serialization, we
    // used a single pickle for all |SerializedNavigationEntry|s.
    UpgradeNavigationFromV1ToV2(navigations, entry_count, &iter);
  } else {
    // |saved_state_version| == 2 and greater.
    for (int i = 0; i < entry_count; ++i) {
      // Read each SerializedNavigationEntry as a separate pickle to avoid
      // optional reads of one tab bleeding into the next tab's data.
      std::optional<base::span<const uint8_t>> tab_entry = iter.ReadData();
      if (!tab_entry.has_value()) {
        LOG(ERROR) << "Failed to restore tab entry from byte array.";
        return false;  // It's dangerous to keep deserializing now, give up.
      }
      base::Pickle tab_navigation_pickle =
          base::Pickle::WithUnownedBuffer(*tab_entry);
      base::PickleIterator tab_navigation_pickle_iterator(
          tab_navigation_pickle);
      sessions::SerializedNavigationEntry nav;
      if (!nav.ReadFromPickle(&tab_navigation_pickle_iterator)) {
        return false;  // If we failed to read a navigation, give up on others.
      }

      navigations->push_back(nav);
    }
  }

  // Validate the data.
  if (*current_entry_index < 0 ||
      *current_entry_index >= static_cast<int>(navigations->size())) {
    return false;
  }

  return true;
}

ScopedJavaLocalRef<jobject>
WebContentsState::CreateSingleNavigationStateAsByteBuffer(
    JNIEnv* env,
    jstring title,
    jstring url,
    jstring referrer_url,
    jint referrer_policy,
    const base::android::JavaParamRef<jobject>& jinitiator_origin,
    jboolean is_off_the_record) {
  std::unique_ptr<content::NavigationEntry> entry =
      CreatePendingNavigationEntry(env, title, url, referrer_url,
                                   referrer_policy, jinitiator_origin,
                                   is_off_the_record);

  std::vector<content::NavigationEntry*> navigations(1);
  navigations[0] = entry.get();

  return WriteNavigationsAsByteBuffer(env, is_off_the_record, navigations, 0);
}

base::Pickle WebContentsState::CreateSingleNavigationStateAsPickle(
    std::u16string title,
    const GURL& url,
    content::Referrer referrer,
    url::Origin initiator_origin,
    bool is_off_the_record) {
  base::Pickle pickle;
  std::unique_ptr<content::NavigationEntry> navigation_entry =
      content::NavigationController::CreateNavigationEntry(
          url, referrer, initiator_origin,
          /* initiator_base_url= */ std::nullopt, ui::PAGE_TRANSITION_LINK,
          /* is_renderer_initiated= */ true,
          /* extra_headers= */ "", ProfileManager::GetActiveUserProfile(),
          /* blob_url_loader_factory=*/nullptr);
  navigation_entry->SetTitle(std::move(title));

  std::vector<sessions::SerializedNavigationEntry> serialized =
      SerializeNavigations({navigation_entry.get()});

  return WriteSerializedNavigationsAsPickle(is_off_the_record, serialized, 0);
}

ScopedJavaLocalRef<jobject> WebContentsState::AppendPendingNavigation(
    JNIEnv* env,
    base::span<const uint8_t> buffer,
    int saved_state_version,
    jstring title,
    jstring url,
    jstring referrer_url,
    jint referrer_policy,
    const base::android::JavaParamRef<jobject>& jinitiator_origin,
    jboolean jis_off_the_record) {
  bool is_off_the_record;
  int current_entry_index;
  std::vector<sessions::SerializedNavigationEntry> navigations;
  bool success = WebContentsState::ExtractNavigationEntries(
      buffer, saved_state_version, &is_off_the_record, &current_entry_index,
      &navigations);

  bool safeToAppend = success && jis_off_the_record == is_off_the_record;
  base::UmaHistogramBoolean(
      "Tabs.DeserializationResultForAppendPendingNavigation", safeToAppend);
  if (!safeToAppend) {
    LOG(WARNING) << "Failed to deserialize navigation entries, clobbering "
                    "previous navigation state.";
    return CreateSingleNavigationStateAsByteBuffer(
        env, title, url, referrer_url, referrer_policy, jinitiator_origin,
        jis_off_the_record);
  }

  std::vector<sessions::SerializedNavigationEntry> new_navigations;
  for (int i = 0; i <= current_entry_index; i++) {
    new_navigations.push_back(std::move(navigations[i]));
  }

  int new_entry_index = current_entry_index + 1;
  std::unique_ptr<content::NavigationEntry> new_entry =
      CreatePendingNavigationEntry(env, title, url, referrer_url,
                                   referrer_policy, jinitiator_origin,
                                   is_off_the_record);
  new_navigations.push_back(
      sessions::ContentSerializedNavigationBuilder::FromNavigationEntry(
          new_entry_index, new_entry.get()));

  return WriteSerializedNavigationsAsByteBuffer(
      env, is_off_the_record, new_navigations, new_entry_index);
}

// Static JNI methods.

static ScopedJavaLocalRef<jobject>
JNI_WebContentsStateBridge_RestoreContentsFromByteBuffer(
    JNIEnv* env,
    const JavaParamRef<jobject>& state,
    jint saved_state_version,
    jboolean initially_hidden,
    jboolean no_renderer) {
  return WebContentsState::RestoreContentsFromByteBuffer(
      env, state, saved_state_version, initially_hidden, no_renderer);
}

static ScopedJavaLocalRef<jobject>
JNI_WebContentsStateBridge_GetContentsStateAsByteBuffer(
    JNIEnv* env,
    const JavaParamRef<jobject>& jweb_contents) {
  WebContents* web_contents =
      content::WebContents::FromJavaWebContents(jweb_contents);
  return WebContentsState::GetContentsStateAsByteBuffer(env, web_contents);
}

static base::android::ScopedJavaLocalRef<jobject>
JNI_WebContentsStateBridge_DeleteNavigationEntries(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& state,
    jint saved_state_version,
    jlong predicate_ptr) {
  base::span<const uint8_t> span =
      base::android::JavaByteBufferToSpan(env, state);

  const auto* predicate =
      reinterpret_cast<WebContentsState::DeletionPredicate*>(predicate_ptr);

  return WebContentsState::DeleteNavigationEntriesFromByteBuffer(
      env, span, saved_state_version, *predicate);
}

static ScopedJavaLocalRef<jobject>
JNI_WebContentsStateBridge_CreateSingleNavigationStateAsByteBuffer(
    JNIEnv* env,
    const JavaParamRef<jstring>& title,
    const JavaParamRef<jstring>& url,
    const JavaParamRef<jstring>& referrer_url,
    jint referrer_policy,
    const JavaParamRef<jobject>& initiator_origin,
    jboolean is_off_the_record) {
  return WebContentsState::CreateSingleNavigationStateAsByteBuffer(
      env, title, url, referrer_url, referrer_policy, initiator_origin,
      is_off_the_record);
}

static ScopedJavaLocalRef<jobject>
JNI_WebContentsStateBridge_AppendPendingNavigation(
    JNIEnv* env,
    const JavaParamRef<jobject>& state,
    jint saved_state_version,
    const JavaParamRef<jstring>& title,
    const JavaParamRef<jstring>& url,
    const JavaParamRef<jstring>& referrer_url,
    jint referrer_policy,
    const JavaParamRef<jobject>& initiator_origin,
    jboolean is_off_the_record) {
  base::span<const uint8_t> span =
      base::android::JavaByteBufferToSpan(env, state);

  return WebContentsState::AppendPendingNavigation(
      env, span, saved_state_version, title, url, referrer_url, referrer_policy,
      initiator_origin, is_off_the_record);
}

static ScopedJavaLocalRef<jstring>
JNI_WebContentsStateBridge_GetDisplayTitleFromByteBuffer(
    JNIEnv* env,
    const JavaParamRef<jobject>& state,
    jint saved_state_version) {
  base::span<const uint8_t> span =
      base::android::JavaByteBufferToSpan(env, state);

  ScopedJavaLocalRef<jstring> result =
      WebContentsState::GetDisplayTitleFromByteBuffer(env, span,
                                                      saved_state_version);
  return result;
}

static ScopedJavaLocalRef<jstring>
JNI_WebContentsStateBridge_GetVirtualUrlFromByteBuffer(
    JNIEnv* env,
    const JavaParamRef<jobject>& state,
    jint saved_state_version) {
  base::span<const uint8_t> span =
      base::android::JavaByteBufferToSpan(env, state);

  ScopedJavaLocalRef<jstring> result =
      WebContentsState::GetVirtualUrlFromByteBuffer(env, span,
                                                    saved_state_version);
  return result;
}
