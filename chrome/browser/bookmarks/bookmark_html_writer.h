// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BOOKMARKS_BOOKMARK_HTML_WRITER_H_
#define CHROME_BROWSER_BOOKMARKS_BOOKMARK_HTML_WRITER_H_

#include "base/functional/callback.h"

class Profile;

namespace base {
class FilePath;
}

namespace bookmark_html_writer {

// Callback called on completion of the bookmark export.
enum class Result {
  kSuccess,
  kCouldNotCreateFile,
  kCouldNotWriteHeader,
  kCouldNotWriteNodes,
};
using BookmarksExportCallback = base::OnceCallback<void(Result)>;

// Writes the bookmarks out in the 'bookmarks.html' format understood by
// Firefox and IE. The results are written asynchronously to the file at |path|.
// Before writing to the file favicons are fetched on the main thread.
// |callback| is notified on completion, on the IO thread.
void WriteBookmarks(Profile* profile,
                    const base::FilePath& path,
                    BookmarksExportCallback callback);

}  // namespace bookmark_html_writer

#endif  // CHROME_BROWSER_BOOKMARKS_BOOKMARK_HTML_WRITER_H_
