// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Okay since no review required.
#include "buildtools/checkdeps/testdata/requires_review/foo.h"
// Not okay since review required.
#include "buildtools/checkdeps/testdata/requires_review/sub/foo.h"
// Okay since no review required.
#include "buildtools/checkdeps/testdata/requires_review/sub/sub/no_review/foo.h"
// Okay since no review required.
#include "buildtools/checkdeps/testdata/requires_review/sub/sub/no_review/sub/foo.h"
