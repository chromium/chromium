// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/cpp/module.h"

namespace pp {

// This symbol must be defined in order to link again the ppapi library.
Module* CreateModule() {
  return nullptr;
}

}  // namespace pp
