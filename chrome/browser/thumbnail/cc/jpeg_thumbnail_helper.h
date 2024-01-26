// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_THUMBNAIL_CC_JPEG_THUMBNAIL_HELPER_H_
#define CHROME_BROWSER_THUMBNAIL_CC_JPEG_THUMBNAIL_HELPER_H_

#include <optional>

#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#include "base/task/task_runner.h"
#include "chrome/browser/thumbnail/cc/thumbnail.h"

namespace thumbnail {

class JpegThumbnailHelper {
 public:
  // Implementation of the JPEG thumbnail helper class which takes a base
  // file path on instantiation for future file path retrieval.
  JpegThumbnailHelper(
      const base::FilePath& base_path,
      scoped_refptr<base::SequencedTaskRunner> file_task_runner);
  ~JpegThumbnailHelper();

  JpegThumbnailHelper(const JpegThumbnailHelper&) = delete;
  JpegThumbnailHelper& operator=(const JpegThumbnailHelper&) = delete;

  // `post_processing_task` will run on the thread that created this
  // JpegThumbnailHelper.
  void Compress(
      const SkBitmap& bitmap,
      base::OnceCallback<void(std::vector<uint8_t>)> post_processing_task);
  // `post_write_task` will run on the thread that created this
  // JpegThumbnailHelper.
  void Write(thumbnail::TabId tab_id,
             std::vector<uint8_t> compressed_data,
             base::OnceCallback<void(bool)> post_write_task);

  // `post_read_task` will run on the thread that created this
  // JpegThumbnailHelper.
  void Read(thumbnail::TabId tab_id,
            base::OnceCallback<void(std::optional<std::vector<uint8_t>>)>
                post_read_task);
  void Delete(thumbnail::TabId tab_id);

 private:
  friend class JpegThumbnailHelperTest;

  // Member function to retrieve the JPEG file path using the stored base
  // path, and is exposed primarily for unit testing purposes.
  base::FilePath GetJpegFilePath(thumbnail::TabId tab_id);

  const base::FilePath base_path_;

  scoped_refptr<base::SequencedTaskRunner> default_task_runner_;
  scoped_refptr<base::SequencedTaskRunner> file_task_runner_;
};

}  // namespace thumbnail

#endif  // CHROME_BROWSER_THUMBNAIL_CC_JPEG_THUMBNAIL_HELPER_H_
