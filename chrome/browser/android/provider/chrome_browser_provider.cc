// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/provider/chrome_browser_provider.h"

#include <stdint.h>

#include <cmath>
#include <list>
#include <utility>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ref_counted_memory.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/time/time.h"
#include "chrome/browser/android/provider/blocking_ui_thread_async_request.h"
#include "chrome/browser/android/provider/bookmark_model_task.h"
#include "chrome/browser/android/provider/run_on_ui_thread_blocking.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/bookmarks/managed_bookmark_service_factory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "chrome/browser/history/android/sqlite_cursor.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/history/top_sites_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_utils.h"
#include "components/bookmarks/managed/managed_bookmark_service.h"
#include "components/favicon/core/favicon_service.h"
#include "components/history/core/browser/android/android_history_types.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/top_sites.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_service.h"
#include "content/public/browser/browser_thread.h"
#include "jni/ChromeBrowserProvider_jni.h"
#include "ui/base/layout.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/favicon_size.h"

using base::android::AttachCurrentThread;
using base::android::CheckException;
using base::android::ClearException;
using base::android::ConvertJavaStringToUTF16;
using base::android::ConvertJavaStringToUTF8;
using base::android::ConvertUTF8ToJavaString;
using base::android::ConvertUTF16ToJavaString;
using base::android::GetClass;
using base::android::JavaParamRef;
using base::android::MethodID;
using base::android::JavaRef;
using base::android::ScopedJavaGlobalRef;
using base::android::ScopedJavaLocalRef;
using bookmarks::BookmarkModel;
using bookmarks::BookmarkNode;
using bookmarks::ManagedBookmarkService;
using content::BrowserThread;

// After refactoring the following class hierarchy has been created in order
// to avoid repeating code again for the same basic kind of tasks, to enforce
// the correct thread usage and to prevent known race conditions and deadlocks.
//
// - RunOnUIThreadBlocking: auxiliary class to run methods in the UI thread
//   blocking the current one until finished. Because of the provider threading
//   expectations this cannot be used from the UI thread.
//
// - BookmarkModelTask: base class for all tasks that operate in any way with
//   the bookmark model. This class ensures that the model is loaded and
//   prevents possible deadlocks. Derived classes should make use of
//   RunOnUIThreadBlocking to perform any manipulation of the bookmark model in
//   the UI thread. The Run method of these tasks cannot be invoked directly
//   from the UI thread, but RunOnUIThread can be safely used from the UI
//   thread code of other BookmarkModelTasks.
//
// - AsyncServiceRequest: base class for any asynchronous requests made to a
//   Chromium service that require to block the current thread until completed.
//   Derived classes should make use of RunAsyncRequestOnUIThreadBlocking to
//   post their requests in the UI thread and return the results synchronously.
//   All derived classes MUST ALWAYS call RequestCompleted when receiving the
//   request response. These tasks cannot be invoked from the UI thread.
//
// - HistoryProviderTask: base class for asynchronous requests that make use of
//   AndroidHistoryProviderService. See AsyncServiceRequest for mode details.
//
// - SearchTermTask: base class for asynchronous requests that involve the
//   search term API. Works in the same way as HistoryProviderTask.

namespace {

const char kDefaultUrlScheme[] = "http://";
const int64_t kInvalidContentProviderId = 0;
const int64_t kInvalidBookmarkId = -1;

// ------------- Java-related utility methods ------------- //

jlong JNI_ChromeBrowserProvider_ConvertJLongObjectToPrimitive(
    JNIEnv* env,
    const JavaRef<jobject>& long_obj) {
  ScopedJavaLocalRef<jclass> jlong_clazz = GetClass(env, "java/lang/Long");
  jmethodID long_value = MethodID::Get<MethodID::TYPE_INSTANCE>(
      env, jlong_clazz.obj(), "longValue", "()J");
  return env->CallLongMethod(long_obj.obj(), long_value, NULL);
}

jboolean JNI_ChromeBrowserProvider_ConvertJBooleanObjectToPrimitive(
    JNIEnv* env,
    const JavaRef<jobject>& boolean_object) {
  ScopedJavaLocalRef<jclass> jboolean_clazz =
      GetClass(env, "java/lang/Boolean");
  jmethodID boolean_value = MethodID::Get<MethodID::TYPE_INSTANCE>(
      env, jboolean_clazz.obj(), "booleanValue", "()Z");
  return env->CallBooleanMethod(boolean_object.obj(), boolean_value, NULL);
}

base::Time ConvertJlongToTime(jlong value) {
  return base::Time::UnixEpoch() +
         base::TimeDelta::FromMilliseconds((int64_t)value);
}

jint JNI_ChromeBrowserProvider_ConvertJIntegerToJint(
    JNIEnv* env,
    const JavaRef<jobject>& integer_obj) {
  ScopedJavaLocalRef<jclass> jinteger_clazz =
      GetClass(env, "java/lang/Integer");
  jmethodID int_value = MethodID::Get<MethodID::TYPE_INSTANCE>(
      env, jinteger_clazz.obj(), "intValue", "()I");
  return env->CallIntMethod(integer_obj.obj(), int_value, NULL);
}

std::vector<base::string16> ConvertJStringArrayToString16Array(
    JNIEnv* env,
    const JavaRef<jobjectArray>& array) {
  std::vector<base::string16> results;
  if (!array.is_null()) {
    jsize len = env->GetArrayLength(array.obj());
    for (int i = 0; i < len; i++) {
      ScopedJavaLocalRef<jstring> j_str(
          env,
          static_cast<jstring>(env->GetObjectArrayElement(array.obj(), i)));
      results.push_back(ConvertJavaStringToUTF16(env, j_str));
    }
  }
  return results;
}

// ------------- Utility methods used by tasks ------------- //

// Parse the given url and return a GURL, appending the default scheme
// if one is not present.
GURL ParseAndMaybeAppendScheme(const base::string16& url,
                               const char* default_scheme) {
  GURL gurl(url);
  if (!gurl.is_valid() && !gurl.has_scheme()) {
    base::string16 refined_url(base::ASCIIToUTF16(default_scheme));
    refined_url.append(url);
    gurl = GURL(refined_url);
  }
  return gurl;
}

// ------------- Synchronous task classes ------------- //

// Utility task to add a bookmark.
class AddBookmarkTask : public BookmarkModelTask {
 public:
  explicit AddBookmarkTask(BookmarkModel* model) : BookmarkModelTask(model) {}

