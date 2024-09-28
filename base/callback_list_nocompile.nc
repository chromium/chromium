// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is a "No Compile Test" suite.
// http://dev.chromium.org/developers/testing/no-compile-tests

#include "base/callback_list.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"

namespace base {

class Foo {
 public:
  Foo() = default;
  ~Foo() = default;
};

class FooListener {
 public:
  FooListener() = default;
  FooListener(const FooListener&) = delete;
  FooListener& operator=(const FooListener&) = delete;

  void GotAScopedFoo(std::unique_ptr<Foo> f) { foo_ = std::move(f); }

  std::unique_ptr<Foo> foo_;
};

// Callbacks run with a move-only typed parameter.
//
// CallbackList does not support move-only typed parameters. Notify() is
// designed to take zero or more parameters, and run each registered callback
// with them. With move-only types, the parameter will be set to NULL after the
// first callback has been run.
void WontCompile() {
  FooListener f;
  RepeatingCallbackList<void(std::unique_ptr<Foo>)> c1;
  CallbackListSubscription sub =
      c1.Add(BindRepeating(&FooListener::GotAScopedFoo, Unretained(&f)));
  c1.Notify(std::unique_ptr<Foo>(new Foo()));  // expected-error@*:* {{call to implicitly-deleted copy constructor of 'std::unique_ptr<base::Foo>'}}
}

}  // namespace base
