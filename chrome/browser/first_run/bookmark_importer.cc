// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/first_run/bookmark_importer.h"

#include <memory>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "chrome/browser/first_run/first_run_internal.h"
#include "chrome/browser/image_fetcher/image_fetcher_service_factory.h"
#include "chrome/browser/profiles/keep_alive/profile_keep_alive_types.h"
#include "chrome/browser/profiles/keep_alive/scoped_profile_keep_alive.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_key.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_model_load_waiter.h"
#include "components/favicon/core/favicon_service.h"
#include "components/image_fetcher/core/image_decoder.h"
#include "components/image_fetcher/core/image_fetcher.h"
#include "components/image_fetcher/core/image_fetcher_service.h"
#include "components/keyed_service/core/service_access_type.h"
#include "net/base/data_url.h"
#include "third_party/abseil-cpp/absl/cleanup/cleanup.h"

using first_run::internal::FirstRunImportBookmarksResult;

namespace {

// Keys required for importing bookmarks from Initial Preferences on First Run.
constexpr char kImportBookmarksChildrenKey[] = "children";
constexpr char kImportBookmarksTypeKey[] = "type";
constexpr char kImportBookmarksNameKey[] = "name";
constexpr char kImportBookmarksUrlKey[] = "url";
constexpr char kImportBookmarksIconDataUrlKey[] = "icon_data_url";
constexpr char kImportBookmarksFolderType[] = "folder";
constexpr char kImportBookmarksUrlType[] = "url";
constexpr char kImportBookmarksBookmarksKey[] = "first_run_bookmarks";

void RecordImportBookmarksResult(FirstRunImportBookmarksResult result) {
  base::UmaHistogramEnumeration("FirstRun.ImportBookmarksDict", result);
}

class FirstRunBookmarkImporter {
 public:
  static void Import(Profile& profile,
                     const base::Value::Dict& bookmarks_dict,
                     bookmarks::BookmarkModel& bookmark_model) {
    FirstRunBookmarkImporter(profile, bookmark_model)
        .BeginImport(bookmarks_dict);
  }

 private:
  FirstRunBookmarkImporter(Profile& profile,
                           bookmarks::BookmarkModel& bookmark_model)
      : profile_(profile), bookmark_model_(bookmark_model) {
    image_fetcher::ImageFetcherService* image_fetcher_service =
        ImageFetcherServiceFactory::GetForKey(profile_->GetProfileKey());

    if (image_fetcher_service) {
      image_decoder_ =
          image_fetcher_service
              ->GetImageFetcher(image_fetcher::ImageFetcherConfig::kNetworkOnly)
              ->GetImageDecoder();
    }
  }

  void BeginImport(const base::Value::Dict& bookmarks_to_import) const {
    bookmark_model_->BeginExtensiveChanges();
    absl::Cleanup end_changes = [this] {
      bookmark_model_->EndExtensiveChanges();
    };

    const bookmarks::BookmarkNode* parent =
        bookmark_model_->bookmark_bar_node();
    ImportBookmarksAndFoldersRecursively(bookmarks_to_import, parent);

    RecordImportBookmarksResult(FirstRunImportBookmarksResult::kSuccess);
  }

  // Recursive helper that walks the JSON dictionary and creates matching
  // bookmark nodes.
  void ImportBookmarksAndFoldersRecursively(
      const base::Value::Dict& folder_node_dict,
      const bookmarks::BookmarkNode* parent) const {
    const base::Value::List* children =
        folder_node_dict.FindList(kImportBookmarksChildrenKey);
    if (!children) {
      return;
    }

    CHECK(parent);
    size_t index = parent->children().size();

    for (const auto& child_value : *children) {
      const base::Value::Dict* child_dict = child_value.GetIfDict();
      if (!child_dict) {
        continue;
      }

      const std::string* type = child_dict->FindString(kImportBookmarksTypeKey);
      const std::string* name = child_dict->FindString(kImportBookmarksNameKey);
      if (!type || !name) {
        continue;
      }

      if (*type == kImportBookmarksUrlType) {
        const std::string* url = child_dict->FindString(kImportBookmarksUrlKey);
        const std::string* icon_url =
            child_dict->FindString(kImportBookmarksIconDataUrlKey);
        if (url && AddUrlToBookmarkModelIfValid(index, *url, *name, icon_url,
                                                parent)) {
          ++index;
        }
      } else if (*type == kImportBookmarksFolderType) {
        const bookmarks::BookmarkNode* new_folder =
            bookmark_model_->AddFolder(parent, index, base::UTF8ToUTF16(*name));
        if (new_folder) {
          ImportBookmarksAndFoldersRecursively(*child_dict, new_folder);
          ++index;
        }
      }
    }
  }