  int64_t Run(const base::string16& title,
              const base::string16& url,
              const bool is_folder,
              const int64_t parent_id) {
    int64_t result = kInvalidBookmarkId;
    RunOnUIThreadBlocking::Run(
        base::Bind(&AddBookmarkTask::RunOnUIThread,
                   model(), title, url, is_folder, parent_id, &result));
    return result;
  }

  static void RunOnUIThread(BookmarkModel* model,
                            const base::string16& title,
                            const base::string16& url,
                            const bool is_folder,
                            const int64_t parent_id,
                            int64_t* result) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    DCHECK(result);
    GURL gurl = ParseAndMaybeAppendScheme(url, kDefaultUrlScheme);

    // Check if the bookmark already exists.
    const BookmarkNode* node = model->GetMostRecentlyAddedUserNodeForURL(gurl);
    if (!node) {
      const BookmarkNode* parent_node = NULL;
      if (parent_id >= 0)
        parent_node = bookmarks::GetBookmarkNodeByID(model, parent_id);
      if (!parent_node)
        parent_node = model->bookmark_bar_node();

      if (is_folder)
        node = model->AddFolder(parent_node, parent_node->child_count(), title);
      else
        node = model->AddURL(parent_node, 0, title, gurl);
    }

    *result = node ? node ->id() : kInvalidBookmarkId;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(AddBookmarkTask);
};

// Utility method to remove a bookmark.
class RemoveBookmarkTask : public BookmarkModelTask {
 public:
  explicit RemoveBookmarkTask(BookmarkModel* model)
      : BookmarkModelTask(model) {}

  int Run(const int64_t id) {
    bool did_delete = false;
    RunOnUIThreadBlocking::Run(base::Bind(&RemoveBookmarkTask::RunOnUIThread,
                                          model(), id, &did_delete));
    return did_delete ? 1 : 0;
  }

  static void RunOnUIThread(BookmarkModel* model,
                            const int64_t id,
                            bool* did_delete) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    const BookmarkNode* node = bookmarks::GetBookmarkNodeByID(model, id);
    if (node && node->parent()) {
      model->Remove(node);
      *did_delete = true;
    }
  }

  DISALLOW_COPY_AND_ASSIGN(RemoveBookmarkTask);
};

// Utility method to update a bookmark.
class UpdateBookmarkTask : public BookmarkModelTask {
 public:
  explicit UpdateBookmarkTask(BookmarkModel* model)
      : BookmarkModelTask(model) {}

  int Run(const int64_t id,
          const base::string16& title,
          const base::string16& url,
          const int64_t parent_id) {
    bool did_update = false;
    RunOnUIThreadBlocking::Run(base::Bind(&UpdateBookmarkTask::RunOnUIThread,
                                          model(), id, title, url, parent_id,
                                          &did_update));
    return did_update ? 1 : 0;
  }

  static void RunOnUIThread(BookmarkModel* model,
                            const int64_t id,
                            const base::string16& title,
                            const base::string16& url,
                            const int64_t parent_id,
                            bool* did_update) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    const BookmarkNode* node = bookmarks::GetBookmarkNodeByID(model, id);
    if (node) {
      if (node->GetTitle() != title) {
        model->SetTitle(node, title);
        *did_update = true;
      }

      if (node->type() == BookmarkNode::URL) {
        GURL bookmark_url = ParseAndMaybeAppendScheme(url, kDefaultUrlScheme);
        if (bookmark_url != node->url()) {
          model->SetURL(node, bookmark_url);
          *did_update = true;
        }
      }

      if (parent_id >= 0 &&
          (!node->parent() || parent_id != node->parent()->id())) {
        const BookmarkNode* new_parent =
            bookmarks::GetBookmarkNodeByID(model, parent_id);

        if (new_parent) {
          model->Move(node, new_parent, 0);
          *did_update = true;
        }
      }
    }
  }

  DISALLOW_COPY_AND_ASSIGN(UpdateBookmarkTask);
};

// Checks if a node belongs to the Mobile Bookmarks hierarchy branch.
class IsInMobileBookmarksBranchTask : public BookmarkModelTask {
 public:
  explicit IsInMobileBookmarksBranchTask(BookmarkModel* model)
      : BookmarkModelTask(model) {}

