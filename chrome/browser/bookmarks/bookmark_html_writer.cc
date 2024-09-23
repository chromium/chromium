// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/bookmarks/bookmark_html_writer.h"

#include <stddef.h>
#include <stdint.h>

#include <list>
#include <map>
#include <memory>
#include <string>

#include "base/base64.h"
#include "base/containers/span.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/escape.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/supports_user_data.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/bookmarks/browser/bookmark_codec.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/favicon/core/favicon_service.h"
#include "components/favicon_base/favicon_types.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/browser_thread.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/favicon_size.h"

using bookmarks::BookmarkCodec;
using bookmarks::BookmarkNode;
using content::BrowserThread;

namespace {

const char kBookmarkFaviconFetcherKey[] = "bookmark-favicon-fetcher";

// File header.
const char kHeader[] =
    "<!DOCTYPE NETSCAPE-Bookmark-file-1>\r\n"
    "<!-- This is an automatically generated file.\r\n"
    "     It will be read and overwritten.\r\n"
    "     DO NOT EDIT! -->\r\n"
    "<META HTTP-EQUIV=\"Content-Type\""
    " CONTENT=\"text/html; charset=UTF-8\">\r\n"
    "<TITLE>Bookmarks</TITLE>\r\n"
    "<H1>Bookmarks</H1>\r\n"
    "<DL><p>\r\n";

// Newline separator.
const char kNewline[] = "\r\n";

// The following are used for bookmarks.

// Start of a bookmark.
const char kBookmarkStart[] = "<DT><A HREF=\"";
// After kBookmarkStart.
const char kAddDate[] = "\" ADD_DATE=\"";
// After kAddDate.
const char kIcon[] = "\" ICON=\"";
// After kIcon.
const char kBookmarkAttributeEnd[] = "\">";
// End of a bookmark.
const char kBookmarkEnd[] = "</A>";

// The following are used when writing folders.

// Start of a folder.
const char kFolderStart[] = "<DT><H3 ADD_DATE=\"";
// After kFolderStart.
const char kLastModified[] = "\" LAST_MODIFIED=\"";
// After kLastModified when writing the bookmark bar.
const char kBookmarkBar[] = "\" PERSONAL_TOOLBAR_FOLDER=\"true\">";
// After kLastModified when writing a user created folder.
const char kFolderAttributeEnd[] = "\">";
// End of the folder.
const char kFolderEnd[] = "</H3>";
// Start of the children of a folder.
const char kFolderChildren[] = "<DL><p>";
// End of the children for a folder.
const char kFolderChildrenEnd[] = "</DL><p>";

// Number of characters to indent by.
const size_t kIndentSize = 4;

// Fetches favicons for list of bookmarks and then starts Writer which outputs
// bookmarks and favicons to html file.
class BookmarkFaviconFetcher : public base::SupportsUserData::Data {
 public:
  // Map of URL and corresponding favicons.
  typedef std::map<std::string, scoped_refptr<base::RefCountedMemory>>
      URLFaviconMap;

  BookmarkFaviconFetcher(Profile* profile,
                         const base::FilePath& path,
                         BookmarksExportObserver* observer);

  BookmarkFaviconFetcher(const BookmarkFaviconFetcher&) = delete;
  BookmarkFaviconFetcher& operator=(const BookmarkFaviconFetcher&) = delete;

  ~BookmarkFaviconFetcher() override = default;

  // Executes bookmark export process.
  void ExportBookmarks();

 private:
  // Recursively extracts URLs from bookmarks.
  void ExtractUrls(const bookmarks::BookmarkNode* node);

  // Executes Writer task that writes bookmarks data to html file.
  void ExecuteWriter();

  // Starts async fetch for the next bookmark favicon.
  // Takes single url from bookmark_urls_ and removes it from the list.
  // Returns true if there are more favicons to extract.
  bool FetchNextFavicon();

  // Favicon fetch callback. After all favicons are fetched executes
  // html output with |background_io_task_runner_|.
  void OnFaviconDataAvailable(
      const favicon_base::FaviconRawBitmapResult& bitmap_result);

  // The Profile object used for accessing FaviconService, bookmarks model.
  raw_ptr<Profile> profile_;

  // All URLs that are extracted from bookmarks. Used to fetch favicons
  // for each of them. After favicon is fetched top url is removed from list.
  std::list<std::string> bookmark_urls_;

  // Tracks favicon tasks.
  base::CancelableTaskTracker cancelable_task_tracker_;

  // Map that stores favicon per URL.
  std::unique_ptr<URLFaviconMap> favicons_map_;