  bool AddUrlToBookmarkModelIfValid(
      int index,
      const std::string& url,
      const std::string& name,
      const std::string* icon_url,
      const bookmarks::BookmarkNode* parent) const {
    const GURL gurl = GURL(url);
    if (gurl.is_valid()) {
      bookmark_model_->AddURL(parent, index, base::UTF8ToUTF16(name), gurl);
      if (icon_url) {
        ParseAndPersistEncodedBookmarkFavicon(gurl, GURL(*icon_url));
      }
      return true;
    }
    return false;
  }

  void ParseAndPersistEncodedBookmarkFavicon(const GURL& page_url,
                                             const GURL& icon_url) const {
    if (!image_decoder_ || !icon_url.is_valid() ||
        !icon_url.SchemeIs(url::kDataScheme)) {
      return;
    }

    std::string mime_type, charset, parsed_image_data;
    if (!net::DataURL::Parse(icon_url, &mime_type, &charset,
                             &parsed_image_data)) {
      return;
    }

    // Image decoding will happen in a sandboxed utility process.
    image_decoder_->DecodeImage(
        parsed_image_data, /*desired_image_frame_size=*/gfx::Size(),
        /*data_decoder=*/nullptr,
        base::BindOnce(&PersistFaviconPostFetch, profile_->GetWeakPtr(),
                       page_url, icon_url));
  }

  // This method is marked static to outlive the destruction of the importer
  // instance.
  static void PersistFaviconPostFetch(base::WeakPtr<Profile> profile,
                                      const GURL& page_url,
                                      const GURL& icon_url,
                                      const gfx::Image& image) {
    if (!profile) {
      return;
    }

    favicon::FaviconService* favicon_service =
        FaviconServiceFactory::GetForProfile(
            profile.get(), ServiceAccessType::IMPLICIT_ACCESS);
    if (!favicon_service) {
      return;
    }
    favicon_service->SetOnDemandFavicons(page_url, icon_url,
                                         favicon_base::IconType::kFavicon,
                                         image, base::DoNothing());
  }

  const raw_ref<Profile> profile_;
  const raw_ref<bookmarks::BookmarkModel> bookmark_model_;
  raw_ptr<image_fetcher::ImageDecoder> image_decoder_ = nullptr;
};

}  // namespace

namespace first_run {

void StartBookmarkImportFromDict(Profile* profile,
                                 base::Value::Dict bookmarks_dict) {
  base::Value::Dict* bookmarks_to_import =
      bookmarks_dict.FindDictByDottedPath(kImportBookmarksBookmarksKey);

  if (!bookmarks_to_import ||
      !bookmarks_to_import->FindList(kImportBookmarksChildrenKey)) {
    RecordImportBookmarksResult(FirstRunImportBookmarksResult::kInvalidDict);
    return;
  }

  bookmarks::BookmarkModel* bookmark_model =
      BookmarkModelFactory::GetForBrowserContext(profile);

  if (!bookmark_model) {
    RecordImportBookmarksResult(FirstRunImportBookmarksResult::kInvalidProfile);
    return;
  }

  auto scoped_profile = std::make_unique<ScopedProfileKeepAlive>(
      profile, ProfileKeepAliveOrigin::kWaitingForBookmarksImportOnFirstRun);

  // Schedule the import to run after the bookmark model is loaded.
  // A ScopedProfileKeepAlive is bound to the callback to ensure the profile is
  // not destroyed before the import is complete. The lambda acts as an adapter
  // to keep the ScopedProfile instance alive without passing it to the
  // importer.
  bookmarks::ScheduleCallbackOnBookmarkModelLoad(
      *bookmark_model,
      base::BindOnce(
          [](std::unique_ptr<ScopedProfileKeepAlive> profile_keep_alive,
             const base::Value::Dict& bookmarks,
             bookmarks::BookmarkModel& bookmark_model_arg) {
            FirstRunBookmarkImporter::Import(*profile_keep_alive->profile(),
                                             bookmarks, bookmark_model_arg);
          },
          std::move(scoped_profile), std::move(*bookmarks_to_import),
          std::ref(*bookmark_model)));
}

}  // namespace first_run