  bool Run(const int64_t id) {
    bool result = false;
    RunOnUIThreadBlocking::Run(
        base::Bind(&IsInMobileBookmarksBranchTask::RunOnUIThread,
                   model(), id, &result));
    return result;
  }

  static void RunOnUIThread(BookmarkModel* model,
                            const int64_t id,
                            bool* result) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    DCHECK(result);
    const BookmarkNode* node = bookmarks::GetBookmarkNodeByID(model, id);
    const BookmarkNode* mobile_node = model->mobile_node();
    while (node && node != mobile_node)
      node = node->parent();

    *result = node == mobile_node;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(IsInMobileBookmarksBranchTask);
};

// ------------- Aynchronous requests classes ------------- //

// Base class for asynchronous blocking requests to Chromium services.
// Service: type of the service to use (e.g. HistoryService, FaviconService).
template <typename Service>
class AsyncServiceRequest : protected BlockingUIThreadAsyncRequest {
 public:
  AsyncServiceRequest(Service* service,
                      base::CancelableTaskTracker* cancelable_tracker)
      : service_(service), cancelable_tracker_(cancelable_tracker) {}

  Service* service() const { return service_; }

  base::CancelableTaskTracker* cancelable_tracker() const {
    return cancelable_tracker_;
  }

 private:
  Service* service_;
  base::CancelableTaskTracker* cancelable_tracker_;

  DISALLOW_COPY_AND_ASSIGN(AsyncServiceRequest);
};

// Base class for all asynchronous blocking tasks that use the Android history
// provider service.
class HistoryProviderTask
    : public AsyncServiceRequest<AndroidHistoryProviderService> {
 public:
  HistoryProviderTask(AndroidHistoryProviderService* service,
                      base::CancelableTaskTracker* cancelable_tracker)
      : AsyncServiceRequest<AndroidHistoryProviderService>(service,
                                                           cancelable_tracker) {
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(HistoryProviderTask);
};

// Adds a bookmark from the API.
class AddBookmarkFromAPITask : public HistoryProviderTask {
 public:
  AddBookmarkFromAPITask(AndroidHistoryProviderService* service,
                         base::CancelableTaskTracker* cancelable_tracker)
      : HistoryProviderTask(service, cancelable_tracker) {}

  history::URLID Run(const history::HistoryAndBookmarkRow& row) {
    RunAsyncRequestOnUIThreadBlocking(
        base::Bind(&AndroidHistoryProviderService::InsertHistoryAndBookmark,
                   base::Unretained(service()),
                   row,
                   base::Bind(&AddBookmarkFromAPITask::OnBookmarkInserted,
                              base::Unretained(this)),
                   cancelable_tracker()));
    return result_;
  }

 private:
  void OnBookmarkInserted(history::URLID id) {
    // Note that here 0 means an invalid id.
    // This is because it represents a SQLite database row id.
    result_ = id;
    RequestCompleted();
  }

  history::URLID result_;

  DISALLOW_COPY_AND_ASSIGN(AddBookmarkFromAPITask);
};

// Queries bookmarks from the API.
class QueryBookmarksFromAPITask : public HistoryProviderTask {
 public:
  QueryBookmarksFromAPITask(AndroidHistoryProviderService* service,
                            base::CancelableTaskTracker* cancelable_tracker)
      : HistoryProviderTask(service, cancelable_tracker), result_(NULL) {}

  history::AndroidStatement* Run(
      const std::vector<history::HistoryAndBookmarkRow::ColumnID>& projections,
      const std::string& selection,
      const std::vector<base::string16>& selection_args,
      const std::string& sort_order) {
    RunAsyncRequestOnUIThreadBlocking(
        base::Bind(&AndroidHistoryProviderService::QueryHistoryAndBookmarks,
                   base::Unretained(service()),
                   projections,
                   selection,
                   selection_args,
                   sort_order,
                   base::Bind(&QueryBookmarksFromAPITask::OnBookmarksQueried,
                              base::Unretained(this)),
                   cancelable_tracker()));
    return result_;
  }

 private:
  void OnBookmarksQueried(history::AndroidStatement* statement) {
    result_ = statement;
    RequestCompleted();
  }

  history::AndroidStatement* result_;

  DISALLOW_COPY_AND_ASSIGN(QueryBookmarksFromAPITask);
};

// Updates bookmarks from the API.
class UpdateBookmarksFromAPITask : public HistoryProviderTask {
 public:
  UpdateBookmarksFromAPITask(AndroidHistoryProviderService* service,
                             base::CancelableTaskTracker* cancelable_tracker)
      : HistoryProviderTask(service, cancelable_tracker), result_(0) {}

  int Run(const history::HistoryAndBookmarkRow& row,
          const std::string& selection,
          const std::vector<base::string16>& selection_args) {
    RunAsyncRequestOnUIThreadBlocking(
        base::Bind(&AndroidHistoryProviderService::UpdateHistoryAndBookmarks,
                   base::Unretained(service()),
                   row,
                   selection,
                   selection_args,
                   base::Bind(&UpdateBookmarksFromAPITask::OnBookmarksUpdated,
                              base::Unretained(this)),
                   cancelable_tracker()));
    return result_;
  }

 private:
  void OnBookmarksUpdated(int updated_row_count) {
    result_ = updated_row_count;
    RequestCompleted();
  }

  int result_;

  DISALLOW_COPY_AND_ASSIGN(UpdateBookmarksFromAPITask);
};

// Removes bookmarks from the API.
class RemoveBookmarksFromAPITask : public HistoryProviderTask {
 public:
  RemoveBookmarksFromAPITask(AndroidHistoryProviderService* service,
                             base::CancelableTaskTracker* cancelable_tracker)
      : HistoryProviderTask(service, cancelable_tracker), result_(0) {}

  int Run(const std::string& selection,
          const std::vector<base::string16>& selection_args) {
    RunAsyncRequestOnUIThreadBlocking(
        base::Bind(&AndroidHistoryProviderService::DeleteHistoryAndBookmarks,
                   base::Unretained(service()),
                   selection,
                   selection_args,
                   base::Bind(&RemoveBookmarksFromAPITask::OnBookmarksRemoved,
                              base::Unretained(this)),
                   cancelable_tracker()));
    return result_;
  }

 private:
  void OnBookmarksRemoved(int removed_row_count) {
    result_ = removed_row_count;
    RequestCompleted();
  }

  int result_;

  DISALLOW_COPY_AND_ASSIGN(RemoveBookmarksFromAPITask);
};

// Removes history from the API.
class RemoveHistoryFromAPITask : public HistoryProviderTask {
 public:
  RemoveHistoryFromAPITask(AndroidHistoryProviderService* service,
                           base::CancelableTaskTracker* cancelable_tracker)
      : HistoryProviderTask(service, cancelable_tracker), result_(0) {}

  int Run(const std::string& selection,
          const std::vector<base::string16>& selection_args) {
    RunAsyncRequestOnUIThreadBlocking(
        base::Bind(&AndroidHistoryProviderService::DeleteHistory,
                   base::Unretained(service()),
                   selection,
                   selection_args,
                   base::Bind(&RemoveHistoryFromAPITask::OnHistoryRemoved,
                              base::Unretained(this)),
                   cancelable_tracker()));
    return result_;
  }

 private:
  void OnHistoryRemoved(int removed_row_count) {
    result_ = removed_row_count;
    RequestCompleted();
  }

  int result_;

  DISALLOW_COPY_AND_ASSIGN(RemoveHistoryFromAPITask);
};

// This class provides the common method for the SearchTermAPIHelper.
class SearchTermTask : public HistoryProviderTask {
 protected:
  SearchTermTask(AndroidHistoryProviderService* service,
                 base::CancelableTaskTracker* cancelable_tracker,
                 Profile* profile)
      : HistoryProviderTask(service, cancelable_tracker), profile_(profile) {}

  // Fill SearchRow's keyword_id and url fields according the given
  // search_term. Return true if succeeded.
  void BuildSearchRow(history::SearchRow* row) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);

    TemplateURLService* template_service =
        TemplateURLServiceFactory::GetForProfile(profile_);
    template_service->Load();

    const TemplateURL* search_engine =
        template_service->GetDefaultSearchProvider();
    if (search_engine) {
      const TemplateURLRef* search_url = &search_engine->url_ref();
      TemplateURLRef::SearchTermsArgs search_terms_args(row->search_term());
      search_terms_args.append_extra_query_params_from_command_line = true;
      std::string url = search_url->ReplaceSearchTerms(
          search_terms_args, template_service->search_terms_data());
      if (!url.empty()) {
        row->set_url(GURL(url));
        row->set_keyword_id(search_engine->id());
      }
    }
  }

 private:
  Profile* profile_;

  DISALLOW_COPY_AND_ASSIGN(SearchTermTask);
};

