// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This provides a wrapper around system calls which may be interrupted by a
// signal and return EINTR. See man 7 signal.
// To prevent long-lasting loops (which would likely be a bug, such as a signal
// that should be masked) to go unnoticed, there is a limit after which the
// caller will nonetheless see an EINTR in Debug builds.
// 这提供了一个系统调用的包装器，它可能被信号中断并返回 EINTR。 参见 man 7 信号。
// 为了防止长期循环（这可能是一个错误，例如应该被屏蔽的信号）被忽视，调用者在调试构建中仍然会看到
// EINTR 有一个限制。
//
// On Windows and Fuchsia, this wrapper macro does nothing because there are no
// signals.
// 在 Windows 和 Fuchsia 上，这个包装宏什么都不做，因为没有信号。
//
// Don't wrap close calls in HANDLE_EINTR. Use IGNORE_EINTR if the return
// value of close is significant. See http://crbug.com/269623.
// 不要在 HANDLE_EINTR 中包含关闭调用。 如果 close 的返回值很大，请使用 IGNORE_EINTR。
// 请参阅 http://crbug.com/269623。

// Linux基础知识：EINTR
// 参考：https://blog.csdn.net/yygydjkthh/article/details/7284302
// 慢系统调用(slow system call)：此术语适用于那些可能永远阻塞的系统调用。永远阻塞的系统调用是指
// 调用有可能永远无法返回，多数网络支持函数都属于这一类。如：若没有客户连接到服务器上，那么服务器的
// accept调用就没有返回的保证。
// EINTR 错误的产生：当阻塞于某个慢系统调用的一个进程捕获某个信号且相应信号处理函数返回时，该系统调
// 用可能返回一个EINTR错误。例如：在socket服务器端，设置了信号捕获机制，有子进程，当在父进程阻塞于
// 慢系统调用时由父进程捕获到了一个有效信号时，内核会致使accept返回一个EINTR错误(被中断的系统调用)。
// 当碰到EINTR错误的时候，可以采取有一些可以重启的系统调用要进行重启，而对于有一些系统调用是不能够重
// 启的。例如：accept、read、write、select、和open之类的函数来说，是可以进行重启的。不过对于套接
// 字编程中的connect函数我们是不能重启的，若connect函数返回一个EINTR错误的时候，我们不能再次调用它，
// 否则将立即返回一个错误。针对connect不能重启的处理方法是，必须调用select来等待连接完成。

#ifndef BASE_POSIX_EINTR_WRAPPER_H_
#define BASE_POSIX_EINTR_WRAPPER_H_

#include "build/build_config.h"

#if defined(OS_POSIX)

#include <errno.h> // Unix/Linux系统调用(systerm call)的全局错误码

// NDEBUG宏，其实表示的是release模式，并阻止 assert()起作用
// 参见：https://blog.csdn.net/heluan123132/article/details/76135401
#if defined(NDEBUG)
// 下面宏的最后一句（eintr_wrapper_result）是GCC 扩展。
// 内部块中的最后一个表达式作为整体的值，一旦它被执行，就像逗号运算符一样。
// 使用案例: base/logging.cc 中
// rv = HANDLE_EINTR(write(STDERR_FILENO,
//                         message + bytes_written,
//                         message_len - bytes_written));
// 可以看出来这里HANDLE_EINTR()宏处理的是write函数表达式及其返回值，如果write()执行失败，
// 则返回-1，并设置Linux的全局errno值，即write失败，x为-1或正确的非-1值：
//（1）如果write()正确返回值为非-1，则：跳出do{}while;，并返回正确的write返回值
//（2）如果write()失败返回为-1，则继续在do()while中循环执行write()函数，并循环检查其返回值，直
//    到write()执行正确，返回非-1值，跳出循环。整个HANDLE_EINTR()宏返回write()的返回值。
#define HANDLE_EINTR(x) ({ \
  decltype(x) eintr_wrapper_result; \
  do { \
    eintr_wrapper_result = (x); \
  } while (eintr_wrapper_result == -1 && errno == EINTR); \
  eintr_wrapper_result; \
})

#else
// debug版本有次数限制
#define HANDLE_EINTR(x) ({ \
  int eintr_wrapper_counter = 0; \
  decltype(x) eintr_wrapper_result; \
  do { \
    eintr_wrapper_result = (x); \
  } while (eintr_wrapper_result == -1 && errno == EINTR && \
           eintr_wrapper_counter++ < 100); \
  eintr_wrapper_result; \
})

#endif  // NDEBUG

// 忽略system call的返回值，重新设置为0后返回.
// 例子是：base/files/dir_reader_linux.h 中的析构函数
#define IGNORE_EINTR(x) ({ \
  decltype(x) eintr_wrapper_result; \
  do { \
    eintr_wrapper_result = (x); \
    if (eintr_wrapper_result == -1 && errno == EINTR) { \
      eintr_wrapper_result = 0; \
    } \
  } while (0); \
  eintr_wrapper_result; \
})

#else  // !OS_POSIX

#define HANDLE_EINTR(x) (x)
#define IGNORE_EINTR(x) (x)

#endif  // !OS_POSIX

#endif  // BASE_POSIX_EINTR_WRAPPER_H_
