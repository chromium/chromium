// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_WIN_MESSAGE_WINDOW_H_
#define BASE_WIN_MESSAGE_WINDOW_H_

#include <string>

#include "base/base_export.h"
#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/threading/thread_checker.h"
#include "base/win/windows_types.h"

// Protect against windows.h being included before this header.
#undef FindWindow

namespace base {
namespace win {

// Implements a message-only window.
// 用以接收Windows窗口消息，并回调消息处理函数，是Windows平台驱动消息泵的核心所在。
// 且是Message-Only的
class BASE_EXPORT MessageWindow {
 public:
  // Used to register a process-wide message window class.
  // 用于注册进程范围的消息窗口类
  class WindowClass;

  // Implement this callback to handle messages received by the message window.
  // If the callback returns |false|, the first four parameters are passed to
  // DefWindowProc(). Otherwise, |*result| is returned by the window procedure.
  // 用于回调到消息泵的回调函数
  using MessageCallback = base::RepeatingCallback<
      bool(UINT message, WPARAM wparam, LPARAM lparam, LRESULT* result)>;

  MessageWindow();

  MessageWindow(const MessageWindow&) = delete;
  MessageWindow& operator=(const MessageWindow&) = delete;

  ~MessageWindow();

  // Creates a message-only window. The incoming messages will be passed by
  // |message_callback|. |message_callback| must outlive |this|.
  // 创建一个仅有消息窗口的隐藏窗口。传入的消息将通过|message_callback| 传递。
  // |message_callback| 必须比 |this| 更长寿。
  bool Create(MessageCallback message_callback);

  // Same as Create() but assigns the name to the created window.
  bool CreateNamed(MessageCallback message_callback,
                   const std::wstring& window_name);

  // 返回Windows整整的窗口window_，这个窗口在创建窗口标识符时被设置为隐藏、且绑定回调函数
  HWND hwnd() const { return window_; }

  // Retrieves a handle of the first message-only window with matching
  // |window_name|.
  // 根据窗口名查找到窗口
  static HWND FindWindow(const std::wstring& window_name);

 private:
  // Give |WindowClass| access to WindowProc().
  friend class WindowClass;

  // Contains the actual window creation code.
  // 真正执行窗口创建的地方：根据窗口标识符创建窗口window_
  bool DoCreate(MessageCallback message_callback, const wchar_t* window_name);

  // Invoked by the OS to process incoming window messages.
  // 由OS调用，用以处理传入的窗口消息，在创建窗口标识符时设置
  static LRESULT CALLBACK WindowProc(HWND hwnd,
                                     UINT message,
                                     WPARAM wparam,
                                     LPARAM lparam);

  // Invoked to handle messages received by the window.
  // 用于向消息泵回调的消息处理回调函数，由消息泵初始化
  MessageCallback message_callback_;

  // Handle of the input window.
  // 真正的Windows平台的输入窗口，接收消息，回调消息处理函数
  HWND window_ = nullptr;

  THREAD_CHECKER(thread_checker_);
};

}  // namespace win
}  // namespace base

#endif  // BASE_WIN_MESSAGE_WINDOW_H_