// Adds a search term from the API.
class AddSearchTermFromAPITask : public SearchTermTask {
 public:
  AddSearchTermFromAPITask(AndroidHistoryProviderService* service,
                           base::CancelableTaskTracker* cancelable_tracker,
                           Profile* profile)
      : SearchTermTask(service, cancelable_tracker, profile) {}

  history::URLID Run(const history::SearchRow& row) {
    RunAsyncRequestOnUIThreadBlocking(
        base::Bind(&AddSearchTermFromAPITask::MakeRequestOnUIThread,
                   base::Unretained(this), row));
    return result_;
  }

 private:
  void MakeRequestOnUIThread(const history::SearchRow& row) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    history::SearchRow internal_row = row;
    BuildSearchRow(&internal_row);
    service()->InsertSearchTerm(
        internal_row,
        base::Bind(&AddSearchTermFromAPITask::OnSearchTermInserted,
                   base::Unretained(this)),
        cancelable_tracker());
  }

  void OnSearchTermInserted(history::URLID id) {
    // Note that here 0 means an invalid id.
    // This is because it represents a SQLite database row id.
    result_ = id;
    RequestCompleted();
  }

  history::URLID result_;

  DISALLOW_COPY_AND_ASSIGN(AddSearchTermFromAPITask);
};

// Queries search terms from the API.
class QuerySearchTermsFromAPITask : public SearchTermTask {
 public:
  QuerySearchTermsFromAPITask(AndroidHistoryProviderService* service,
                              base::CancelableTaskTracker* cancelable_tracker,
                              Profile* profile)
      : SearchTermTask(service, cancelable_tracker, profile), result_(NULL) {}