  // Path where html output is stored.
  base::FilePath path_;

  raw_ptr<BookmarksExportObserver> observer_;
};

// Class responsible for the actual writing. Takes ownership of favicons_map.
class Writer : public base::RefCountedThreadSafe<Writer> {
 public:
  Writer(const bookmarks::BookmarkModel* model,
         const base::FilePath& path,
         BookmarkFaviconFetcher::URLFaviconMap* favicons_map,
         BookmarksExportObserver* observer)
      : path_(path), favicons_map_(favicons_map), observer_(observer) {
    // BookmarkModel isn't thread safe (nor would we want to lock it down
    // for the duration of the write), as such we make a copy of the
    // BookmarkModel using BookmarkCodec then write from that.
    BookmarkCodec codec;
    bookmarks_ =
        codec.Encode(model->bookmark_bar_node(), model->other_node(),
                     model->mobile_node(), /*sync_metadata_str=*/std::string());
  }

  Writer(const Writer&) = delete;
  Writer& operator=(const Writer&) = delete;

  // Writing bookmarks and favicons data to file.
  void DoWrite() {
    if (!OpenFile()) {
      NotifyOnFinish(BookmarksExportObserver::Result::kCouldNotCreateFile);
      return;
    }

    if (!Write(kHeader)) {
      NotifyOnFinish(BookmarksExportObserver::Result::kCouldNotWriteHeader);
      return;
    }

    base::Value::Dict* roots = bookmarks_.FindDict(BookmarkCodec::kRootsKey);
    DCHECK(roots);

    base::Value::Dict* root_folder_value =
        roots->FindDict(BookmarkCodec::kBookmarkBarFolderNameKey);
    base::Value::Dict* other_folder_value =
        roots->FindDict(BookmarkCodec::kOtherBookmarkFolderNameKey);
    base::Value::Dict* mobile_folder_value =
        roots->FindDict(BookmarkCodec::kMobileBookmarkFolderNameKey);
    DCHECK(root_folder_value);
    DCHECK(other_folder_value);
    DCHECK(mobile_folder_value);

    IncrementIndent();

    if (!WriteNode(*root_folder_value, BookmarkNode::BOOKMARK_BAR) ||
        !WriteNode(*other_folder_value, BookmarkNode::OTHER_NODE) ||
        !WriteNode(*mobile_folder_value, BookmarkNode::MOBILE)) {
      NotifyOnFinish(BookmarksExportObserver::Result::kCouldNotWriteNodes);
      return;
    }

    DecrementIndent();

    Write(kFolderChildrenEnd);
    Write(kNewline);
    // File close is forced so that unit test could read it.
    file_.reset();

    NotifyOnFinish(BookmarksExportObserver::Result::kSuccess);
  }

 private:
  friend class base::RefCountedThreadSafe<Writer>;

  // Types of text being written out. The type dictates how the text is
  // escaped.
  enum TextType {
    // The text is the value of an html attribute, eg foo in
    // <a href="foo">.
    ATTRIBUTE_VALUE,

    // Actual content, eg foo in <h1>foo</h2>.
    CONTENT
  };

  ~Writer() {}

  // Opens the file, returning true on success.
  bool OpenFile() {
    int flags = base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE;
    file_ = std::make_unique<base::File>(path_, flags);
    if (!file_->IsValid()) {
      PLOG(ERROR) << "Could not create " << path_;
      return false;
    }
    return true;
  }

  // Increments the indent.
  void IncrementIndent() {
    indent_.resize(indent_.size() + kIndentSize, ' ');
  }

  // Decrements the indent.
  void DecrementIndent() {
    DCHECK(!indent_.empty());
    indent_.resize(indent_.size() - kIndentSize, ' ');
  }

  // Called at the end of the export process.
  void NotifyOnFinish(BookmarksExportObserver::Result result) {
    if (observer_ != nullptr) {
      observer_->OnExportFinished(result);
    }
  }

  // Writes raw text out returning true on success. This does not escape
  // the text in anyway.
  bool Write(const std::string& text) {
    if (!text.length()) {
      return true;
    }
    if (!file_->WriteAtCurrentPosAndCheck(base::as_byte_span(text))) {
      PLOG(ERROR) << "Could not write text to " << path_;
      return false;
    }
    return true;
  }

  // Writes out the text string (as UTF8). The text is escaped based on
  // type.
  bool Write(const std::string& text, TextType type) {
    DCHECK(base::IsStringUTF8(text));
    std::string utf8_string;

    switch (type) {
      case ATTRIBUTE_VALUE:
        // Convert " to &quot;
        utf8_string = text;
        base::ReplaceSubstringsAfterOffset(&utf8_string, 0, "\"", "&quot;");
        break;

      case CONTENT:
        utf8_string = base::EscapeForHTML(text);
        break;

      default:
        NOTREACHED_IN_MIGRATION();
    }

    return Write(utf8_string);
  }

