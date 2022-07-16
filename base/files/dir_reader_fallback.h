// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_FILES_DIR_READER_FALLBACK_H_
#define BASE_FILES_DIR_READER_FALLBACK_H_

namespace base {

class DirReaderFallback { // 目录阅读器后备
 public:
  // Open a directory. If |IsValid| is true, then |Next| can be called to start
  // the iteration at the beginning of the directory.
  // 打开一个目录。 如果 |IsValid| 为真，则|Next| 可以调用在目录开头开始迭代。
  explicit DirReaderFallback(const char* directory_path) {
  }

  // After construction, IsValid returns true iff the directory was
  // successfully opened.
  // 构造完成后，如果目录已成功打开，则 IsValid 返回 true。
  bool IsValid() const {
    return false;
  }

  // Move to the next entry returning false if the iteration is complete.
  // 如果迭代完成，则移动到下一个返回 false 的条目。
  bool Next() {
    return false;
  }

  // Return the name of the current directory entry.
  // 返回当前目录条目的名称。
  const char* name() {
    return nullptr;
  }

  // Return the file descriptor which is being used.
  // 返回正在使用的文件描述符。
  int fd() const {
    return -1;
  }

  // Returns true if this is a no-op fallback class (for testing).
  // 如果这是一个无操作回退类（用于测试），则返回 true。
  static bool IsFallback() {
    return true;
  }
};

}  // namespace base

#endif  // BASE_FILES_DIR_READER_FALLBACK_H_
