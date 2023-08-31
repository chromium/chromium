// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TAB_WEB_CONTENTS_STATE_H_
#define CHROME_BROWSER_TAB_WEB_CONTENTS_STATE_H_

#include "base/android/scoped_java_ref.h"
#include "base/functional/callback.h"
#include "content/public/browser/web_contents.h"

namespace sessions {
class SerializedNavigationEntry;
}

namespace content {
class WebContents;
}  // namespace content

// A struct to store the WebContentsState passed down from the JNI to be
// potentially used in restoring a frozen tab, as a byte buffer.
struct WebContentsStateByteBuffer {
  // `web_contents_byte_buffer_result` ScopedJavaLocalRef that holds the jobject
  // for a web contents state byte buffer.
  // `saved_state_version` Saved state version of the WebContentsState.
  WebContentsStateByteBuffer(base::android::ScopedJavaLocalRef<jobject>
                                 web_contents_byte_buffer_result,
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
  raw_ptr<void> byte_buffer_data;
  int byte_buffer_size;
  int state_version;
  base::android::ScopedJavaGlobalRef<jobject> byte_buffer_result;
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
                                        void* data,
                                        int size,
                                        int saved_state_version,
                                        const DeletionPredicate& predicate);

  // Extracts display title from serialized tab data on restore.
  static base::android::ScopedJavaLocalRef<jstring>
  GetDisplayTitleFromByteBuffer(JNIEnv* env,
                                void* data,
                                int size,
                                int saved_state_version);

  // Extracts virtual url from serialized tab data on restore.
  static base::android::ScopedJavaLocalRef<jstring> GetVirtualUrlFromByteBuffer(
      JNIEnv* env,
      void* data,
      int size,
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

  // Synthesizes a stub, single-navigation state for a tab that will be loaded
  // lazily.
  static base::android::ScopedJavaLocalRef<jobject>
  CreateSingleNavigationStateAsByteBuffer(
      JNIEnv* env,
      jstring url,
      jstring referrer_url,
      jint referrer_policy,
      const base::android::JavaParamRef<jobject>& initiator_origin,
      jboolean is_off_the_record);

 private:
  static std::unique_ptr<content::WebContents>
  RestoreContentsFromByteBufferImpl(void* data,
                                    int size,
                                    int saved_state_version,
                                    bool initially_hidden,
                                    bool no_renderer);
};

#endif  // CHROME_BROWSER_TAB_WEB_CONTENTS_STATE_H_