  // Indents the current line.
  bool WriteIndent() {
    return Write(indent_);
  }

  // Converts a time string written to the JSON codec into a time_t string
  // (used by bookmarks.html) and writes it.
  bool WriteTime(const std::string& time_string) {
    int64_t internal_value;
    base::StringToInt64(time_string, &internal_value);
    return Write(base::NumberToString(
        base::Time::FromInternalValue(internal_value).ToTimeT()));
  }

  // Writes the node and all its children, returning true on success.
  bool WriteNode(const base::Value::Dict& value,
                 BookmarkNode::Type folder_type) {
    const std::string* title_ptr = value.FindString(BookmarkCodec::kNameKey);
    const std::string* date_added_string =
        value.FindString(BookmarkCodec::kDateAddedKey);
    const std::string* type_string = value.FindString(BookmarkCodec::kTypeKey);
    if (!title_ptr || !date_added_string || !type_string ||
        (*type_string != BookmarkCodec::kTypeURL &&
         *type_string != BookmarkCodec::kTypeFolder)) {
      NOTREACHED_IN_MIGRATION();
      return false;
    }

    std::string title = *title_ptr;
    if (*type_string == BookmarkCodec::kTypeURL) {
      const std::string* url_string = value.FindString(BookmarkCodec::kURLKey);
      if (!url_string) {
        NOTREACHED_IN_MIGRATION();
        return false;
      }

      std::string favicon_string;
      auto itr = favicons_map_->find(*url_string);
      if (itr != favicons_map_->end()) {
        scoped_refptr<base::RefCountedMemory> data = itr->second;
        std::string favicon_base64_encoded = base::Base64Encode(*data);
        GURL favicon_url("data:image/png;base64," + favicon_base64_encoded);
        favicon_string = favicon_url.spec();
      }

      if (!WriteIndent() || !Write(kBookmarkStart) ||
          !Write(*url_string, ATTRIBUTE_VALUE) || !Write(kAddDate) ||
          !WriteTime(*date_added_string) ||
          (!favicon_string.empty() &&
           (!Write(kIcon) || !Write(favicon_string, ATTRIBUTE_VALUE))) ||
          !Write(kBookmarkAttributeEnd) || !Write(title, CONTENT) ||
          !Write(kBookmarkEnd) || !Write(kNewline)) {
        return false;
      }
      return true;
    }

    // Folder.
    const std::string* last_modified_date =
        value.FindString(BookmarkCodec::kDateModifiedKey);
    const base::Value::List* child_values =
        value.FindList(BookmarkCodec::kChildrenKey);
    if (!last_modified_date || !child_values) {
      NOTREACHED_IN_MIGRATION();
      return false;
    }
    if (folder_type != BookmarkNode::OTHER_NODE &&
        folder_type != BookmarkNode::MOBILE) {
      // The other/mobile folder name are not written out. This gives the effect
      // of making the contents of the 'other folder' be a sibling to the
      // bookmark bar folder.
      if (!WriteIndent() || !Write(kFolderStart) ||
          !WriteTime(*date_added_string) || !Write(kLastModified) ||
          !WriteTime(*last_modified_date)) {
        return false;
      }
      if (folder_type == BookmarkNode::BOOKMARK_BAR) {
        if (!Write(kBookmarkBar))
          return false;
        title = l10n_util::GetStringUTF8(IDS_BOOKMARK_BAR_FOLDER_NAME);
      } else if (!Write(kFolderAttributeEnd)) {
        return false;
      }
      if (!Write(title, CONTENT) ||
          !Write(kFolderEnd) ||
          !Write(kNewline) ||
          !WriteIndent() ||
          !Write(kFolderChildren) ||
          !Write(kNewline)) {
        return false;
      }
      IncrementIndent();
    }

    // Write the children.
    for (const base::Value& child_value : *child_values) {
      if (!child_value.is_dict()) {
        NOTREACHED_IN_MIGRATION();
        return false;
      }
      if (!WriteNode(child_value.GetDict(), BookmarkNode::FOLDER)) {
        return false;
      }
    }
    if (folder_type != BookmarkNode::OTHER_NODE &&
        folder_type != BookmarkNode::MOBILE) {
      // Close out the folder.
      DecrementIndent();
      if (!WriteIndent() ||
          !Write(kFolderChildrenEnd) ||
          !Write(kNewline)) {
        return false;
      }
    }
    return true;
  }

