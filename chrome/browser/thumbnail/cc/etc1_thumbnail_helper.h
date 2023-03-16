// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_THUMBNAIL_CC_ETC1_THUMBNAIL_HELPER_H_
#define CHROME_BROWSER_THUMBNAIL_CC_ETC1_THUMBNAIL_HELPER_H_

#include "base/files/file_path.h"
#include "chrome/browser/thumbnail/cc/thumbnail.h"

namespace thumbnail {

class Etc1ThumbnailHelper {
 public:
  // Implementation of the ETC1 thumbnail helper class which takes a base
  // file path on instantiation for future file path retrieval.
  Etc1ThumbnailHelper(
      const base::FilePath& base_path,
      scoped_refptr<base::SequencedTaskRunner> file_task_runner);
  ~Etc1ThumbnailHelper();

  Etc1ThumbnailHelper(const Etc1ThumbnailHelper&) = delete;
  Etc1ThumbnailHelper& operator=(const Etc1ThumbnailHelper&) = delete;

  // Callback post_compression_task will run on the thread created by this
  // helper.
  void Compress(SkBitmap raw_data,
                gfx::Size encoded_size,
                base::OnceCallback<void(sk_sp<SkPixelRef>, const gfx::Size&)>
                    post_compression_task);
  // Closure post_write_task will run on the thread created by this helper.
  void Write(thumbnail::TabId tab_id,
             sk_sp<SkPixelRef> compressed_data,
             float scale,
             const gfx::Size& content_size,
             base::OnceClosure post_write_task);
  // Callback post_read_task will run on the thread created by this helper.
  void Read(thumbnail::TabId tab_id,
            base::OnceCallback<void(sk_sp<SkPixelRef>, float, const gfx::Size&)>
                post_read_task);
  void Delete(thumbnail::TabId tab_id);
  // Callback post_decompress_callback will run on the thread created by this
  // helper.
  void Decompress(
      base::OnceCallback<void(bool, const SkBitmap&)> post_decompress_callback,
      sk_sp<SkPixelRef> compressed_data,
      float scale,
      const gfx::Size& encoded_size);

 private:
  friend class Etc1ThumbnailHelperTest;

  // Member function to retrieve the ETC1 file path using the stored base
  // path, and is exposed primarily for unit testing purposes.
  base::FilePath GetFilePath(thumbnail::TabId tab_id);

  const base::FilePath base_path_;

  scoped_refptr<base::SequencedTaskRunner> default_task_runner_;
  scoped_refptr<base::SequencedTaskRunner> file_task_runner_;
};

}  // namespace thumbnail

#endif  // CHROME_BROWSER_THUMBNAIL_CC_ETC1_THUMBNAIL_HELPER_H_