  history::AndroidStatement* Run(
      const std::vector<history::SearchRow::ColumnID>& projections,
      const std::string& selection,
      const std::vector<base::string16>& selection_args,
      const std::string& sort_order) {
    RunAsyncRequestOnUIThreadBlocking(base::Bind(
        &AndroidHistoryProviderService::QuerySearchTerms,
        base::Unretained(service()),
        projections,
        selection,
        selection_args,
        sort_order,
        base::Bind(&QuerySearchTermsFromAPITask::OnSearchTermsQueried,
                   base::Unretained(this)),
        cancelable_tracker()));
    return result_;
  }

 private:
  // Callback to return the result.
  void OnSearchTermsQueried(history::AndroidStatement* statement) {
    result_ = statement;
    RequestCompleted();
  }

  history::AndroidStatement* result_;

  DISALLOW_COPY_AND_ASSIGN(QuerySearchTermsFromAPITask);
};

// Updates search terms from the API.
class UpdateSearchTermsFromAPITask : public SearchTermTask {
 public:
  UpdateSearchTermsFromAPITask(AndroidHistoryProviderService* service,
                               base::CancelableTaskTracker* cancelable_tracker,
                               Profile* profile)
      : SearchTermTask(service, cancelable_tracker, profile), result_(0) {}

  int Run(const history::SearchRow& row,
          const std::string& selection,
          const std::vector<base::string16>& selection_args) {
    RunAsyncRequestOnUIThreadBlocking(
        base::Bind(&UpdateSearchTermsFromAPITask::MakeRequestOnUIThread,
                   base::Unretained(this), row, selection, selection_args));
    return result_;
  }

 private:
  void MakeRequestOnUIThread(
      const history::SearchRow& row,
      const std::string& selection,
      const std::vector<base::string16>& selection_args) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    history::SearchRow internal_row = row;
    BuildSearchRow(&internal_row);
    service()->UpdateSearchTerms(
        internal_row,
        selection,
        selection_args,
        base::Bind(&UpdateSearchTermsFromAPITask::OnSearchTermsUpdated,
                   base::Unretained(this)),
        cancelable_tracker());
  }

  void OnSearchTermsUpdated(int updated_row_count) {
    result_ = updated_row_count;
    RequestCompleted();
  }

  int result_;

  DISALLOW_COPY_AND_ASSIGN(UpdateSearchTermsFromAPITask);
};

// Removes search terms from the API.
class RemoveSearchTermsFromAPITask : public SearchTermTask {
 public:
  RemoveSearchTermsFromAPITask(AndroidHistoryProviderService* service,
                               base::CancelableTaskTracker* cancelable_tracker,
                               Profile* profile)
      : SearchTermTask(service, cancelable_tracker, profile), result_() {}

  int Run(const std::string& selection,
          const std::vector<base::string16>& selection_args) {
    RunAsyncRequestOnUIThreadBlocking(base::Bind(
        &AndroidHistoryProviderService::DeleteSearchTerms,
        base::Unretained(service()),
        selection,
        selection_args,
        base::Bind(&RemoveSearchTermsFromAPITask::OnSearchTermsDeleted,
                   base::Unretained(this)),
        cancelable_tracker()));
    return result_;
  }

 private:
  void OnSearchTermsDeleted(int deleted_row_count) {
    result_ = deleted_row_count;
    RequestCompleted();
  }

  int result_;

  DISALLOW_COPY_AND_ASSIGN(RemoveSearchTermsFromAPITask);
};

// ------------- Other utility methods (may use tasks) ------------- //

// Fills the bookmark |row| with the given java objects.
void JNI_ChromeBrowserProvider_FillBookmarkRow(
    JNIEnv* env,
    const JavaRef<jobject>& obj,
    const JavaRef<jstring>& url,
    const JavaRef<jobject>& created,
    const JavaRef<jobject>& isBookmark,
    const JavaRef<jobject>& date,
    const JavaRef<jbyteArray>& favicon,
    const JavaRef<jstring>& title,
    const JavaRef<jobject>& visits,
    jlong parent_id,
    history::HistoryAndBookmarkRow* row,
    BookmarkModel* model) {
  // Needed because of the internal bookmark model task invocation.
  DCHECK(!BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (!url.is_null()) {
    base::string16 raw_url = ConvertJavaStringToUTF16(env, url);
    // GURL doesn't accept the URL without protocol, but the Android CTS
    // allows it. We are trying to prefix with 'http://' to see whether
    // GURL thinks it is a valid URL. The original url will be stored in
    // history::BookmarkRow.raw_url_.
    GURL gurl = ParseAndMaybeAppendScheme(raw_url, kDefaultUrlScheme);
    row->set_url(gurl);
    row->set_raw_url(base::UTF16ToUTF8(raw_url));
  }

  if (!created.is_null())
    row->set_created(ConvertJlongToTime(
        JNI_ChromeBrowserProvider_ConvertJLongObjectToPrimitive(env, created)));

  if (!isBookmark.is_null())
    row->set_is_bookmark(
        JNI_ChromeBrowserProvider_ConvertJBooleanObjectToPrimitive(env,
                                                                   isBookmark));

  if (!date.is_null())
    row->set_last_visit_time(ConvertJlongToTime(
        JNI_ChromeBrowserProvider_ConvertJLongObjectToPrimitive(env, date)));

  if (!favicon.is_null()) {
    std::vector<uint8_t> bytes;
    base::android::JavaByteArrayToByteVector(env, favicon, &bytes);
    row->set_favicon(base::RefCountedBytes::TakeVector(&bytes));
  }

  if (!title.is_null())
    row->set_title(ConvertJavaStringToUTF16(env, title));

  if (!visits.is_null())
    row->set_visit_count(
        JNI_ChromeBrowserProvider_ConvertJIntegerToJint(env, visits));

  // Make sure parent_id is always in the mobile_node branch.
  IsInMobileBookmarksBranchTask task(model);
  if (task.Run(parent_id))
    row->set_parent_id(parent_id);
}

// Fills the bookmark |row| with the given java objects if it is not null.
void JNI_ChromeBrowserProvider_FillSearchRow(
    JNIEnv* env,
    const JavaRef<jobject>& obj,
    const JavaRef<jstring>& search_term,
    const JavaRef<jobject>& date,
    history::SearchRow* row) {
  if (!search_term.is_null())
    row->set_search_term(ConvertJavaStringToUTF16(env, search_term));

  if (!date.is_null())
    row->set_search_time(ConvertJlongToTime(
        JNI_ChromeBrowserProvider_ConvertJLongObjectToPrimitive(env, date)));
}

}  // namespace

