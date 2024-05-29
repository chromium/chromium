// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/bookmarks/partner_bookmarks_reader.h"

#include <utility>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread_restrictions.h"
#include "base/uuid.h"
#include "chrome/browser/android/bookmarks/partner_bookmarks_shim.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "chrome/browser/favicon/large_icon_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/favicon/core/favicon_service.h"
#include "components/favicon/core/large_icon_service_impl.h"
#include "components/image_fetcher/core/image_fetcher.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/favicon_size.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/android/chrome_jni_headers/PartnerBookmarksReader_jni.h"

using base::android::AttachCurrentThread;
using base::android::CheckException;
using base::android::ConvertJavaStringToUTF8;
using base::android::ConvertJavaStringToUTF16;
using base::android::ConvertUTF8ToJavaString;
using base::android::JavaParamRef;
using base::android::JavaRef;
using base::android::ScopedJavaGlobalRef;
using bookmarks::BookmarkNode;
using content::BrowserThread;

namespace {

const int kPartnerBookmarksMinimumFaviconSizePx = 16;

void SetFaviconTask(Profile* profile,
                    const GURL& page_url, const GURL& icon_url,
                    const std::vector<unsigned char>& image_data,
                    favicon_base::IconType icon_type) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  scoped_refptr<base::RefCountedMemory> bitmap_data(
      new base::RefCountedBytes(image_data));
  gfx::Size pixel_size(gfx::kFaviconSize, gfx::kFaviconSize);
  favicon::FaviconService* favicon_service =
      FaviconServiceFactory::GetForProfile(profile,
                                           ServiceAccessType::EXPLICIT_ACCESS);
  if (!favicon_service)
    return;

  favicon_service->MergeFavicon(
      page_url, page_url, icon_type, bitmap_data, pixel_size);
}

void SetFaviconCallback(Profile* profile,
                        const GURL& page_url, const GURL& icon_url,
                        const std::vector<unsigned char>& image_data,
                        favicon_base::IconType icon_type,
                        base::WaitableEvent* bookmark_added_event) {
  SetFaviconTask(profile, page_url, icon_url, image_data, icon_type);
  if (bookmark_added_event)
    bookmark_added_event->Signal();
}

const BookmarkNode* GetNodeByID(const BookmarkNode* parent, int64_t id) {
  if (parent->id() == id)
    return parent;
  for (const auto& child : parent->children()) {
    const BookmarkNode* result = GetNodeByID(child.get(), id);
    if (result)
      return result;
  }
  return nullptr;
}

std::unique_ptr<BookmarkNode> CreatePartnerBookmarksRoot(int id) {
  return std::make_unique<BookmarkNode>(id, base::Uuid::GenerateRandomV4(),
                                        GURL());
}

}  // namespace

PartnerBookmarksReader::PartnerBookmarksReader(
    PartnerBookmarksShim* partner_bookmarks_shim,
    Profile* profile)
    : partner_bookmarks_shim_(partner_bookmarks_shim),
      profile_(profile),
      large_icon_service_(nullptr),
      wip_next_available_id_(0) {}

PartnerBookmarksReader::~PartnerBookmarksReader() {}

void PartnerBookmarksReader::PartnerBookmarksCreationComplete(
    JNIEnv*,
    const JavaParamRef<jobject>&) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  partner_bookmarks_shim_->SetPartnerBookmarksRoot(
      std::move(wip_partner_bookmarks_root_));
  wip_next_available_id_ = 0;
}

void PartnerBookmarksReader::Destroy(JNIEnv* env,
                                     const JavaParamRef<jobject>& obj) {
  delete this;
}

void PartnerBookmarksReader::Reset(JNIEnv* env,
                                   const JavaParamRef<jobject>& obj) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  wip_partner_bookmarks_root_.reset();
  wip_next_available_id_ = 0;
}

