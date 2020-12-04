// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_IMAGE_WRITER_PRIVATE_UNZIP_HELPER_H_
#define CHROME_BROWSER_EXTENSIONS_API_IMAGE_WRITER_PRIVATE_UNZIP_HELPER_H_

#include "base/callback.h"
#include "base/memory/ref_counted_memory.h"
#include "build/build_config.h"

namespace base {
class FilePath;
}

namespace zip {
class ZipReader;
}

namespace extensions {
namespace image_writer {

// A helper to provide Unzip operation.
class UnzipHelper : public base::RefCountedThreadSafe<UnzipHelper> {
 public:
  explicit UnzipHelper(
      const base::Callback<void(const base::FilePath&)>& open_callback,
      const base::Closure& complete_callback,
      const base::Callback<void(const std::string&)>& failure_callback,
      const base::Callback<void(int64_t, int64_t)>& progress_callback);

  void Unzip(const base::FilePath& image_path,
             const base::FilePath& temp_dir_path);

 private:
  friend class base::RefCountedThreadSafe<UnzipHelper>;
  ~UnzipHelper();

  void OnError(const std::string& error);
  void OnOpenSuccess(const base::FilePath& image_path);
  void OnComplete();
  void OnProgress(int64_t total_bytes, int64_t curr_bytes);

  base::Callback<void(const base::FilePath&)> open_callback_;
  base::Closure complete_callback_;
  base::Callback<void(const std::string&)> failure_callback_;
  base::Callback<void(int64_t, int64_t)> progress_callback_;

  // Zip reader for unzip operations. The reason for using a pointer is that we
  // don't want to include zip_reader.h here which can mangle definitions in
  // jni.h when included in the same file. See crbug.com/554199.
  std::unique_ptr<zip::ZipReader> zip_reader_;

  DISALLOW_COPY_AND_ASSIGN(UnzipHelper);
};

}  // namespace image_writer
}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_IMAGE_WRITER_PRIVATE_UNZIP_HELPER_H_
