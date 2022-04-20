// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/win/message_window.h"

#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/strings/string_util.h"
#include "base/win/current_module.h"
#include "base/win/wrapped_window_proc.h"

#include <windows.h>

// To avoid conflicts with the macro from the Windows SDK...
#undef FindWindow

const wchar_t kMessageWindowClassName[] = L"Chrome_MessageWindow";

namespace base {
namespace win {

// Used along with LazyInstance to register a window class for message-only
// windows created by MessageWindow.
// 与 LazyInstance 一起用于为 MessageWindow 创建的仅消息窗口注册一个窗口类。
class MessageWindow::WindowClass {
 public:
  WindowClass();

  WindowClass(const WindowClass&) = delete;
  WindowClass& operator=(const WindowClass&) = delete;

  ~WindowClass();

  // 返回Windows窗口标识符
  ATOM atom() { return atom_; } // ATOM <-> WORD
  HINSTANCE instance() { return instance_; }

 private:
  ATOM atom_ = 0; // window_class(WNDCLASSEX类型)这个隐式窗口的标识符
  HINSTANCE instance_ = CURRENT_MODULE();
};

// 进程退出析构时，
static LazyInstance<MessageWindow::WindowClass>::DestructorAtExit
    g_window_class = LAZY_INSTANCE_INITIALIZER;

/**
 * @brief 为了可以创建自己的窗口，就需要向 Windows 注册窗口类型，以便后面
 * 创建窗口时调用。当然，如果使用 Windows 预先注册的窗口是不需要注册的。
 * 通过参数可知，这是一个隐藏窗口，用途是只接收消息，通知 lpfnWndProc函数
 * 指针指向的 WindowProc()函数去处理消息。
 */
MessageWindow::WindowClass::WindowClass() {
  WNDCLASSEX window_class; // 窗口类(信息)
  // 是本结构的字节大小，一般设置为 sizeof(WNDCLASSEX)
  window_class.cbSize = sizeof(window_class);
  // style 是窗口类型
  window_class.style = 0;
  // 核心：是窗口处理消息的回调函数
  window_class.lpfnWndProc = &WrappedWindowProc<WindowProc>;
  // 是窗口类型的扩展
  window_class.cbClsExtra = 0;
  // 是窗口实例的扩展
  window_class.cbWndExtra = 0;
  // 是窗口实例句柄
  window_class.hInstance = instance_;
  // 是窗口图标
  window_class.hIcon = nullptr;
  // 是窗口的光标
  window_class.hCursor = nullptr;
  // 是窗口背景颜色
  window_class.hbrBackground = nullptr;
  // 是窗口菜单名称
  window_class.lpszMenuName = nullptr;
  // 是窗口类型的名称
  window_class.lpszClassName = kMessageWindowClassName;
  // 是窗口小图标
  window_class.hIconSm = nullptr;
  // 注册这个窗口类型，如果注册成功，返回这个窗口类型的标识符(atom_)，可以用标
  // 识号进行创建窗口，查找窗口和注销窗口类型等等。如果失败返回的值是空，因此可
  // 以通过检查返回值为判断是否调用成功。
  atom_ = RegisterClassEx(&window_class);
  if (atom_ == 0) {
    PLOG(ERROR)
        << "Failed to register the window class for a message-only window";
  }
}

MessageWindow::WindowClass::~WindowClass() {
  if (atom_ != 0) {
    // 通过窗口标识符atom_来反注册窗口
    BOOL result = UnregisterClass(MAKEINTATOM(atom_), instance_);
    // Hitting this DCHECK usually means that some MessageWindow objects were
    // leaked. For example not calling
    // ui::Clipboard::DestroyClipboardForCurrentThread() results in a leaked
    // MessageWindow.
    DCHECK(result);
  }
}

MessageWindow::MessageWindow() = default;

MessageWindow::~MessageWindow() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (window_ != nullptr) {
    // 在析构函数中销毁窗口
    BOOL result = DestroyWindow(window_);
    DCHECK(result);
  }
}

bool MessageWindow::Create(MessageCallback message_callback) {
  return DoCreate(std::move(message_callback), nullptr);
}

bool MessageWindow::CreateNamed(MessageCallback message_callback,
                                const std::wstring& window_name) {
  return DoCreate(std::move(message_callback), window_name.c_str());
}

// static
HWND MessageWindow::FindWindow(const std::wstring& window_name) {
  return FindWindowEx(HWND_MESSAGE, nullptr, kMessageWindowClassName,
                      window_name.c_str());
}

/**
 * @brief 根据窗口标识符、窗口名等参数创建窗口window_(HWND类型)
 */
bool MessageWindow::DoCreate(MessageCallback message_callback,
                             const wchar_t* window_name) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(message_callback_.is_null());
  DCHECK(!window_);

  // 保存消息泵传递下来的窗口消息回调函数
  message_callback_ = std::move(message_callback);

  WindowClass& window_class = g_window_class.Get();
  // 根据窗口标识符(atom_)创建窗口(window_)，注意这是一个隐式窗口，在创建atom_时设置
  window_ = CreateWindow(MAKEINTATOM(window_class.atom()), // 窗口标识符
                                     window_name, // 窗口名
                                     0, 0, 0, 0, 0,
                                     HWND_MESSAGE, // 仅接收消息的窗口类型，即：Message-Only窗口
                                     nullptr,
                                     window_class.instance(),
                                     this);
  if (!window_) {
    PLOG(ERROR) << "Failed to create a message-only window";
    return false;
  }

  return true;
}

/**
 * @brief 在创建窗口标识符时，设置的窗口消息处理函数
 */
LRESULT CALLBACK MessageWindow::WindowProc(HWND hwnd,
                                           UINT message,
                                           WPARAM wparam,
                                           LPARAM lparam) {
  MessageWindow* self = // 根据窗口参数(hwnd, 在此时window_)，获取消息窗口
      reinterpret_cast<MessageWindow*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));

  switch (message) {
    // Set up the self before handling WM_CREATE.
    case WM_CREATE: { // 窗口创建事件
      CREATESTRUCT* cs = reinterpret_cast<CREATESTRUCT*>(lparam);
      self = reinterpret_cast<MessageWindow*>(cs->lpCreateParams);

      // Make |hwnd| available to the message handler. At this point the control
      // hasn't returned from CreateWindow() yet.
      self->window_ = hwnd; // 设置窗口

      // Store pointer to the self to the window's user data.
      SetLastError(ERROR_SUCCESS);
      LONG_PTR result = SetWindowLongPtr(hwnd, GWLP_USERDATA,
                                         reinterpret_cast<LONG_PTR>(self));
      CHECK(result != 0 || GetLastError() == ERROR_SUCCESS);
      break;
    }

    // Clear the pointer to stop calling the self once WM_DESTROY is
    // received.
    case WM_DESTROY: { // 窗口销毁事件
      SetLastError(ERROR_SUCCESS);
      LONG_PTR result = SetWindowLongPtr(hwnd, GWLP_USERDATA, NULL);
      CHECK(result != 0 || GetLastError() == ERROR_SUCCESS);
      break;
    }
  }

  // Handle the message. 回调到上层使用方来处理消息
  if (self) {
    LRESULT message_result;
    if (self->message_callback_.Run(message, wparam, lparam, &message_result))
      return message_result;
  }

  return DefWindowProc(hwnd, message, wparam, lparam);
}

}  // namespace win
}  // namespace base
