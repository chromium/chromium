// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_MAC_CALL_WITH_EH_FRAME_H_
#define BASE_MAC_CALL_WITH_EH_FRAME_H_

#include "base/base_export.h"

namespace base {
namespace mac {

// Invokes the specified block in a stack frame with a special exception
// handler. This function creates an exception handling stack frame that
// specifies a custom C++ exception personality routine, which terminates the
// search for an exception handler at this frame.
// 使用特殊异常处理程序调用堆栈帧中的指定块。此函数创建一个异常处理堆栈帧，该堆栈
// 帧指定一个自定义 C++ 异常个性例程，该例程在此帧终止对异常处理程序的搜索。
//
// The purpose of this function is to prevent a try/catch statement in system
// libraries, acting as a global exception handler, from handling exceptions
// in such a way that disrupts the generation of useful stack traces.
// 此函数的目的是防止系统库中的 try/catch 语句作为全局异常处理程序以破坏有用堆栈
// 跟踪生成的方式处理异常。
void BASE_EXPORT CallWithEHFrame(void (^block)(void));

}  // namespace mac
}  // namespace base

#endif  // BASE_MAC_CALL_WITH_EH_FRAME_H_
