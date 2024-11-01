// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/fuzztest_init_helper.h"

void (*fuzztest_init_helper::initialization_function)(int argc, char** argv);
