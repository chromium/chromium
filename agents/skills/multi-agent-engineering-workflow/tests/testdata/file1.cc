// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"

#define CONCAT_LOG(x) LOG(x)
#define MAGI_LOG CONCAT_LOG(INFO)

void MockFunction() {
  MAGI_LOG << "Hello World";
}
