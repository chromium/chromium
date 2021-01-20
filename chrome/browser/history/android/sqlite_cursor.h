// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_HISTORY_ANDROID_SQLITE_CURSOR_H_
#define CHROME_BROWSER_HISTORY_ANDROID_SQLITE_CURSOR_H_

#include <memory>
#include <string>
#include <vector>

#include "base/android/scoped_java_ref.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/strings/string16.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/cancelable_task_tracker.h"
#include "chrome/browser/history/android/android_history_provider_service.h"
#include "components/favicon_base/favicon_callback.h"
#include "components/history/core/browser/history_types.h"

// This class is JNI implementation of
// org.chromium.chrome.database.SqliteCursor, it uses the AndroidStatement to
// iterate among the result rows. This is not thread safe, all methods should
// be called from the same non-UI thread which typical is the Java thread.
//
// This class can not be in history namespace because the class name has to
// match to the generated sqlite_cursor_jni.h.
class SQLiteCursor {
 public:
  // Mapping to the column type definitions in java.sql.Types.
  enum JavaColumnType {
    BLOB = 2004,
    LONG_VAR_CHAR = -1,
    NULL_TYPE = 0,
    NUMERIC = 2,
    DOUBLE = 8,
  };

  // This class is intended to be used only in unit tests.
  //
  // There are 2 threads in unit test, one is the UI thread, another is the DB
  // thread, after the task posted into UI thread, the MessageLoop needs to run
  // to execute the task. The OnPostMoveToTask() and the OnPostGetFaviconTask()
  // give unit tests a chance to run the message loop before event_.Wait is
  // invoked, The OnGetMoveToResult() and OnGetFaviconResult() is used to notify
  // the test observer in the UI thread when the task's result comes back, it
  // calls MessageLoop::QuitWhenIdle() to exit the loop, then the event.Wait()
  // is called. Basically, Two threads are used to simulate 3 threads' behavior
  // here.
  // The whole observer design is only for test purpose and should only be used
  // in unit test.
  class TestObserver {
   public:
    TestObserver();

    // Notify the MoveTo task has been posted to UI thread.
    virtual void OnPostMoveToTask() = 0;
    // Notify the MoveTo result has been gotten in UI thread.
    virtual void OnGetMoveToResult() = 0;
    // Notify the GetFavicon task has been posted to UI thread.
    virtual void OnPostGetFaviconTask() = 0;
    // Notify the GetFavicon result has been gotten in UI thread.
    virtual void OnGetFaviconResult() = 0;

   protected:
    virtual ~TestObserver();
  };

  // Returns org.chromium.chrome.SQLiteCursor java object by creating
  // SQLitCursor native and java objects, then bind them together.
  static base::android::ScopedJavaLocalRef<jobject> NewJavaSqliteCursor(
      JNIEnv* env,
      const std::vector<std::string>& column_names,
      history::AndroidStatement* statement,
      AndroidHistoryProviderService* service);

  // JNI methods -----------------------------------------------------------

  // Returns the result row count.
  jint GetCount(JNIEnv* env, const base::android::JavaParamRef<jobject>& obj);

  // Returns the result's columns' name.
  base::android::ScopedJavaLocalRef<jobjectArray> GetColumnNames(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);

  // Returns the given column value as jstring.
  base::android::ScopedJavaLocalRef<jstring> GetString(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      jint column);

  // Returns the given column value as jlong.
  jlong GetLong(JNIEnv* env,
                const base::android::JavaParamRef<jobject>& obj,
                jint column);

  // Returns the given column value as int.
  jint GetInt(JNIEnv* env,
              const base::android::JavaParamRef<jobject>& obj,
              jint column);

  // Returns the given column value as double.
  jdouble GetDouble(JNIEnv* env,
                    const base::android::JavaParamRef<jobject>& obj,
                    jint column);

  // Returns the given column value as jbyteArray.
  base::android::ScopedJavaLocalRef<jbyteArray> GetBlob(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      jint column);

  // Return JNI_TRUE if the give column value is NULL, JNI_FALSE otherwise.
  jboolean IsNull(JNIEnv* env,
                  const base::android::JavaParamRef<jobject>& obj,
                  jint column);

  // Moves the cursor to |pos|, returns new position.
  // If the returned position is not equal to |pos|, then the cursor points to
  // the last row.
  jint MoveTo(JNIEnv* env,
              const base::android::JavaParamRef<jobject>& obj,
              jint pos);

  // Returns the type of column.
  jint GetColumnType(JNIEnv* env,
                     const base::android::JavaParamRef<jobject>& obj,
                     jint column);

  // Called from Java to relase this object.
  void Destroy(JNIEnv* env, const base::android::JavaParamRef<jobject>& obj);

 private:
  FRIEND_TEST_ALL_PREFIXES(SQLiteCursorTest, Run);

  // |column_names| is the column names of this cursor, the sequence of name
  // should match the sql query's projection name.
  // |statement| is query's statement which bound the variables. This class
  // take the ownership of |statement|.
  SQLiteCursor(const std::vector<std::string>& column_names,
               history::AndroidStatement* statement,
               AndroidHistoryProviderService* service);

  virtual ~SQLiteCursor();

  // Destory SQLiteCursor object on UI thread. All cleanup need finish in UI
  // thread.
  void DestroyOnUIThread();

  // This method is for testing only.
  void set_test_observer(TestObserver* test_observer) {
    test_observer_ = test_observer;
  }

  // Get Favicon from history backend.
  bool GetFavicon(favicon_base::FaviconID id,
                  std::vector<unsigned char>* image_data);

  void GetFaviconForIDInUIThread(
      favicon_base::FaviconID id,
      favicon_base::FaviconRawBitmapCallback callback);

  // The callback function of FaviconService::GetLargestRawFaviconForID().
  void OnFaviconData(const favicon_base::FaviconRawBitmapResult& bitmap_result);

  // The callback function of MoveTo().
  void OnMoved(int pos);

  JavaColumnType GetColumnTypeInternal(int column);

  // Runs the MoveStatement on UI thread.
  void RunMoveStatementOnUIThread(int pos);

  // The current row position, '-1' means the position before the first one.
  int position_;

  base::WaitableEvent event_;

  // The wrapped history::AndroidStatement.
  history::AndroidStatement* statement_;

  // Result set columns' name
  const std::vector<std::string> column_names_;

  AndroidHistoryProviderService* service_;

  // Live on UI thread.
  std::unique_ptr<base::CancelableTaskTracker> tracker_;

  // The count of result rows.
  int count_;

  // The favicon image.
  favicon_base::FaviconRawBitmapResult favicon_bitmap_result_;

  TestObserver* test_observer_;

  DISALLOW_COPY_AND_ASSIGN(SQLiteCursor);
};

#endif  // CHROME_BROWSER_HISTORY_ANDROID_SQLITE_CURSOR_H_
