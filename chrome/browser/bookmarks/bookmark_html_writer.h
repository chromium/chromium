// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BOOKMARKS_BOOKMARK_HTML_WRITER_H_
#define CHROME_BROWSER_BOOKMARKS_BOOKMARK_HTML_WRITER_H_

class Profile;

namespace base {
class FilePath;
}

// Observer for bookmark html output. Used only in tests.
class BookmarksExportObserver {
 public:
  enum class Result {
    kSuccess,
    kCouldNotCreateFile,
    kCouldNotWriteHeader,
    kCouldNotWriteNodes,
  };
  // Is invoked on the IO thread.
  virtual void OnExportFinished(Result result) = 0;

 protected:
  virtual ~BookmarksExportObserver() {}
};

namespace bookmark_html_writer {

// Writes the bookmarks out in the 'bookmarks.html' format understood by
// Firefox and IE. The results are written asynchronously to the file at |path|.
// Before writing to the file favicons are fetched on the main thread.
// TODO(sky): need a callback on failure.
void WriteBookmarks(Profile* profile,
                    const base::FilePath& path,
                    BookmarksExportObserver* observer);

}  // namespace bookmark_html_writer

#endif  // CHROME_BROWSER_BOOKMARKS_BOOKMARK_HTML_WRITER_H_
