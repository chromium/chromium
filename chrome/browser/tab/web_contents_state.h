// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TAB_WEB_CONTENTS_STATE_H_
#define CHROME_BROWSER_TAB_WEB_CONTENTS_STATE_H_

#include "base/android/scoped_java_ref.h"
#include "base/containers/span.h"
#include "base/functional/callback.h"
#include "base/memory/raw_span.h"
#include "content/public/browser/web_contents.h"

namespace sessions {
class SerializedNavigationEntry;
}

namespace content {
class WebContents;
}  // namespace content

// A struct to store the WebContentsState passed down from the JNI to be
// potentially used in restoring a frozen tab, as a byte buffer.
//
// An instance of this type holds a reference to a java.nio.ByteBuffer, and also
// stores a cached base::span<> which provides a view of that ByteBuffer's
// contents.
//
// The saved_state_version parameter is which version of the saved state format
// the buffer stores; the known versions are:
//   0: Chrome <= 18
//   1: Chrome 18 - 25
//   2: Chrome 26+
// TODO(crbug.com/41493935): Get rid of the old versions and possibly the
// version field altogether.
struct WebContentsStateByteBuffer {
  WebContentsStateByteBuffer(base::android::ScopedJavaLocalRef<jobject>
                                 web_contents_byte_buffer_result,
                             int saved_state_version);

  // Initialize from a raw span that needs to be owned elsewhere.
  // `backing_buffer` is never used. Useful for tests.
  WebContentsStateByteBuffer(base::raw_span<const uint8_t> raw_data,
                             int saved_state_version);

  WebContentsStateByteBuffer(const WebContentsStateByteBuffer&) = delete;
  WebContentsStateByteBuffer& operator=(const WebContentsStateByteBuffer&) =
      delete;

  WebContentsStateByteBuffer& operator=(
      WebContentsStateByteBuffer&& other) noexcept;
  WebContentsStateByteBuffer(WebContentsStateByteBuffer&& other) noexcept;

  ~WebContentsStateByteBuffer();

  // This struct and its parameters are only meant for use in storing web
  // contents parsed from the JNI createHistoricalTab and syncedTabDelegate
  // family of function calls, and transferring the data to the
  // RestoreContentsFromByteBuffer function as needed. Outside of this scope,
  // this struct is not meant to be used for any other purposes. Please do not
  // attempt to use this struct anywhere else except for in the provided
  // callstack/use case.
  //
  // TODO(ellyjones): is it necessary to cache this view of the buffer? It is
  // very cheap to recompute on the fly as needed, as long as we have the
  // JNIEnv* ready to hand.
  base::raw_span<const uint8_t> backing_buffer;
  int state_version;
  base::android::ScopedJavaGlobalRef<jobject> java_buffer;
};

// Stores state for a WebContents, including its navigation history.
class WebContentsState {
 public:
  using DeletionPredicate = base::RepeatingCallback<bool(
      const sessions::SerializedNavigationEntry& entry)>;

  static base::android::ScopedJavaLocalRef<jobject>
  GetContentsStateAsByteBuffer(JNIEnv* env, content::WebContents* web_contents);

  // Returns a new buffer without the navigations matching |predicate|.
  // Returns null if no deletions happened.
  static base::android::ScopedJavaLocalRef<jobject>
  DeleteNavigationEntriesFromByteBuffer(JNIEnv* env,
                                        base::span<const uint8_t> buffer,
                                        int saved_state_version,
                                        const DeletionPredicate& predicate);

  // Extracts display title from serialized tab data on restore.
  static base::android::ScopedJavaLocalRef<jstring>
  GetDisplayTitleFromByteBuffer(JNIEnv* env,
                                base::span<const uint8_t> buffer,
                                int saved_state_version);

  // Extracts virtual url from serialized tab data on restore.
  static base::android::ScopedJavaLocalRef<jstring> GetVirtualUrlFromByteBuffer(
      JNIEnv* env,
      base::span<const uint8_t> buffer,
      int saved_state_version);

  // Restores a WebContents from the passed in state using JNI parameters.
  static base::android::ScopedJavaLocalRef<jobject>
  RestoreContentsFromByteBuffer(JNIEnv* env,
                                jobject state,
                                jint saved_state_version,
                                jboolean initially_hidden,
                                jboolean no_renderer);

  // Restores a WebContents from the passed in state using native parameters.
  static std::unique_ptr<content::WebContents> RestoreContentsFromByteBuffer(
      const WebContentsStateByteBuffer* byte_buffer,
      bool initially_hidden,
      bool no_renderer);

  // Extracts state and navigation entries from the given Pickle data and
  // returns whether un-pickling the data succeeded.
  static bool ExtractNavigationEntries(
      base::span<const uint8_t> buffer,
      int saved_state_version,
      bool* is_off_the_record,
      int* current_entry_index,
      std::vector<sessions::SerializedNavigationEntry>* navigations);

  // Synthesizes a stub, single-navigation state for a tab that will be loaded
  // lazily.
  static base::android::ScopedJavaLocalRef<jobject>
  CreateSingleNavigationStateAsByteBuffer(
      JNIEnv* env,
      jstring title,
      jstring url,
      jstring referrer_url,
      jint referrer_policy,
      const base::android::JavaParamRef<jobject>& initiator_origin,
      jboolean is_off_the_record);

  // Creates a single navigation entry in a serilized form.
  static base::Pickle CreateSingleNavigationStateAsPickle(
      std::u16string title,
      const GURL& url,
      content::Referrer referrer,
      url::Origin initiator_origin,
      bool is_off_the_record);

  // Appends a single-navigation state to a WebContentsState to be later loaded
  // lazily.
  static base::android::ScopedJavaLocalRef<jobject> AppendPendingNavigation(
      JNIEnv* env,
      base::span<const uint8_t> buffer,
      int saved_state_version,
      jstring title,
      jstring url,
      jstring referrer_url,
      jint referrer_policy,
      const base::android::JavaParamRef<jobject>& initiator_origin,
      jboolean is_off_the_record);

 private:
  static std::unique_ptr<content::WebContents>
  RestoreContentsFromByteBufferImpl(base::span<const uint8_t> buffer,
                                    int saved_state_version,
                                    bool initially_hidden,
                                    bool no_renderer);
};

#endif  // CHROME_BROWSER_TAB_WEB_CONTENTS_STATE_H_
