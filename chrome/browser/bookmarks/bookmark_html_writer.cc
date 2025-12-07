// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#include "chrome/browser/bookmarks/bookmark_html_writer.h"

#include <stddef.h>
#include <stdint.h>

#include <list>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include "base/base64.h"
#include "base/check.h"
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
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/favicon/core/favicon_service.h"
#include "components/favicon_base/favicon_types.h"
#include "content/public/browser/browser_thread.h"
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

  BookmarkFaviconFetcher(
      Profile* profile,
      const base::FilePath& path,
      bookmark_html_writer::BookmarksExportCallback callback);

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
  [[nodiscard]] bool FetchNextFavicon();

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
  URLFaviconMap favicons_map_;

  // Path where html output is stored.
  base::FilePath path_;

  bookmark_html_writer::BookmarksExportCallback callback_;
};

// Class responsible for the actual writing.
class Writer : public base::RefCountedThreadSafe<Writer> {
 public:
  Writer(const bookmarks::BookmarkModel* model,
         const base::FilePath& path,
         BookmarkFaviconFetcher::URLFaviconMap favicons_map)
      : path_(path), favicons_map_(std::move(favicons_map)) {
    // BookmarkModel isn't thread safe (nor would we want to lock it down
    // for the duration of the write), as such we make a copy of the
    // BookmarkModel using BookmarkCodec then write from that.
    BookmarkCodec codec;
    local_bookmarks_ =
        codec.Encode(model->bookmark_bar_node(), model->other_node(),
                     model->mobile_node(), /*sync_metadata_str=*/std::string());

    if (model->account_bookmark_bar_node()) {
      CHECK(model->account_other_node());
      CHECK(model->account_mobile_node());
      account_bookmarks_ = codec.Encode(
          model->account_bookmark_bar_node(), model->account_other_node(),
          model->account_mobile_node(), /*sync_metadata_str=*/std::string());
    } else {
      CHECK(!model->account_other_node());
      CHECK(!model->account_mobile_node());
    }
  }

  Writer(const Writer&) = delete;
  Writer& operator=(const Writer&) = delete;

