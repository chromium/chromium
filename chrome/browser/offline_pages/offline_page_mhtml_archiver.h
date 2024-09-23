// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_OFFLINE_PAGES_OFFLINE_PAGE_MHTML_ARCHIVER_H_
#define CHROME_BROWSER_OFFLINE_PAGES_OFFLINE_PAGE_MHTML_ARCHIVER_H_

#include <stdint.h>

#include <map>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "components/offline_pages/core/offline_page_archiver.h"
#include "content/public/browser/mhtml_generation_result.h"

namespace base {
class FilePath;
}  // namespace base

namespace content {
class WebContents;
}  // namespace content

namespace offline_pages {

// Class implementing an offline page archiver using MHTML as an archive format.
//
// To generate an MHTML archiver for a given URL, a WebContents instance should
// have that URL loaded.
//
// Example:
//   void SavePageOffline(content::WebContents* web_contents) {
//     const GURL& url = web_contents->GetLastCommittedURL();
//     std::unique_ptr<OfflinePageMHTMLArchiver> archiver(
//         new OfflinePageMHTMLArchiver(
//             web_contents, archive_dir));
//     // Callback is of type OfflinePageModel::SavePageCallback.
//     model->SavePage(url, std::move(archiver), callback);
//   }
//
// TODO(crbug.com/41392683): turn this into a singleton.
class OfflinePageMHTMLArchiver : public OfflinePageArchiver {
 public:
  OfflinePageMHTMLArchiver();

  OfflinePageMHTMLArchiver(const OfflinePageMHTMLArchiver&) = delete;
  OfflinePageMHTMLArchiver& operator=(const OfflinePageMHTMLArchiver&) = delete;

  ~OfflinePageMHTMLArchiver() override;

  // OfflinePageArchiver implementation:
  void CreateArchive(const base::FilePath& archives_dir,
                     const CreateArchiveParams& create_archive_params,
                     content::WebContents* web_contents,
                     CreateArchiveCallback callback) override;

 protected:
  // Try to generate MHTML.
  // Might be overridden for testing purpose.
  virtual void GenerateMHTML(const base::FilePath& archives_dir,
                             content::WebContents* web_contents,
                             const CreateArchiveParams& create_archive_params);

  // Callback for Generating MHTML.
  void OnGenerateMHTMLDone(const GURL& url,
                           const base::FilePath& file_path,
                           const std::u16string& title,
                           const std::string& name_space,
                           base::Time mhtml_start_time,
                           const content::MHTMLGenerationResult& result);
  void OnComputeDigestDone(const GURL& url,
                           const base::FilePath& file_path,
                           const std::u16string& title,
                           const std::string& name_space,
                           base::Time digest_start_time,
                           int64_t file_size,
                           const std::string& digest);

  // Reports failure to create archive a page to the client that requested it.
  void ReportFailure(ArchiverResult result);

 private:
  void DeleteFileAndReportFailure(const base::FilePath& file_path,
                                  ArchiverResult result);

  CreateArchiveCallback callback_;

  base::WeakPtrFactory<OfflinePageMHTMLArchiver> weak_ptr_factory_{this};
};

}  // namespace offline_pages

#endif  // CHROME_BROWSER_OFFLINE_PAGES_OFFLINE_PAGE_MHTML_ARCHIVER_H_