jlong PartnerBookmarksReader::AddPartnerBookmark(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jstring>& jurl,
    const JavaParamRef<jstring>& jtitle,
    jboolean is_folder,
    jlong parent_id,
    const JavaParamRef<jbyteArray>& favicon,
    const JavaParamRef<jbyteArray>& touchicon,
    jboolean fetch_uncached_favicons_from_server,
    jint desired_favicon_size_px,
    const JavaParamRef<jobject>& j_callback) {
  std::u16string url;
  std::u16string title;
  if (jurl) {
    DCHECK(!is_folder);
    url = ConvertJavaStringToUTF16(env, jurl);
  }
  if (jtitle)
    title = ConvertJavaStringToUTF16(env, jtitle);

  jlong node_id = 0;
  if (wip_partner_bookmarks_root_.get()) {
    std::unique_ptr<BookmarkNode> node = std::make_unique<BookmarkNode>(
        wip_next_available_id_++, base::Uuid::GenerateRandomV4(), GURL(url));
    node->SetTitle(title);

    // Handle favicon and touchicon
    if (profile_ != nullptr) {
      if (favicon != nullptr || touchicon != nullptr) {
        jbyteArray icon = (touchicon != nullptr) ? touchicon : favicon;
        const favicon_base::IconType icon_type =
            touchicon ? favicon_base::IconType::kTouchIcon
                      : favicon_base::IconType::kFavicon;
        const int icon_len = env->GetArrayLength(icon);
        jbyte* icon_bytes = env->GetByteArrayElements(icon, nullptr);
        if (icon_bytes)
          PrepareAndSetFavicon(icon_bytes, icon_len, node.get(), profile_,
                               icon_type);
        env->ReleaseByteArrayElements(icon, icon_bytes, JNI_ABORT);
      } else {
        // We should attempt to read the favicon from cache or retrieve it from
        // a server and cache it.
        Java_FetchFaviconCallback_onFaviconFetch(env, j_callback);
        GetFavicon(
            GURL(url), profile_, fetch_uncached_favicons_from_server,
            desired_favicon_size_px,
            base::BindOnce(&PartnerBookmarksReader::OnFaviconFetched,
                           base::Unretained(this),
                           ScopedJavaGlobalRef<jobject>(env, j_callback)));
      }
    }

    const BookmarkNode* parent =
        GetNodeByID(wip_partner_bookmarks_root_.get(), parent_id);
    if (!parent) {
      LOG(WARNING) << "partner_bookmarks_shim: invalid/unknown parent_id="
                   << parent_id << ": adding to the root";
      parent = wip_partner_bookmarks_root_.get();
    }
    node_id = node->id();
    const_cast<BookmarkNode*>(parent)->Add(std::move(node));
  } else {
    std::unique_ptr<BookmarkNode> node =
        CreatePartnerBookmarksRoot(wip_next_available_id_++);
    node_id = node->id();
    node->SetTitle(title);
    wip_partner_bookmarks_root_ = std::move(node);
  }
  return node_id;
}

// static
std::unique_ptr<BookmarkNode>
PartnerBookmarksReader::CreatePartnerBookmarksRootForTesting() {
  return CreatePartnerBookmarksRoot(/*id=*/0);
}

void PartnerBookmarksReader::GetFavicon(const GURL& page_url,
                                        Profile* profile,
                                        bool fallback_to_server,
                                        int desired_favicon_size_px,
                                        FaviconFetchedCallback callback) {
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&PartnerBookmarksReader::GetFaviconImpl,
                                base::Unretained(this), page_url, profile,
                                fallback_to_server, desired_favicon_size_px,
                                std::move(callback)));
}

void PartnerBookmarksReader::GetFaviconImpl(const GURL& page_url,
                                            Profile* profile,
                                            bool fallback_to_server,
                                            int desired_favicon_size_px,
                                            FaviconFetchedCallback callback) {
  if (!GetLargeIconService()) {
    std::move(callback).Run(
        FaviconFetchResult::FAILURE_ICON_SERVICE_UNAVAILABLE);
    return;
  }

  GetFaviconFromCacheOrServer(page_url, fallback_to_server,
                              false /* from_server */, desired_favicon_size_px,
                              std::move(callback));
}

favicon::LargeIconService* PartnerBookmarksReader::GetLargeIconService() {
  if (!large_icon_service_) {
    large_icon_service_ =
        LargeIconServiceFactory::GetForBrowserContext(profile_);
  }
  return large_icon_service_;
}

void PartnerBookmarksReader::GetFaviconFromCacheOrServer(
    const GURL& page_url,
    bool fallback_to_server,
    bool from_server,
    int desired_favicon_size_px,
    FaviconFetchedCallback callback) {
  GetLargeIconService()->GetLargeIconRawBitmapOrFallbackStyleForPageUrl(
      page_url, kPartnerBookmarksMinimumFaviconSizePx, desired_favicon_size_px,
      base::BindOnce(&PartnerBookmarksReader::OnGetFaviconFromCacheFinished,
                     base::Unretained(this), page_url, std::move(callback),
                     fallback_to_server, from_server, desired_favicon_size_px),
      &favicon_task_tracker_);
}

