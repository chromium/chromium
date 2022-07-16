// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_MEMORY_SCOPED_POLICY_H_
#define BASE_MEMORY_SCOPED_POLICY_H_

namespace base {
namespace scoped_policy {

// Defines the ownership policy for a scoped object.
enum OwnershipPolicy {
  // The scoped object takes ownership of an object by taking over an existing
  // ownership claim.
  // 作用域对象通过接管现有的所有权声明来获得对象的所有权。
  ASSUME,

  // The scoped object will retain the object and any initial ownership is
  // not changed.
  // 作用域对象将保留该对象，并且任何初始所有权都不会更改。
  RETAIN
};

}  // namespace scoped_policy
}  // namespace base

#endif  // BASE_MEMORY_SCOPED_POLICY_H_
