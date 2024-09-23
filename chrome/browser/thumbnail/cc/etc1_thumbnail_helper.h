// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_THUMBNAIL_CC_ETC1_THUMBNAIL_HELPER_H_
#define CHROME_BROWSER_THUMBNAIL_CC_ETC1_THUMBNAIL_HELPER_H_

#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
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

  // `post_compression_task` will run on the thread that created this
  // Etc1ThumbnailHelper.
  // `supports_etc_non_power_of_two` is true if the encoded bitmap bounds don't
  // need to be a multiple of 2.
  void Compress(SkBitmap raw_data,
                bool supports_etc_non_power_of_two,
                base::OnceCallback<void(sk_sp<SkPixelRef>, const gfx::Size&)>
                    post_compression_task);
  // `post_write_task` will run on the thread that created this
  // Etc1ThumbnailHelper.
  void Write(thumbnail::TabId tab_id,
             sk_sp<SkPixelRef> compressed_data,
             float scale,
             const gfx::Size& content_size,
             base::OnceClosure post_write_task);
  // Callers are expected to bind `post_read_task` to the correct thread.
  void Read(thumbnail::TabId tab_id,
            base::OnceCallback<void(sk_sp<SkPixelRef>, float, const gfx::Size&)>
                post_read_task);
  void Delete(thumbnail::TabId tab_id);
  // `post_decompress_callback` will run on the thread that created this
  // Etc1ThumbnailHelper.
  void Decompress(
      base::OnceCallback<void(bool, const SkBitmap&)> post_decompress_callback,
      sk_sp<SkPixelRef> compressed_data,
      float scale,
      const gfx::Size& encoded_size);

  base::WeakPtr<Etc1ThumbnailHelper> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  friend class Etc1ThumbnailHelperTest;

  // Member function to retrieve the ETC1 file path using the stored base
  // path, and is exposed primarily for unit testing purposes.
  base::FilePath GetFilePath(thumbnail::TabId tab_id);

  const base::FilePath base_path_;

  scoped_refptr<base::SequencedTaskRunner> default_task_runner_;
  scoped_refptr<base::SequencedTaskRunner> file_task_runner_;
  base::WeakPtrFactory<Etc1ThumbnailHelper> weak_ptr_factory_{this};
};

}  // namespace thumbnail

#endif  // CHROME_BROWSER_THUMBNAIL_CC_ETC1_THUMBNAIL_HELPER_H_
