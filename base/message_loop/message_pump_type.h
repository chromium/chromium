// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_MESSAGE_LOOP_MESSAGE_PUMP_TYPE_H_
#define BASE_MESSAGE_LOOP_MESSAGE_PUMP_TYPE_H_

#include "build/build_config.h"

namespace base {

// A MessagePump has a particular type, which indicates the set of
// asynchronous events it may process in addition to tasks and timers.
// MessagePump 有一个特定的类型，它表示除了任务和计时器之外它可以处理的异步事件集。
enum class MessagePumpType {
  // This type of pump only supports tasks and timers.
  // 这种类型的泵只支持任务和计时器。
  DEFAULT,

  // This type of pump also supports native UI events (e.g., Windows messages).
  // 这种类型的泵还支持原生 UI 事件（例如，Windows 消息）。
  UI,

  // User provided implementation of MessagePump interface
  // 用户提供的 MessagePump 接口实现
  CUSTOM,

  // This type of pump also supports asynchronous IO.
  // 这种类型的泵还支持异步 IO。
  IO,

#if defined(OS_ANDROID)
  // This type of pump is backed by a Java message handler which is
  // responsible for running the tasks added to the ML. This is only for use
  // on Android. TYPE_JAVA behaves in essence like TYPE_UI, except during
  // construction where it does not use the main thread specific pump factory.
  // 这种类型的泵由 Java 消息处理程序支持，该处理程序负责运行添加到 ML 的任务。 这仅适用于
  // Android。TYPE_JAVA 的行为本质上类似于 TYPE_UI，除了在不使用主线程特定泵工厂的构造期间。
  JAVA,
#endif  // defined(OS_ANDROID)

#if defined(OS_APPLE)
  // This type of pump is backed by a NSRunLoop. This is only for use on
  // OSX and IOS.
  // 这种类型的泵由 NSRunLoop 支持。 这仅适用于 OSX 和 IOS。
  NS_RUNLOOP,
#endif  // defined(OS_APPLE)

#if defined(OS_WIN)
  // This type of pump supports WM_QUIT messages in addition to other native
  // UI events. This is only for use on windows.
  // 除了其他本机 UI 事件之外，这种类型的泵还支持 WM_QUIT 消息。 这仅适用于 Windows。
  UI_WITH_WM_QUIT_SUPPORT,
#endif  // defined(OS_WIN)
};

}  // namespace base

#endif  // BASE_MESSAGE_LOOP_MESSAGE_PUMP_TYPE_H_
