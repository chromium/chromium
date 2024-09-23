// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_IMAGE_SOURCE_IMAGE_SOURCE_H_
#define CHROME_BROWSER_ASH_IMAGE_SOURCE_IMAGE_SOURCE_H_

#include <string>

#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "content/public/browser/url_data_source.h"

namespace base {
class SequencedTaskRunner;
}

namespace ash {

// Data source that reads and decodes an image from the RO file system.
class ImageSource : public content::URLDataSource {
 public:
  ImageSource();

  ImageSource(const ImageSource&) = delete;
  ImageSource& operator=(const ImageSource&) = delete;

  ~ImageSource() override;

  // content::URLDataSource implementation.
  std::string GetSource() override;
  void StartDataRequest(
      const GURL& url,
      const content::WebContents::Getter& wc_getter,
      content::URLDataSource::GotDataCallback got_data_callback) override;

  std::string GetMimeType(const GURL& url) override;

 private:
  // Continuation from StartDataRequest().
  void StartDataRequestAfterPathExists(
      const base::FilePath& image_path,
      content::URLDataSource::GotDataCallback got_data_callback,
      bool path_exists);

  // Checks whether we have allowed the image to be loaded.
  bool IsAllowlisted(const std::string& path) const;

  // The background task runner on which file I/O and image decoding are done.
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  base::WeakPtrFactory<ImageSource> weak_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_IMAGE_SOURCE_IMAGE_SOURCE_H_
