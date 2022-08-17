// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_FILES_DIR_READER_POSIX_H_
#define BASE_FILES_DIR_READER_POSIX_H_

#include "build/build_config.h"

// This header provides a class, DirReaderPosix, which allows one to open and
// read from directories without allocating memory. For the interface, see
// the generic fallback in dir_reader_fallback.h.
// 这个头文件提供了一个类，DirReaderPosix，它允许在不分配内存的情况下打开和读取目录。
// 对于接口，请参阅 dir_reader_fallback.h 中的通用后备。

// Mac note: OS X has getdirentries, but it only works if we restrict Chrome to
// 32-bit inodes. There is a getdirentries64 syscall in 10.6, but it's not
// wrapped and the direct syscall interface is unstable. Using an unstable API
// seems worse than falling back to enumerating all file descriptors so we will
// probably never implement this on the Mac.
// Mac 注意：OS X 有 getdirentries，但它仅在我们将 Chrome 限制为 32 位 inode 时才有效。
// 10.6 中有一个 getdirentries64 系统调用，但它没有被包装，直接系统调用接口不稳定。 使用不
// 稳定的 API 似乎比回退到枚举所有文件描述符更糟糕，所以我们可能永远不会在 Mac 上实现它。

#if defined(OS_LINUX) || defined(OS_CHROMEOS) || defined(OS_ANDROID)
#include "base/files/dir_reader_linux.h"
#else
#include "base/files/dir_reader_fallback.h"
#endif

namespace base {

// 这是常用的跨平台手法：对外统一类型名称，隐藏平台差异
#if defined(OS_LINUX) || defined(OS_CHROMEOS) || defined(OS_ANDROID)
typedef DirReaderLinux DirReaderPosix;
#else
typedef DirReaderFallback DirReaderPosix;
#endif

}  // namespace base

#endif  // BASE_FILES_DIR_READER_POSIX_H_
