// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/win/message_window.h"

#include <windows.h>

#include <map>
#include <utility>

#include "base/check.h"
#include "base/check_op.h"
#include "base/compiler_specific.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ref.h"
#include "base/no_destructor.h"
#include "base/process/memory.h"
#include "base/strings/string_util.h"
#include "base/thread_annotations.h"
#include "base/threading/thread_checker.h"
#include "base/threading/thread_local.h"
#include "base/win/current_module.h"
#include "base/win/resource_exhaustion.h"
#include "base/win/wrapped_window_proc.h"

// To avoid conflicts with the macro from the Windows SDK...
#undef FindWindow

const wchar_t kMessageWindowClassName[] = L"Chrome_MessageWindow";

namespace {

// This class can be accessed from multiple threads,
// this is handled by each thread having a different instance.
class MessageWindowMap {
 public:
  static MessageWindowMap& GetInstanceForCurrentThread() {
    static base::NoDestructor<base::ThreadLocalOwnedPointer<MessageWindowMap>>
        instance;
    if (!instance->Get()) {
      instance->Set(base::WrapUnique(new MessageWindowMap));
    }
    return *(instance->Get());
  }

  MessageWindowMap(const MessageWindowMap&) = delete;
  MessageWindowMap& operator=(const MessageWindowMap&) = delete;

  // Each key should only be inserted once.
  void Insert(HWND hwnd, base::win::MessageWindow& message_window) {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    CHECK(map_.emplace(hwnd, message_window).second);
  }

  // Erase should only be called on an existing key.
  void Erase(HWND hwnd) {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    // Check that exactly one element is erased from the map.
    CHECK_EQ(map_.erase(hwnd), 1u);
  }

  // Will return nullptr if `hwnd` is not in the map.
  base::win::MessageWindow* Get(HWND hwnd) const {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    if (auto search = map_.find(hwnd); search != map_.end()) {
      return &(search->second.get());
    }
    return nullptr;
  }

 private:
  MessageWindowMap() = default;
  THREAD_CHECKER(thread_checker_);
  std::map<HWND, const raw_ref<base::win::MessageWindow>> map_
      GUARDED_BY_CONTEXT(thread_checker_);
};

}  // namespace

namespace base {
namespace win {

// Used along with LazyInstance to register a window class for message-only
// windows created by MessageWindow.
class MessageWindow::WindowClass {
 public:
  WindowClass();

  WindowClass(const WindowClass&) = delete;
  WindowClass& operator=(const WindowClass&) = delete;

  ~WindowClass();

  ATOM atom() { return atom_; }
  HINSTANCE instance() { return instance_; }

 private:
  ATOM atom_ = 0;
  HINSTANCE instance_ = CURRENT_MODULE();
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
    OnResourceExhausted();
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
    if (::GetLastError() == ERROR_NOT_ENOUGH_MEMORY) {
      base::TerminateBecauseOutOfMemory(0);
    }
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
  // This can be called from different threads for different windows,
  // each thread has its own MessageWindowMap instance.
  auto& message_window_map = MessageWindowMap::GetInstanceForCurrentThread();
  MessageWindow* self = message_window_map.Get(hwnd);

  // CreateWindow will send a WM_CREATE message during window creation.
  if (!self && message == WM_CREATE) [[unlikely]] {
    CREATESTRUCT* const cs = reinterpret_cast<CREATESTRUCT*>(lparam);
    self = reinterpret_cast<MessageWindow*>(cs->lpCreateParams);

    // Tell the MessageWindow instance the HWND that CreateWindow has produced.
    self->window_ = hwnd;

    // Associate the MessageWindow instance with the HWND in the map.
    message_window_map.Insert(hwnd, *self);
  }

  if (!self) [[unlikely]] {
    return DefWindowProc(hwnd, message, wparam, lparam);
  }

  LRESULT message_result = {};
  if (!self->message_callback_.Run(message, wparam, lparam, &message_result)) {
    message_result = DefWindowProc(hwnd, message, wparam, lparam);
  }

  if (message == WM_DESTROY) [[unlikely]] {
    // Tell the MessageWindow instance that it no longer has an HWND.
    self->window_ = nullptr;

    // Remove this HWND's MessageWindow from the map since it is going away.
    message_window_map.Erase(hwnd);
  }

  return message_result;
}

}  // namespace win
}  // namespace base