// ------------- Native initialization and destruction ------------- //

static jlong JNI_ChromeBrowserProvider_Init(JNIEnv* env,
                                            const JavaParamRef<jobject>& obj) {
  ChromeBrowserProvider* provider = new ChromeBrowserProvider(env, obj);
  return reinterpret_cast<intptr_t>(provider);
}

ChromeBrowserProvider::ChromeBrowserProvider(JNIEnv* env, jobject obj)
    : weak_java_provider_(env, obj),
      history_service_observer_(this),
      handling_extensive_changes_(false) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  profile_ = g_browser_process->profile_manager()->GetLastUsedProfile();
  bookmark_model_ = BookmarkModelFactory::GetForBrowserContext(profile_);
  top_sites_ = TopSitesFactory::GetForProfile(profile_);
  favicon_service_ = FaviconServiceFactory::GetForProfile(
      profile_, ServiceAccessType::EXPLICIT_ACCESS),
  service_.reset(new AndroidHistoryProviderService(profile_));

  // Register as observer for service we are interested.
  bookmark_model_->AddObserver(this);
  history_service_observer_.Add(HistoryServiceFactory::GetForProfile(
      profile_, ServiceAccessType::EXPLICIT_ACCESS));
  TemplateURLService* template_service =
        TemplateURLServiceFactory::GetForProfile(profile_);
  if (!template_service->loaded())
    template_service->Load();
}

ChromeBrowserProvider::~ChromeBrowserProvider() {
  bookmark_model_->RemoveObserver(this);
}

void ChromeBrowserProvider::Destroy(JNIEnv*, const JavaParamRef<jobject>&) {
  history_service_observer_.RemoveAll();
  delete this;
}

// ------------- Provider public APIs ------------- //

jlong ChromeBrowserProvider::AddBookmark(JNIEnv* env,
                                         const JavaParamRef<jobject>&,
                                         const JavaParamRef<jstring>& jurl,
                                         const JavaParamRef<jstring>& jtitle,
                                         jboolean is_folder,
                                         jlong parent_id) {
  base::string16 url;
  if (jurl)
    url = ConvertJavaStringToUTF16(env, jurl);
  base::string16 title = ConvertJavaStringToUTF16(env, jtitle);

  AddBookmarkTask task(bookmark_model_);
  return task.Run(title, url, is_folder, parent_id);
}

jint ChromeBrowserProvider::RemoveBookmark(JNIEnv*,
                                           const JavaParamRef<jobject>&,
                                           jlong id) {
  RemoveBookmarkTask task(bookmark_model_);
  return task.Run(id);
}

jint ChromeBrowserProvider::UpdateBookmark(JNIEnv* env,
                                           const JavaParamRef<jobject>&,
                                           jlong id,
                                           const JavaParamRef<jstring>& jurl,
                                           const JavaParamRef<jstring>& jtitle,
                                           jlong parent_id) {
  base::string16 url;
  if (jurl)
    url = ConvertJavaStringToUTF16(env, jurl);
  base::string16 title = ConvertJavaStringToUTF16(env, jtitle);

  UpdateBookmarkTask task(bookmark_model_);
  return task.Run(id, title, url, parent_id);
}

// Add the bookmark with the given column values.
jlong ChromeBrowserProvider::AddBookmarkFromAPI(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jstring>& url,
    const JavaParamRef<jobject>& created,
    const JavaParamRef<jobject>& isBookmark,
    const JavaParamRef<jobject>& date,
    const JavaParamRef<jbyteArray>& favicon,
    const JavaParamRef<jstring>& title,
    const JavaParamRef<jobject>& visits,
    jlong parent_id) {
  DCHECK(url);

  history::HistoryAndBookmarkRow row;
  JNI_ChromeBrowserProvider_FillBookmarkRow(env, obj, url, created, isBookmark,
                                            date, favicon, title, visits,
                                            parent_id, &row, bookmark_model_);

  // URL must be valid.
  if (row.url().is_empty()) {
    LOG(ERROR) << "Not a valid URL " << row.raw_url();
    return kInvalidContentProviderId;
  }

  AddBookmarkFromAPITask task(service_.get(), &cancelable_task_tracker_);
  return task.Run(row);
}

