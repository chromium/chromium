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
class MessageWindow::WindowClass {
 public:
  WindowClass();
  ~WindowClass();

  ATOM atom() { return atom_; }
  HINSTANCE instance() { return instance_; }

 private:
  ATOM atom_ = 0;
  HINSTANCE instance_ = CURRENT_MODULE();

  DISALLOW_COPY_AND_ASSIGN(WindowClass);
};

static LazyInstance<MessageWindow::WindowClass>::DestructorAtExit
    g_window_class = LAZY_INSTANCE_INITIALIZER;

MessageWindow::WindowClass::WindowClass() {
  WNDCLASSEX window_class;
  window_class.cbSize = sizeof(window_class);
  window_class.style = 0;
  window_class.lpfnWndProc = &WrappedWindowProc<WindowProc>;
  window_class.cbClsExtra = 0;
  window_class.cbWndExtra = 0;
  window_class.hInstance = instance_;
  window_class.hIcon = nullptr;
  window_class.hCursor = nullptr;
  window_class.hbrBackground = nullptr;
  window_class.lpszMenuName = nullptr;
  window_class.lpszClassName = kMessageWindowClassName;
  window_class.hIconSm = nullptr;
  atom_ = RegisterClassEx(&window_class);
  if (atom_ == 0) {
    PLOG(ERROR)
        << "Failed to register the window class for a message-only window";
  }
}

MessageWindow::WindowClass::~WindowClass() {
  if (atom_ != 0) {
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

bool MessageWindow::DoCreate(MessageCallback message_callback,
                             const wchar_t* window_name) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(message_callback_.is_null());
  DCHECK(!window_);

  message_callback_ = std::move(message_callback);

  WindowClass& window_class = g_window_class.Get();
  window_ =
      CreateWindow(MAKEINTATOM(window_class.atom()), window_name, 0, 0, 0, 0, 0,
                   HWND_MESSAGE, nullptr, window_class.instance(), this);
  if (!window_) {
    PLOG(ERROR) << "Failed to create a message-only window";
    return false;
  }

  return true;
}

// static
LRESULT CALLBACK MessageWindow::WindowProc(HWND hwnd,
                                           UINT message,
                                           WPARAM wparam,
                                           LPARAM lparam) {
  MessageWindow* self =
      reinterpret_cast<MessageWindow*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));

  switch (message) {
    // Set up the self before handling WM_CREATE.
    case WM_CREATE: {
      CREATESTRUCT* cs = reinterpret_cast<CREATESTRUCT*>(lparam);
      self = reinterpret_cast<MessageWindow*>(cs->lpCreateParams);

      // Make |hwnd| available to the message handler. At this point the control
      // hasn't returned from CreateWindow() yet.
      self->window_ = hwnd;

      // Store pointer to the self to the window's user data.
      SetLastError(ERROR_SUCCESS);
      LONG_PTR result = SetWindowLongPtr(hwnd, GWLP_USERDATA,
                                         reinterpret_cast<LONG_PTR>(self));
      CHECK(result != 0 || GetLastError() == ERROR_SUCCESS);
      break;
    }

    // Clear the pointer to stop calling the self once WM_DESTROY is
    // received.
    case WM_DESTROY: {
      SetLastError(ERROR_SUCCESS);
      LONG_PTR result = SetWindowLongPtr(hwnd, GWLP_USERDATA, NULL);
      CHECK(result != 0 || GetLastError() == ERROR_SUCCESS);
      break;
    }
  }

  // Handle the message.
  if (self) {
    LRESULT message_result;
    if (self->message_callback_.Run(message, wparam, lparam, &message_result))
      return message_result;
  }

  return DefWindowProc(hwnd, message, wparam, lparam);
}

}  // namespace win
}  // namespace base
