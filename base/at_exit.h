// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_AT_EXIT_H_
#define BASE_AT_EXIT_H_

#include "base/base_export.h"
#include "base/callback.h"
#include "base/containers/stack.h"
#include "base/synchronization/lock.h"
#include "base/thread_annotations.h"

namespace base {

// This class provides a facility similar to the CRT atexit(), except that
// we control when the callbacks are executed. Under Windows for a DLL they
// happen at a really bad time and under the loader lock. This facility is
// mostly used by base::Singleton.
// 此类提供类似于 CRT atexit() 的工具，除了我们控制何时执行回调。在 Windows 下，
// 对于 DLL，它们发生在非常糟糕的时间和加载程序锁定下。这个工具主要由
// base::Singleton 使用。
//
// The usage is simple. Early in the main() or WinMain() scope create an
// AtExitManager object on the stack:
// 用法很简单。在 main() 或 WinMain() 作用域的早期，在堆栈上创建一个
// AtExitManager 对象：
// int main(...) {
//    base::AtExitManager exit_manager;
//
// }
// When the exit_manager object goes out of scope, all the registered
// callbacks and singleton destructors will be called.
// 当 exit_manager 对象超出范围时，将调用所有注册的回调和单例析构函数。
// 参考：https://chowdera.com/2022/02/202202280550401719.html
// 参考：https://illx10000.github.io/2018/12/26/4.html
// AtExitManager离开作用域，所有的回调函数将会被调用。
// AtExitManager类似于Linux下的atexit，注册退出清理函数，不过base库的实现机制是
// 利用了C++的RAII

class BASE_EXPORT AtExitManager {
 public:
  typedef void (*AtExitCallbackType)(void*);

  AtExitManager();
  AtExitManager(const AtExitManager&) = delete;
  AtExitManager& operator=(const AtExitManager&) = delete;

  // The dtor calls all the registered callbacks. Do not try to register more
  // callbacks after this point.
  ~AtExitManager();

  // Registers the specified function to be called at exit. The prototype of
  // the callback function is void func(void*).
  static void RegisterCallback(AtExitCallbackType func, void* param);

  // Registers the specified task to be called at exit.
  static void RegisterTask(base::OnceClosure task);

  // Calls the functions registered with RegisterCallback in LIFO order. It
  // is possible to register new callbacks after calling this function.
  static void ProcessCallbacksNow();

  // Disable all registered at-exit callbacks. This is used only in a single-
  // process mode.
  static void DisableAllAtExitManagers();

 protected:
  // This constructor will allow this instance of AtExitManager to be created
  // even if one already exists.  This should only be used for testing!
  // AtExitManagers are kept on a global stack, and it will be removed during
  // destruction.  This allows you to shadow another AtExitManager.
  explicit AtExitManager(bool shadow);

 private:
  base::Lock lock_;

  base::stack<base::OnceClosure> stack_ GUARDED_BY(lock_);

#if DCHECK_IS_ON()
  bool processing_callbacks_ GUARDED_BY(lock_) = false;
#endif

  // Stack of managers to allow shadowing.
  AtExitManager* const next_manager_;
};

#if defined(UNIT_TEST)
class ShadowingAtExitManager : public AtExitManager {
 public:
  ShadowingAtExitManager() : AtExitManager(true) {}
};
#endif  // defined(UNIT_TEST)

}  // namespace base

#endif  // BASE_AT_EXIT_H_