  // The BookmarkModel as a base::Value. This value was generated from the
  // BookmarkCodec.
  base::Value::Dict bookmarks_;

  // Path we're writing to.
  base::FilePath path_;

  // Map that stores favicon per URL.
  std::unique_ptr<BookmarkFaviconFetcher::URLFaviconMap> favicons_map_;

  // Observer to be notified on finish.
  raw_ptr<BookmarksExportObserver> observer_;

  // File we're writing to.
  std::unique_ptr<base::File> file_;

  // How much we indent when writing a bookmark/folder. This is modified
  // via IncrementIndent and DecrementIndent.
  std::string indent_;
};

}  // namespace

BookmarkFaviconFetcher::BookmarkFaviconFetcher(
    Profile* profile,
    const base::FilePath& path,
    BookmarksExportObserver* observer)
    : profile_(profile),
      path_(path),
      observer_(observer) {
  DCHECK(!profile->IsOffTheRecord());
  favicons_map_ = std::make_unique<URLFaviconMap>();
}

void BookmarkFaviconFetcher::ExportBookmarks() {
  ExtractUrls(BookmarkModelFactory::GetForBrowserContext(profile_)
                  ->bookmark_bar_node());
  ExtractUrls(
      BookmarkModelFactory::GetForBrowserContext(profile_)->other_node());
  ExtractUrls(
      BookmarkModelFactory::GetForBrowserContext(profile_)->mobile_node());
  if (!bookmark_urls_.empty())
    FetchNextFavicon();
  else
    ExecuteWriter();
}

void BookmarkFaviconFetcher::ExtractUrls(const BookmarkNode* node) {
  if (node->is_url()) {
    std::string url = node->url().spec();
    if (!url.empty())
      bookmark_urls_.push_back(url);
  } else {
    for (const auto& child : node->children())
      ExtractUrls(child.get());
  }
}

void BookmarkFaviconFetcher::ExecuteWriter() {
  base::ThreadPool::PostTask(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&Writer::DoWrite,
                     base::MakeRefCounted<Writer>(
                         BookmarkModelFactory::GetForBrowserContext(profile_),
                         path_, favicons_map_.release(), observer_)));
  profile_->RemoveUserData(kBookmarkFaviconFetcherKey);
  // |this| is deleted!
}

bool BookmarkFaviconFetcher::FetchNextFavicon() {
  if (bookmark_urls_.empty()) {
    return false;
  }
  do {
    std::string url = bookmark_urls_.front();
    // Filter out urls that we've already got favicon for.
    URLFaviconMap::const_iterator iter = favicons_map_->find(url);
    if (favicons_map_->end() == iter) {
      favicon::FaviconService* favicon_service =
          FaviconServiceFactory::GetForProfile(
              profile_, ServiceAccessType::EXPLICIT_ACCESS);
      favicon_service->GetRawFaviconForPageURL(
          GURL(url), {favicon_base::IconType::kFavicon}, gfx::kFaviconSize,
          /*fallback_to_host=*/false,
          base::BindOnce(&BookmarkFaviconFetcher::OnFaviconDataAvailable,
                         base::Unretained(this)),
          &cancelable_task_tracker_);
      return true;
    } else {
      bookmark_urls_.pop_front();
    }
  } while (!bookmark_urls_.empty());
  return false;
}

void BookmarkFaviconFetcher::OnFaviconDataAvailable(
    const favicon_base::FaviconRawBitmapResult& bitmap_result) {
  GURL url;
  if (!bookmark_urls_.empty()) {
    url = GURL(bookmark_urls_.front());
    bookmark_urls_.pop_front();
  }
  if (bitmap_result.is_valid() && !url.is_empty()) {
    favicons_map_->insert(
        make_pair(url.spec(), bitmap_result.bitmap_data));
  }

  if (FetchNextFavicon()) {
    return;
  }
  ExecuteWriter();
}

namespace bookmark_html_writer {

void WriteBookmarks(Profile* profile,
                    const base::FilePath& path,
                    BookmarksExportObserver* observer) {
  // We allow only one concurrent bookmark export operation per profile.
  if (profile->GetUserData(kBookmarkFaviconFetcherKey))
    return;

  auto fetcher =
      std::make_unique<BookmarkFaviconFetcher>(profile, path, observer);
  auto* fetcher_ptr = fetcher.get();
  profile->SetUserData(kBookmarkFaviconFetcherKey, std::move(fetcher));
  fetcher_ptr->ExportBookmarks();
}

}  // namespace bookmark_html_writer
