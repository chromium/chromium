// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_FILES_DIR_READER_LINUX_H_
#define BASE_FILES_DIR_READER_LINUX_H_

#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <sys/syscall.h>
#include <unistd.h>

#include "base/logging.h"
#include "base/posix/eintr_wrapper.h"

// See the comments in dir_reader_posix.h about this.

namespace base {

/**
 * 模拟Linux的dirent.h文件中的 struct dirent，一模一样
 */
struct linux_dirent {
  uint64_t        d_ino;     // inode number 索引节点号, 目录的inode号
  int64_t         d_off;     // offset to this dirent 在目录文件中的偏移
  unsigned short  d_reclen;  // length of this d_name 目录文件名长
  unsigned char   d_type;    // the type of d_name 文件类型
  char            d_name[0]; // file name (null-terminated) 文件名，最长255字符
};
// 其中，d_type 的值包括：
// enum {
//   DT_UNKNOWN = 0,
// #define DT_UNKNOWN     DT_UNKNOWN  // 未知类型
//   DT_FIFO = 1,
// #define DT_FIFO        DT_FIFO  //  命名管道
//   DT_CHR = 2,
// #define DT_CHR         DT_CHR  // 字符设备文件
//   DT_DIR = 4,
// #define DT_DIR         DT_DIR  // 目录文件
//   DT_BLK = 6,
// #define DT_BLK         DT_BLK   块设备文件
//   DT_REG = 8,
// #define DT_REG         DT_REG  // 普通文件
//   DT_LNK = 10,
// #define DT_LNK         DT_LNK  //连接
//   DT_SOCK = 12,
// #define DT_SOCK        DT_SOCK  // 本地套接口
//   DT_WHT = 14
// #define DT_WHT         DT_WHT  // whiteout
// };

class DirReaderLinux {
 public:
  explicit DirReaderLinux(const char* directory_path)
      // 调用Linux中的fcntl.h中的函数：只读打开目录文件
      : fd_(open(directory_path, O_RDONLY | O_DIRECTORY)),
        offset_(0),
        size_(0) {
    memset(buf_, 0, sizeof(buf_));
  }

  DirReaderLinux(const DirReaderLinux&) = delete;
  DirReaderLinux& operator=(const DirReaderLinux&) = delete;

  ~DirReaderLinux() {
    if (fd_ >= 0) {
      if (IGNORE_EINTR(close(fd_)))
        RAW_LOG(ERROR, "Failed to close directory handle");
    }
  }

  bool IsValid() const {
    return fd_ >= 0;
  }

  // Move to the next entry returning false if the iteration is complete.
  // 如果迭代完成，则移动到下一个返回 false 的条目。
  // 这个 DirReaderLinux 类的设计很优秀，主要体现在，是按照最大512个字节来部分加载目录内容的，
  // 执行过程如下：
  // 1. 初始化时，没有加载目录中的任何数据到内存，仅仅是open()了一个fd
  // 2. 然后通过Next()函数每次加载最多512字节的目录文件中的数据到内存 ，即：buf_中
  // 3. 每次调用Next()都会以 linux_dirent 大小为步进值单位，直到读取完毕。
  // 好处是内存中最多就512字节，每次用完都会重新从目录文件中再次读取最多512字节数据覆盖到buf_。
  // 节省内存，防止内存占用过大。
  bool Next() {
    if (size_) {
      linux_dirent* dirent = reinterpret_cast<linux_dirent*>(&buf_[offset_]);
      offset_ += dirent->d_reclen;
    }

    if (offset_ != size_) // 没用完这512字节 or 已经用完，需要从目录文件中重新读取512字节
      return true;

    // <unistd.h> 中的 syscall()
    const int r = syscall(__NR_getdents64, fd_, buf_, sizeof(buf_));
    if (r == 0)
      return false;
    if (r == -1) {
      DPLOG(FATAL) << "getdents64 failed";
      return false;
    }
    size_ = r;
    offset_ = 0;
    return true;
  }

  const char* name() const {
    if (!size_)
      return nullptr;

    const linux_dirent* dirent =
        reinterpret_cast<const linux_dirent*>(&buf_[offset_]);
    return dirent->d_name;
  }

  int fd() const {
    return fd_;
  }

  static bool IsFallback() {
    return false;
  }

 private:
  const int fd_;

  // alignas 关键字用来设置内存中对齐方式，最小是8字节对齐，可以是16，32，64，128等
  // buf_ 按照 linux_dirent 类型大小对齐
  alignas(linux_dirent) unsigned char buf_[512];
  size_t offset_;
  size_t size_;
};

}  // namespace base

#endif  // BASE_FILES_DIR_READER_LINUX_H_
