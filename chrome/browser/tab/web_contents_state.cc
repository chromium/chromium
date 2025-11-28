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
#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/pickle.h"
#include "chrome/browser/profiles/profile.h"
#include "components/sessions/content/content_serialized_navigation_builder.h"
#include "components/sessions/core/serialized_navigation_entry.h"
#include "components/sessions/core/session_command.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/restore_type.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/referrer.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/tab/jni_headers/WebContentsState_jni.h"

using base::android::ConvertUTF16ToJavaString;
using base::android::ConvertUTF8ToJavaString;
using base::android::JavaParamRef;
using base::android::MethodID;
using base::android::ScopedJavaLocalRef;
using content::BrowserContext;
using content::NavigationController;
using content::WebContents;

namespace {

ScopedJavaLocalRef<jobject> CreateByteBufferDirect(JNIEnv* env, int size) {
  ScopedJavaLocalRef<jclass> clazz =
      base::android::GetClass(env, "java/nio/ByteBuffer");
  jmethodID method = MethodID::Get<MethodID::TYPE_STATIC>(
      env, clazz.obj(), "allocateDirect", "(I)Ljava/nio/ByteBuffer;");
  jobject ret = env->CallStaticObjectMethod(clazz.obj(), method, size);
  if (base::android::ClearException(env)) {
    return {};
  }
  return base::android::ScopedJavaLocalRef<jobject>::Adopt(env, ret);
}

void WriteStateHeaderToPickle(bool off_the_record,
                              int entry_count,
                              int current_entry_index,
                              base::Pickle* pickle) {
  pickle->WriteBool(off_the_record);
  pickle->WriteInt(entry_count);
  pickle->WriteInt(current_entry_index);
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
      CreateByteBufferDirect(env, static_cast<int>(pickle.size()));
  if (buffer) {
    base::span<uint8_t> buffer_span =
        base::android::JavaByteBufferToMutableSpan(env, buffer);
    buffer_span.copy_from(pickle);
  }
  return buffer;
}

std::vector<sessions::SerializedNavigationEntry> SerializeNavigations(
    const std::vector<content::NavigationEntry*>& navigations) {
  std::vector<sessions::SerializedNavigationEntry> serialized;
  serialized.reserve(navigations.size());
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
    BrowserContext* browser_context,
    const std::optional<std::u16string>& title,
    const std::string& url,
    const std::optional<std::string>& referrer_url,
    int referrer_policy,
    const std::optional<url::Origin>& optional_initiator_origin) {
  content::Referrer referrer;
  if (referrer_url.has_value()) {
    referrer =
        content::Referrer(GURL(*referrer_url),
                          content::Referrer::ConvertToPolicy(referrer_policy));
  }

  url::Origin initiator_origin =
      optional_initiator_origin.value_or(url::Origin());
  // TODO(crbug.com/40062134): Deal with getting initiator_base_url
  // plumbed here too.
  auto navigation_entry = content::NavigationController::CreateNavigationEntry(
      GURL(url), referrer, initiator_origin,
      /* initiator_base_url= */ std::nullopt, ui::PAGE_TRANSITION_LINK,
      /* is_renderer_initiated= */ true,
      /* extra_headers= */ "", browser_context,
      /* blob_url_loader_factory= */ nullptr);
  if (title.has_value()) {
    navigation_entry->SetTitle(*title);
  }
  return navigation_entry;
}

}  // namespace

WebContentsStateByteBuffer::WebContentsStateByteBuffer(
    base::android::ScopedJavaLocalRef<jobject> web_contents_byte_buffer_result,
    int saved_state_version)
    : state_version(saved_state_version) {
  JNIEnv* env = base::android::AttachCurrentThread();
  java_buffer.Reset(web_contents_byte_buffer_result);
  backing_buffer = base::android::JavaByteBufferToSpan(env, java_buffer);
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

  size_t original_size = navigations.size();
  int deleted_navigations = 0;
  size_t write_index = 0;
  for (size_t read_index = 0; read_index < original_size; ++read_index) {
    sessions::SerializedNavigationEntry& navigation = navigations[read_index];
    if (current_entry_index != navigation.index() &&
        predicate.Run(navigations[read_index])) {
      deleted_navigations++;
    } else {
      // Adjust indices according to number of deleted navigations.
      if (current_entry_index == navigation.index()) {
        current_entry_index -= deleted_navigations;
      }
      navigation.set_index(navigation.index() -
                                         deleted_navigations);
      if (write_index != read_index) {
        navigations[write_index] = std::move(navigation);
      }
      write_index++;
    }
  }

  if (write_index == original_size) {
    return ScopedJavaLocalRef<jobject>();
  }

  navigations.resize(write_index);

  return WriteSerializedNavigationsAsByteBuffer(
      env, is_off_the_record, navigations, current_entry_index);
}

std::optional<std::u16string> WebContentsState::GetDisplayTitleFromByteBuffer(
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
    return std::nullopt;
  }

  sessions::SerializedNavigationEntry nav_entry =
      navigations.at(current_entry_index);
  return nav_entry.title();
}

