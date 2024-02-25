// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is a "No Compile Test" suite.
// http://dev.chromium.org/developers/testing/no-compile-tests

#include "base/task/bind_post_task.h"

#include "base/task/sequenced_task_runner.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"

namespace base {

// In general, compilers only print diagnostics for errors once when initially
// instantiating the template. Subsequent uses of the same template will not
// print out any diagnostics.
//
// Using multiple functions that return different types ensures that each
// BindPostTask() call creates a new instantiation.
int ReturnsInt();
double ReturnsDouble();

void OnceCallbackWithNonVoidReturnType1() {
  {
    OnceCallback<int()> cb = BindOnce(&ReturnsInt);
    auto post_cb = BindPostTask(SequencedTaskRunner::GetCurrentDefault(), std::move(cb));  // expected-error@base/task/bind_post_task_nocompile.nc:* {{no matching function for call to 'BindPostTask'}}
                                                                                           // expected-note@base/task/bind_post_task.h:* {{because 'std::is_void_v<int>' evaluated to false}}

  }

  {
    OnceCallback<double()> cb = BindOnce(&ReturnsDouble);
    auto post_cb = BindPostTaskToCurrentDefault(std::move(cb));  // expected-error@base/task/bind_post_task_nocompile.nc:* {{no matching function for call to 'BindPostTaskToCurrentDefault'}}
  }                                                              // expected-note@base/task/bind_post_task.h:* {{because 'std::is_void_v<double>' evaluated to false}}

}

void RepeatingCallbackWithNonVoidReturnType() {
  {
    RepeatingCallback<int()> cb = BindRepeating(&ReturnsInt);
    auto post_cb = BindPostTask(SequencedTaskRunner::GetCurrentDefault(), std::move(cb));  // expected-error@base/task/bind_post_task_nocompile.nc:* {{no matching function for call to 'BindPostTask'}}
                                                                                           // expected-note@base/task/bind_post_task.h:* {{because 'std::is_void_v<int>' evaluated to false}}
  }

  {
    RepeatingCallback<double()> cb = BindRepeating(&ReturnsDouble);
    auto post_cb = BindPostTaskToCurrentDefault(std::move(cb));  // expected-error@base/task/bind_post_task_nocompile.nc:* {{no matching function for call to 'BindPostTaskToCurrentDefault'}}
                                                                 // expected-note@base/task/bind_post_task.h:* {{because 'std::is_void_v<double>' evaluated to false}}

  }
}

}  // namespace base