void PartnerBookmarksReader::OnGetFaviconFromCacheFinished(
    const GURL& page_url,
    FaviconFetchedCallback callback,
    bool fallback_to_server,
    bool from_server,
    int desired_favicon_size_px,
    const favicon_base::LargeIconResult& result) {
  // |from_server| tells us if we fetched the image from the cache after we went
  // to server for it, so this successful cache retrieval should actually return
  // SUCCESS_FROM_SERVER.
  if (result.bitmap.is_valid()) {
    if (from_server) {
      std::move(callback).Run(FaviconFetchResult::SUCCESS_FROM_SERVER);
    } else {
      std::move(callback).Run(FaviconFetchResult::SUCCESS_FROM_CACHE);
    }
    return;
  }

  // We chose not to fetch from server if the cache failed, so return an empty
  // image.
  if (!fallback_to_server) {
    std::move(callback).Run(FaviconFetchResult::FAILURE_NOT_IN_CACHE);
    return;
  }

  // Try to fetch the favicon from a Google favicon server.
  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation(
          "partner_bookmarks_reader_get_favicon", R"(
        semantics {
          sender: "Partner Bookmarks Reader"
          description:
            "Sends a request to a Google server to retrieve the favicon bitmap "
            "for a partner bookmark (URLs are public and provided by Google)."
          trigger:
            "A request can be sent if Chrome does not have a favicon for a "
            "particular partner bookmark."
          data: "Page URL and desired icon size."
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: NO
          setting: "This feature cannot be disabled by settings."
          policy_exception_justification: "Not implemented."
        })");
  GetLargeIconService()
      ->GetLargeIconOrFallbackStyleFromGoogleServerSkippingLocalCache(
          page_url, false /* should_trim_page_url_path */, traffic_annotation,
          base::BindOnce(
              &PartnerBookmarksReader::OnGetFaviconFromServerFinished,
              base::Unretained(this), page_url, desired_favicon_size_px,
              std::move(callback)));
}

void PartnerBookmarksReader::OnGetFaviconFromServerFinished(
    const GURL& page_url,
    int desired_favicon_size_px,
    FaviconFetchedCallback callback,
    favicon_base::GoogleFaviconServerRequestStatus status) {
  if (status != favicon_base::GoogleFaviconServerRequestStatus::SUCCESS) {
    FaviconFetchResult result;
    if (status == favicon_base::GoogleFaviconServerRequestStatus::
                      FAILURE_CONNECTION_ERROR) {
      result = FaviconFetchResult::FAILURE_CONNECTION_ERROR;
    } else if (status == favicon_base::GoogleFaviconServerRequestStatus::
                             FAILURE_ON_WRITE) {
      result = FaviconFetchResult::FAILURE_WRITING_FAVICON_CACHE;
    } else {
      result = FaviconFetchResult::FAILURE_SERVER_ERROR;
    }
    std::move(callback).Run(result);
    return;
  }

  // The icon was successfully retrieved from the server, now we just have to
  // retrieve it from the cache where it was stored.
  GetFaviconFromCacheOrServer(page_url, false /* fallback_to_server */,
                              true /* from_server */, desired_favicon_size_px,
                              std::move(callback));
}

void PartnerBookmarksReader::OnFaviconFetched(
    const JavaRef<jobject>& j_callback,
    FaviconFetchResult result) {
  JNIEnv* env = AttachCurrentThread();
  Java_FetchFaviconCallback_onFaviconFetched(env, j_callback,
                                             static_cast<int>(result));
}

// static
void PartnerBookmarksReader::PrepareAndSetFavicon(
    jbyte* icon_bytes,
    int icon_len,
    BookmarkNode* node,
    Profile* profile,
    favicon_base::IconType icon_type) {
  SkBitmap icon_bitmap;
  if (!gfx::PNGCodec::Decode(reinterpret_cast<const unsigned char*>(icon_bytes),
                             icon_len, &icon_bitmap)) {
    return;
  }
  std::vector<unsigned char> image_data;
  if (!gfx::PNGCodec::EncodeBGRASkBitmap(icon_bitmap, false, &image_data)) {
    return;
  }
  // Since the bookmark URL is used as a key in the history's thumbnail DB,
  // this gives us a value which does not collide with others.
  GURL fake_icon_url = node->url();

  base::WaitableEvent event(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                            base::WaitableEvent::InitialState::NOT_SIGNALED);
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&SetFaviconCallback, profile, node->url(),
                                fake_icon_url, image_data, icon_type, &event));

  {
    // Waiting is okay here because this is called from a background thread
    // in Java.
    DCHECK(!::content::BrowserThread::CurrentlyOn(BrowserThread::UI));
    base::ScopedAllowBaseSyncPrimitives allow_sync;
    event.Wait();
  }
}

// ----------------------------------------------------------------

static void JNI_PartnerBookmarksReader_DisablePartnerBookmarksEditing(
    JNIEnv* env) {
  PartnerBookmarksShim::DisablePartnerBookmarksEditing();
}

static jlong JNI_PartnerBookmarksReader_Init(JNIEnv* env,
                                             const JavaParamRef<jobject>& obj) {
  Profile* profile = ProfileManager::GetActiveUserProfile();
  PartnerBookmarksShim* partner_bookmarks_shim =
      PartnerBookmarksShim::BuildForBrowserContext(profile);
  PartnerBookmarksReader* reader = new PartnerBookmarksReader(
      partner_bookmarks_shim, profile);
  return reinterpret_cast<intptr_t>(reader);
}

static base::android::ScopedJavaLocalRef<jstring>
JNI_PartnerBookmarksReader_GetNativeUrlString(
    JNIEnv* env,
    const JavaParamRef<jstring>& j_url) {
  GURL url(ConvertJavaStringToUTF8(j_url));
  return ConvertUTF8ToJavaString(env, url.spec());
}
