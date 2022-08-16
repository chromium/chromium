// Copyright (c) 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <__assert>
#include <__config>
#include <__verbose_abort>

#include "base/base_export.h"
#include "base/immediate_crash.h"

_LIBCPP_BEGIN_NAMESPACE_STD

_LIBCPP_NORETURN BASE_EXPORT void __libcpp_verbose_abort(char const* format,
                                                         ...) {
  IMMEDIATE_CRASH();
}

_LIBCPP_END_NAMESPACE_STD