ScopedJavaLocalRef<jobject> ChromeBrowserProvider::QueryBookmarkFromAPI(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jobjectArray>& projection,
    const JavaParamRef<jstring>& selections,
    const JavaParamRef<jobjectArray>& selection_args,
    const JavaParamRef<jstring>& sort_order) {
  // Converts the projection to array of ColumnID and column name.
  // Used to store the projection column ID according their sequence.
  std::vector<history::HistoryAndBookmarkRow::ColumnID> query_columns;
  // Used to store the projection column names according their sequence.
  std::vector<std::string> columns_name;
  if (projection) {
    jsize len = env->GetArrayLength(projection);
    for (int i = 0; i < len; i++) {
      ScopedJavaLocalRef<jstring> j_name(
          env, static_cast<jstring>(env->GetObjectArrayElement(projection, i)));
      std::string name = ConvertJavaStringToUTF8(env, j_name);
      history::HistoryAndBookmarkRow::ColumnID id =
          history::HistoryAndBookmarkRow::GetColumnID(name);
      if (id == history::HistoryAndBookmarkRow::COLUMN_END) {
        // Ignore the unknown column; As Android platform will send us
        // the non public column.
        continue;
      }
      query_columns.push_back(id);
      columns_name.push_back(name);
    }
  }

  std::vector<base::string16> where_args =
      ConvertJStringArrayToString16Array(env, selection_args);

  std::string where_clause;
  if (selections) {
    where_clause = ConvertJavaStringToUTF8(env, selections);
  }

  std::string sort_clause;
  if (sort_order) {
    sort_clause = ConvertJavaStringToUTF8(env, sort_order);
  }

  QueryBookmarksFromAPITask task(service_.get(), &cancelable_task_tracker_);
  history::AndroidStatement* statement = task.Run(
      query_columns, where_clause, where_args, sort_clause);
  if (!statement)
    return ScopedJavaLocalRef<jobject>();

  // Creates and returns org.chromium.chrome.browser.database.SQLiteCursor
  // Java object.
  return SQLiteCursor::NewJavaSqliteCursor(env, columns_name, statement,
             service_.get());
}

// Updates the bookmarks with the given column values. The value is not given if
// it is NULL.
jint ChromeBrowserProvider::UpdateBookmarkFromAPI(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jstring>& url,
    const JavaParamRef<jobject>& created,
    const JavaParamRef<jobject>& isBookmark,
    const JavaParamRef<jobject>& date,
    const JavaParamRef<jbyteArray>& favicon,
    const JavaParamRef<jstring>& title,
    const JavaParamRef<jobject>& visits,
    jlong parent_id,
    const JavaParamRef<jstring>& selections,
    const JavaParamRef<jobjectArray>& selection_args) {
  history::HistoryAndBookmarkRow row;
  JNI_ChromeBrowserProvider_FillBookmarkRow(env, obj, url, created, isBookmark,
                                            date, favicon, title, visits,
                                            parent_id, &row, bookmark_model_);

  std::vector<base::string16> where_args =
      ConvertJStringArrayToString16Array(env, selection_args);

  std::string where_clause;
  if (selections)
    where_clause = ConvertJavaStringToUTF8(env, selections);

  UpdateBookmarksFromAPITask task(service_.get(), &cancelable_task_tracker_);
  return task.Run(row, where_clause, where_args);
}

jint ChromeBrowserProvider::RemoveBookmarkFromAPI(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jstring>& selections,
    const JavaParamRef<jobjectArray>& selection_args) {
  std::vector<base::string16> where_args =
      ConvertJStringArrayToString16Array(env, selection_args);

  std::string where_clause;
  if (selections)
    where_clause = ConvertJavaStringToUTF8(env, selections);

  RemoveBookmarksFromAPITask task(service_.get(), &cancelable_task_tracker_);
  return task.Run(where_clause, where_args);
}

jint ChromeBrowserProvider::RemoveHistoryFromAPI(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jstring>& selections,
    const JavaParamRef<jobjectArray>& selection_args) {
  std::vector<base::string16> where_args =
      ConvertJStringArrayToString16Array(env, selection_args);

  std::string where_clause;
  if (selections)
    where_clause = ConvertJavaStringToUTF8(env, selections);

  RemoveHistoryFromAPITask task(service_.get(), &cancelable_task_tracker_);
  return task.Run(where_clause, where_args);
}

// Add the search term with the given column values. The value is not given if
// it is NULL.
jlong ChromeBrowserProvider::AddSearchTermFromAPI(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jstring>& search_term,
    const JavaParamRef<jobject>& date) {
  DCHECK(search_term);

  history::SearchRow row;
  JNI_ChromeBrowserProvider_FillSearchRow(env, obj, search_term, date, &row);

  // URL must be valid.
  if (row.search_term().empty()) {
    LOG(ERROR) << "Search term is empty.";
    return kInvalidContentProviderId;
  }

  AddSearchTermFromAPITask task(service_.get(),
                                &cancelable_task_tracker_,
                                profile_);
  return task.Run(row);
}

