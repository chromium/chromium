// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is a test intended to flush out potential build failures from mismatched
// declarations between absl's dynamic_annotations.h and the (unmaintained but
// still used) version in this directory.

#include "base/third_party/dynamic_annotations/dynamic_annotations.h"
#include "third_party/abseil-cpp/absl/base/dynamic_annotations.h"