std::optional<std::string> WebContentsState::GetVirtualUrlFromByteBuffer(
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
    return std::nullopt;
  }

  sessions::SerializedNavigationEntry nav_entry =
      navigations.at(current_entry_index);
  return nav_entry.virtual_url().spec();
}

ScopedJavaLocalRef<jobject> WebContentsState::RestoreContentsFromByteBuffer(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& state,
    BrowserContext* browser_context,
    int saved_state_version,
    jboolean initially_hidden,
    jboolean no_renderer) {
  base::span<const uint8_t> span =
      base::android::JavaByteBufferToSpan(env, state);

  WebContents* web_contents =
      WebContentsState::RestoreContentsFromByteBufferImpl(
          browser_context, span, saved_state_version, initially_hidden,
          no_renderer)
          .release();

  if (web_contents) {
    return web_contents->GetJavaWebContents();
  } else {
    return ScopedJavaLocalRef<jobject>();
  }
}

std::unique_ptr<WebContents> WebContentsState::RestoreContentsFromByteBuffer(
    BrowserContext* browser_context,
    const WebContentsStateByteBuffer* byte_buffer,
    bool initially_hidden,
    bool no_renderer) {
  return WebContentsState::RestoreContentsFromByteBufferImpl(
      browser_context, byte_buffer->backing_buffer, byte_buffer->state_version,
      initially_hidden, no_renderer);
}

std::unique_ptr<WebContents>
WebContentsState::RestoreContentsFromByteBufferImpl(
    BrowserContext* browser_context,
    base::span<const uint8_t> buffer,
    int saved_state_version,
    bool initially_hidden,
    bool no_renderer) {
  bool is_off_the_record = browser_context->IsOffTheRecord();
  int current_entry_index;
  std::vector<sessions::SerializedNavigationEntry> navigations;
  bool success = WebContentsState::ExtractNavigationEntries(
      buffer, saved_state_version, &is_off_the_record, &current_entry_index,
      &navigations);
  if (!success) {
    return nullptr;
  }

  std::vector<std::unique_ptr<content::NavigationEntry>> entries =
      sessions::ContentSerializedNavigationBuilder::ToNavigationEntries(
          navigations, browser_context);

  WebContents::CreateParams params(browser_context);

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

  // Support for versions 0 and 1 is removed in M142/M143. Metrics suggests
  // in-the-wild usage is virtually non-existent (see crbug.com/41493935).
  if (saved_state_version < 2) {
    LOG(ERROR) << "Unsupported saved_state_version: " << saved_state_version;
    return false;
  }

  // `saved_state_version` == 2 and greater.
  navigations->reserve(entry_count);
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
    base::PickleIterator tab_navigation_pickle_iterator(tab_navigation_pickle);
    sessions::SerializedNavigationEntry nav;
    if (!nav.ReadFromPickle(&tab_navigation_pickle_iterator)) {
      return false;  // If we failed to read a navigation, give up on others.
    }

    navigations->push_back(nav);
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
    BrowserContext* browser_context,
    const std::optional<std::u16string>& title,
    const std::string& url,
    const std::optional<std::string>& referrer_url,
    int referrer_policy,
    const std::optional<url::Origin>& initiator_origin) {
  bool is_off_the_record = browser_context->IsOffTheRecord();
  std::unique_ptr<content::NavigationEntry> entry =
      CreatePendingNavigationEntry(browser_context, title, url, referrer_url,
                                   referrer_policy, initiator_origin);

  std::vector<content::NavigationEntry*> navigations(1);
  navigations[0] = entry.get();

  return WriteNavigationsAsByteBuffer(env, is_off_the_record, navigations, 0);
}

base::Pickle WebContentsState::CreateSingleNavigationStateAsPickle(
    BrowserContext* browser_context,
    std::u16string title,
    const GURL& url,
    content::Referrer referrer,
    url::Origin initiator_origin) {
  base::Pickle pickle;
  std::unique_ptr<content::NavigationEntry> navigation_entry =
      content::NavigationController::CreateNavigationEntry(
          url, referrer, initiator_origin,
          /* initiator_base_url= */ std::nullopt, ui::PAGE_TRANSITION_LINK,
          /* is_renderer_initiated= */ true,
          /* extra_headers= */ "", browser_context,
          /* blob_url_loader_factory= */ nullptr);
  navigation_entry->SetTitle(std::move(title));

  std::vector<sessions::SerializedNavigationEntry> serialized =
      SerializeNavigations({navigation_entry.get()});

  return WriteSerializedNavigationsAsPickle(browser_context->IsOffTheRecord(),
                                            serialized, 0);
}