ScopedJavaLocalRef<jobject> ChromeBrowserProvider::QuerySearchTermFromAPI(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jobjectArray>& projection,
    const JavaParamRef<jstring>& selections,
    const JavaParamRef<jobjectArray>& selection_args,
    const JavaParamRef<jstring>& sort_order) {
  // Converts the projection to array of ColumnID and column name.
  // Used to store the projection column ID according their sequence.
  std::vector<history::SearchRow::ColumnID> query_columns;
  // Used to store the projection column names according their sequence.
  std::vector<std::string> columns_name;
  if (projection) {
    jsize len = env->GetArrayLength(projection);
    for (int i = 0; i < len; i++) {
      ScopedJavaLocalRef<jstring> j_name(
          env, static_cast<jstring>(env->GetObjectArrayElement(projection, i)));
      std::string name = ConvertJavaStringToUTF8(env, j_name);
      history::SearchRow::ColumnID id =
          history::SearchRow::GetColumnID(name);
      if (id == history::SearchRow::COLUMN_END) {
        LOG(ERROR) << "Can not find " << name;
        return ScopedJavaLocalRef<jobject>();
      }
      query_columns.push_back(id);
      columns_name.push_back(name);
    }
  }

  std::vector<base::string16> where_args =
      ConvertJStringArrayToString16Array(env, selection_args);

  std::string where_clause;
  if (selections) {
    where_clause = ConvertJavaStringToUTF8(env, selections);
  }

  std::string sort_clause;
  if (sort_order) {
    sort_clause = ConvertJavaStringToUTF8(env, sort_order);
  }

  QuerySearchTermsFromAPITask task(service_.get(),
                                   &cancelable_task_tracker_,
                                   profile_);
  history::AndroidStatement* statement = task.Run(
      query_columns, where_clause, where_args, sort_clause);
  if (!statement)
    return ScopedJavaLocalRef<jobject>();
  // Creates and returns org.chromium.chrome.browser.database.SQLiteCursor
  // Java object.
  return SQLiteCursor::NewJavaSqliteCursor(env, columns_name, statement,
             service_.get());
}

// Updates the search terms with the given column values. The value is not
// given if it is NULL.
jint ChromeBrowserProvider::UpdateSearchTermFromAPI(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jstring>& search_term,
    const JavaParamRef<jobject>& date,
    const JavaParamRef<jstring>& selections,
    const JavaParamRef<jobjectArray>& selection_args) {
  history::SearchRow row;
  JNI_ChromeBrowserProvider_FillSearchRow(env, obj, search_term, date, &row);

  std::vector<base::string16> where_args = ConvertJStringArrayToString16Array(
      env, selection_args);

  std::string where_clause;
  if (selections)
    where_clause = ConvertJavaStringToUTF8(env, selections);

  UpdateSearchTermsFromAPITask task(service_.get(),
                                    &cancelable_task_tracker_,
                                    profile_);
  return task.Run(row, where_clause, where_args);
}

jint ChromeBrowserProvider::RemoveSearchTermFromAPI(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jstring>& selections,
    const JavaParamRef<jobjectArray>& selection_args) {
  std::vector<base::string16> where_args =
      ConvertJStringArrayToString16Array(env, selection_args);

  std::string where_clause;
  if (selections)
    where_clause = ConvertJavaStringToUTF8(env, selections);

  RemoveSearchTermsFromAPITask task(service_.get(),
                                    &cancelable_task_tracker_,
                                    profile_);
  return task.Run(where_clause, where_args);
}

// ------------- Observer-related methods ------------- //

void ChromeBrowserProvider::ExtensiveBookmarkChangesBeginning(
    BookmarkModel* model) {
  handling_extensive_changes_ = true;
}

void ChromeBrowserProvider::ExtensiveBookmarkChangesEnded(
    BookmarkModel* model) {
  handling_extensive_changes_ = false;
  BookmarkModelChanged();
}

void ChromeBrowserProvider::BookmarkModelChanged() {
  if (handling_extensive_changes_)
    return;

  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = weak_java_provider_.get(env);
  if (obj.is_null())
    return;

  Java_ChromeBrowserProvider_onBookmarkChanged(env, obj);
}

void ChromeBrowserProvider::OnHistoryChanged() {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = weak_java_provider_.get(env);
  if (obj.is_null())
    return;
  Java_ChromeBrowserProvider_onHistoryChanged(env, obj);
}

void ChromeBrowserProvider::OnURLVisited(
    history::HistoryService* history_service,
    ui::PageTransition transition,
    const history::URLRow& row,
    const history::RedirectList& redirects,
    base::Time visit_time) {
  OnHistoryChanged();
}

void ChromeBrowserProvider::OnURLsDeleted(
    history::HistoryService* history_service,
    const history::DeletionInfo& deletion_info) {
  OnHistoryChanged();
}

void ChromeBrowserProvider::OnKeywordSearchTermUpdated(
    history::HistoryService* history_service,
    const history::URLRow& row,
    history::KeywordID keyword_id,
    const base::string16& term) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = weak_java_provider_.get(env);
  if (obj.is_null())
    return;
  Java_ChromeBrowserProvider_onSearchTermChanged(env, obj);
}

void ChromeBrowserProvider::OnKeywordSearchTermDeleted(
    history::HistoryService* history_service,
    history::URLID url_id) {
}