  // Writing bookmarks and favicons data to file.
  bookmark_html_writer::Result DoWrite() {
    if (!OpenFile()) {
      return bookmark_html_writer::Result::kCouldNotCreateFile;
    }

    if (!Write(kHeader)) {
      return bookmark_html_writer::Result::kCouldNotWriteHeader;
    }

    base::Value::Dict* local_permanent_folders =
        local_bookmarks_.FindDict(BookmarkCodec::kRootsKey);
    CHECK(local_permanent_folders);

    base::Value::Dict* bookmark_bar_folder_value =
        local_permanent_folders->FindDict(
            BookmarkCodec::kBookmarkBarFolderNameKey);
    CHECK(bookmark_bar_folder_value);
    base::Value::Dict* other_folder_value = local_permanent_folders->FindDict(
        BookmarkCodec::kOtherBookmarkFolderNameKey);
    CHECK(other_folder_value);
    base::Value::Dict* mobile_folder_value = local_permanent_folders->FindDict(
        BookmarkCodec::kMobileBookmarkFolderNameKey);
    CHECK(mobile_folder_value);

    base::Value::Dict* account_permanent_folders =
        account_bookmarks_.FindDict(BookmarkCodec::kRootsKey);
    base::Value::Dict* account_bookmark_bar_folder_value = nullptr;
    base::Value::Dict* account_other_folder_value = nullptr;
    base::Value::Dict* account_mobile_folder_value = nullptr;
    if (account_permanent_folders) {
      account_bookmark_bar_folder_value = account_permanent_folders->FindDict(
          BookmarkCodec::kBookmarkBarFolderNameKey);
      account_other_folder_value = account_permanent_folders->FindDict(
          BookmarkCodec::kOtherBookmarkFolderNameKey);
      account_mobile_folder_value = account_permanent_folders->FindDict(
          BookmarkCodec::kMobileBookmarkFolderNameKey);
      CHECK(account_bookmark_bar_folder_value);
      CHECK(account_other_folder_value);
      CHECK(account_mobile_folder_value);
    }

    IncrementIndent();

    // Bookmarks are written with the following hierarchy - note the descendents
    // of the other and mobile folders are shifted up one level (compared to the
    // descendents of the bookmark bar). This is for compatibility with the
    // pre-existing file format user by other browsers.
    //
    // - Bookmarks bar (with PERSONAL_TOOLBAR_FOLDER="true" attribute)
    //   - All descendants of the local bookmark bar
    //   - All descendants of the account bookmark bar
    // - All descendants of the local other bookmarks folder
    // - All descendants of the account other bookmarks folder
    // - All descendants of the local mobile bookmarks folder
    // - All descendants of the account mobile bookmarks folder

    // Add the bookmark bar folder, and local descendants.
    if (!WriteFolderStart(*bookmark_bar_folder_value,
                          GetLatestTime({bookmark_bar_folder_value,
                                         account_bookmark_bar_folder_value},
                                        BookmarkCodec::kDateAddedKey),
                          GetLatestTime({bookmark_bar_folder_value,
                                         account_bookmark_bar_folder_value},
                                        BookmarkCodec::kDateModifiedKey),
                          BookmarkNode::BOOKMARK_BAR) ||
        !WriteDescendants(*bookmark_bar_folder_value)) {
      return bookmark_html_writer::Result::kCouldNotWriteNodes;
    }

    // Add account bookmark bar descendants if they exist.
    if (account_bookmark_bar_folder_value &&
        !WriteDescendants(*account_bookmark_bar_folder_value)) {
      return bookmark_html_writer::Result::kCouldNotWriteNodes;
    }

    // Close the bookmark bar folder.
    if (!WriteFolderEnd()) {
      return bookmark_html_writer::Result::kCouldNotWriteNodes;
    }

    // Add the other bookmarks descendants: local, then account if they exist.
    if (!WriteDescendants(*other_folder_value)) {
      return bookmark_html_writer::Result::kCouldNotWriteNodes;
    }
    if (account_other_folder_value &&
        !WriteDescendants(*account_other_folder_value)) {
      return bookmark_html_writer::Result::kCouldNotWriteNodes;
    }

    // Add the mobile bookmarks descendants: local, then account if they exist.
    if (!WriteDescendants(*mobile_folder_value)) {
      return bookmark_html_writer::Result::kCouldNotWriteNodes;
    }
    if (account_mobile_folder_value &&
        !WriteDescendants(*account_mobile_folder_value)) {
      return bookmark_html_writer::Result::kCouldNotWriteNodes;
    }

    DecrementIndent();

    if (!Write(kFolderChildrenEnd) || !Write(kNewline)) {
      return bookmark_html_writer::Result::kCouldNotWriteNodes;
    }

    // File close is forced so that unit test could read it.
    file_.reset();

    return bookmark_html_writer::Result::kSuccess;
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

  ~Writer() = default;

  // Get the latest time of a given type, across a list of folders.
  std::string GetLatestTime(const std::vector<base::Value::Dict*>& folders,
                            std::string_view time_type_key) {
    CHECK(std::ranges::any_of(
        folders, [](const base::Value::Dict* folder) { return folder; }));

    int64_t latest_time = 0;
    for (base::Value::Dict* folder : folders) {
      if (!folder) {
        continue;
      }
      std::string* string_ptr = folder->FindString(time_type_key);
      CHECK(string_ptr);
      int64_t time;
      CHECK(base::StringToInt64(*string_ptr, &time));
      latest_time = std::max(latest_time, time);
    }
    return base::NumberToString(latest_time);
  }

  // Opens the file, returning true on success.
  [[nodiscard]] bool OpenFile() {
    int flags = base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE;
    file_ = std::make_unique<base::File>(path_, flags);
    if (!file_->IsValid()) {
      PLOG(ERROR) << "Could not create " << path_;
      return false;
    }
    return true;
  }

  // Increments the indent.
  void IncrementIndent() { indent_.resize(indent_.size() + kIndentSize, ' '); }

  // Decrements the indent.
  void DecrementIndent() {
    CHECK(!indent_.empty());
    indent_.resize(indent_.size() - kIndentSize, ' ');
  }

  // Writes raw text out returning true on success. This does not escape
  // the text in anyway.
  [[nodiscard]] bool Write(const std::string& text) {
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
  [[nodiscard]] bool Write(const std::string& text, TextType type) {
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
        NOTREACHED();
    }

    return Write(utf8_string);
  }

  // Indents the current line.
  [[nodiscard]] bool WriteIndent() { return Write(indent_); }

  // Converts a time string written to the JSON codec into a time_t string
  // (used by bookmarks.html) and writes it.
  [[nodiscard]] bool WriteTime(const std::string& time_string) {
    int64_t internal_value;
    base::StringToInt64(time_string, &internal_value);
    return Write(base::NumberToString(
        base::Time::FromInternalValue(internal_value).ToTimeT()));
  }

  // Writes the start of a folder section, ready for subsequent calls to write
  // out children of the folder. `value` is the folder to be written, which must
  // be of `folder_type` either `BOOKMARK_BAR` or `FOLDER`.
  [[nodiscard]] bool WriteFolderStart(const base::Value::Dict& value,
                                      const std::string& date_added,
                                      const std::string& date_modified,
                                      BookmarkNode::Type folder_type) {
    const std::string* title = value.FindString(BookmarkCodec::kNameKey);
    CHECK(title);

    if (!WriteIndent() || !Write(kFolderStart) || !WriteTime(date_added) ||
        !Write(kLastModified) || !WriteTime(date_modified)) {
      return false;
    }

    switch (folder_type) {
      case BookmarkNode::BOOKMARK_BAR:
        if (!Write(kBookmarkBar)) {
          return false;
        }
        break;
      case BookmarkNode::FOLDER:
        if (!Write(kFolderAttributeEnd)) {
          return false;
        }
        break;
      case BookmarkNode::URL:
      case BookmarkNode::OTHER_NODE:
      case BookmarkNode::MOBILE:
        NOTREACHED();
    }

    if (!Write(*title, CONTENT) || !Write(kFolderEnd) || !Write(kNewline) ||
        !WriteIndent() || !Write(kFolderChildren) || !Write(kNewline)) {
      return false;
    }
    IncrementIndent();
    return true;
  }

  // Writes the child nodes of folder `folder` (this does not include writing
  // the folder itself).
  [[nodiscard]] bool WriteDescendants(const base::Value::Dict& folder) {
    const base::Value::List* child_values =
        folder.FindList(BookmarkCodec::kChildrenKey);
    CHECK(child_values);

    for (const base::Value& child_value : *child_values) {
      CHECK(child_value.is_dict());
      if (!WriteNodeAndDescendants(child_value.GetDict())) {
        return false;
      }
    }

    return true;
  }

  // Writes the end of a folder section that was previously created with
  // `WriteFolderStart()`.
  [[nodiscard]] bool WriteFolderEnd() {
    DecrementIndent();
    return WriteIndent() && Write(kFolderChildrenEnd) && Write(kNewline);
  }

  // Writes the node and all its children, returning true on success.
  [[nodiscard]] bool WriteNodeAndDescendants(const base::Value::Dict& value) {
    const std::string* title_ptr = value.FindString(BookmarkCodec::kNameKey);
    CHECK(title_ptr);
    const std::string* date_added_string =
        value.FindString(BookmarkCodec::kDateAddedKey);
    CHECK(date_added_string);
    const std::string* date_modified_string =
        value.FindString(BookmarkCodec::kDateModifiedKey);
    const std::string* type_string = value.FindString(BookmarkCodec::kTypeKey);
    CHECK(type_string);
    CHECK(*type_string == BookmarkCodec::kTypeURL ||
          *type_string == BookmarkCodec::kTypeFolder);

    std::string title = *title_ptr;
    if (*type_string == BookmarkCodec::kTypeURL) {
      const std::string* url_string = value.FindString(BookmarkCodec::kURLKey);
      CHECK(url_string);

      std::string favicon_string;
      auto itr = favicons_map_.find(*url_string);
      if (itr != favicons_map_.end()) {
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
    CHECK(date_modified_string);
    if (!WriteFolderStart(value, *date_added_string, *date_modified_string,
                          BookmarkNode::FOLDER)) {
      return false;
    }

    if (!WriteDescendants(value)) {
      return false;
    }

    if (!WriteFolderEnd()) {
      return false;
    }

    return true;
  }

  // The BookmarkModel as a base::Value, split into local and account bookmarks.
  // These values were generated from the BookmarkCodec.
  base::Value::Dict local_bookmarks_;
  base::Value::Dict account_bookmarks_;

  // Path we're writing to.
  base::FilePath path_;

  // Map that stores favicon per URL.
  BookmarkFaviconFetcher::URLFaviconMap favicons_map_;

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
    bookmark_html_writer::BookmarksExportCallback callback)
    : profile_(profile), path_(path), callback_(std::move(callback)) {
  DCHECK(!profile->IsOffTheRecord());
}

void BookmarkFaviconFetcher::ExportBookmarks() {
  bookmarks::BookmarkModel* model =
      BookmarkModelFactory::GetForBrowserContext(profile_);
  ExtractUrls(model->bookmark_bar_node());
  ExtractUrls(model->other_node());
  ExtractUrls(model->mobile_node());

  if (model->account_bookmark_bar_node()) {
    CHECK(model->account_other_node());
    CHECK(model->account_mobile_node());
    ExtractUrls(model->account_bookmark_bar_node());
    ExtractUrls(model->account_other_node());
    ExtractUrls(model->account_mobile_node());
  } else {
    CHECK(!model->account_other_node());
    CHECK(!model->account_mobile_node());
  }

  if (!bookmark_urls_.empty()) {
    // There are bookmarks for which to fetch favicons, and the favicon map is
    // empty (since it was just created). There is therefore async work to do.
    CHECK(favicons_map_.empty());
    CHECK(FetchNextFavicon());
  } else {
    ExecuteWriter();
  }
}

void BookmarkFaviconFetcher::ExtractUrls(const BookmarkNode* node) {
  CHECK(node);
  if (node->is_url()) {
    std::string url = node->url().spec();
    if (!url.empty()) {
      bookmark_urls_.push_back(url);
    }
  } else {
    for (const auto& child : node->children()) {
      ExtractUrls(child.get());
    }
  }
}

void BookmarkFaviconFetcher::ExecuteWriter() {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&Writer::DoWrite,
                     base::MakeRefCounted<Writer>(
                         BookmarkModelFactory::GetForBrowserContext(profile_),
                         path_, std::move(favicons_map_))),
      std::move(callback_));
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
    URLFaviconMap::const_iterator iter = favicons_map_.find(url);
    if (favicons_map_.end() == iter) {
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
    favicons_map_.insert(make_pair(url.spec(), bitmap_result.bitmap_data));
  }

  if (FetchNextFavicon()) {
    return;
  }
  ExecuteWriter();
}

namespace bookmark_html_writer {

void WriteBookmarks(Profile* profile,
                    const base::FilePath& path,
                    BookmarksExportCallback callback) {
  // We allow only one concurrent bookmark export operation per profile.
  if (profile->GetUserData(kBookmarkFaviconFetcherKey)) {
    return;
  }

  auto fetcher = std::make_unique<BookmarkFaviconFetcher>(profile, path,
                                                          std::move(callback));
  auto* fetcher_ptr = fetcher.get();
  profile->SetUserData(kBookmarkFaviconFetcherKey, std::move(fetcher));
  fetcher_ptr->ExportBookmarks();
}

}  // namespace bookmark_html_writer