ScopedJavaLocalRef<jobject> WebContentsState::AppendPendingNavigation(
    JNIEnv* env,
    BrowserContext* browser_context,
    base::span<const uint8_t> buffer,
    int saved_state_version,
    bool clobber_current_entry,
    const std::optional<std::u16string>& title,
    const std::string& url,
    const std::optional<std::string>& referrer_url,
    int referrer_policy,
    const std::optional<url::Origin>& initiator_origin) {
  bool is_off_the_record;
  bool is_context_off_the_record = browser_context->IsOffTheRecord();
  int current_entry_index;
  std::vector<sessions::SerializedNavigationEntry> navigations;
  bool success = WebContentsState::ExtractNavigationEntries(
      buffer, saved_state_version, &is_off_the_record, &current_entry_index,
      &navigations);

  bool safeToAppend = success && is_context_off_the_record == is_off_the_record;
  base::UmaHistogramBoolean(
      "Tabs.DeserializationResultForAppendPendingNavigation", safeToAppend);
  if (!safeToAppend) {
    LOG(WARNING) << "Failed to deserialize navigation entries, clobbering "
                    "previous navigation state.";
    return CreateSingleNavigationStateAsByteBuffer(
        env, browser_context, title, url, referrer_url, referrer_policy,
        initiator_origin);
  }

  int new_entry_index = current_entry_index + (clobber_current_entry ? 0 : 1);
  navigations.erase(std::next(navigations.begin(), new_entry_index),
                    navigations.end());
  std::unique_ptr<content::NavigationEntry> new_entry =
      CreatePendingNavigationEntry(browser_context, title, url, referrer_url,
                                   referrer_policy, initiator_origin);
  navigations.push_back(
      sessions::ContentSerializedNavigationBuilder::FromNavigationEntry(
          new_entry_index, new_entry.get()));

  return WriteSerializedNavigationsAsByteBuffer(
      env, is_off_the_record, navigations, new_entry_index);
}

// Static JNI methods.

static ScopedJavaLocalRef<jobject>
JNI_WebContentsState_RestoreContentsFromByteBuffer(
    JNIEnv* env,
    Profile* profile,
    const JavaParamRef<jobject>& state,
    int saved_state_version,
    jboolean initially_hidden,
    jboolean no_renderer) {
  return WebContentsState::RestoreContentsFromByteBuffer(
      env, state, profile, saved_state_version, initially_hidden, no_renderer);
}

static ScopedJavaLocalRef<jobject>
JNI_WebContentsState_GetContentsStateAsByteBuffer(
    JNIEnv* env,
    const JavaParamRef<jobject>& jweb_contents) {
  WebContents* web_contents =
      content::WebContents::FromJavaWebContents(jweb_contents);
  return WebContentsState::GetContentsStateAsByteBuffer(env, web_contents);
}

static base::android::ScopedJavaLocalRef<jobject>
JNI_WebContentsState_DeleteNavigationEntries(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& state,
    int saved_state_version,
    jlong predicate_ptr) {
  base::span<const uint8_t> span =
      base::android::JavaByteBufferToSpan(env, state);

  const auto* predicate =
      reinterpret_cast<WebContentsState::DeletionPredicate*>(predicate_ptr);

  return WebContentsState::DeleteNavigationEntriesFromByteBuffer(
      env, span, saved_state_version, *predicate);
}

static ScopedJavaLocalRef<jobject>
JNI_WebContentsState_CreateSingleNavigationStateAsByteBuffer(
    JNIEnv* env,
    Profile* profile,
    std::optional<std::u16string>& title,
    std::string& url,
    std::optional<std::string>& referrer_url,
    int referrer_policy,
    std::optional<url::Origin>& initiator_origin) {
  return WebContentsState::CreateSingleNavigationStateAsByteBuffer(
      env, profile, title, url, referrer_url, referrer_policy,
      initiator_origin);
}

static ScopedJavaLocalRef<jobject> JNI_WebContentsState_AppendPendingNavigation(
    JNIEnv* env,
    Profile* profile,
    const JavaParamRef<jobject>& state,
    int saved_state_version,
    jboolean clobber_current_entry,
    std::optional<std::u16string>& title,
    std::string& url,
    std::optional<std::string>& referrer_url,
    int referrer_policy,
    std::optional<url::Origin>& initiator_origin) {
  base::span<const uint8_t> span =
      base::android::JavaByteBufferToSpan(env, state);

  return WebContentsState::AppendPendingNavigation(
      env, profile, span, saved_state_version, clobber_current_entry, title,
      url, referrer_url, referrer_policy, initiator_origin);
}

static std::optional<std::u16string>
JNI_WebContentsState_GetDisplayTitleFromByteBuffer(
    JNIEnv* env,
    const JavaParamRef<jobject>& state,
    int saved_state_version) {
  base::span<const uint8_t> span =
      base::android::JavaByteBufferToSpan(env, state);

  return WebContentsState::GetDisplayTitleFromByteBuffer(env, span,
                                                         saved_state_version);
}

static std::optional<std::string>
JNI_WebContentsState_GetVirtualUrlFromByteBuffer(
    JNIEnv* env,
    const JavaParamRef<jobject>& state,
    int saved_state_version) {
  base::span<const uint8_t> span =
      base::android::JavaByteBufferToSpan(env, state);

  return WebContentsState::GetVirtualUrlFromByteBuffer(env, span,
                                                       saved_state_version);
}

static void JNI_WebContentsState_FreeStringPointer(JNIEnv* env,
                                                   jlong string_pointer) {
  delete reinterpret_cast<std::string*>(string_pointer);
}

DEFINE_JNI(WebContentsState)
