// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Okay since no review required.
#include "buildtools/checkdeps/testdata/requires_review/foo.h"
#include "buildtools/checkdeps/testdata/requires_review/inherited/foo.h"
// Okay since added to DEPS.
#include "buildtools/checkdeps/testdata/requires_review/sub/foo.h"
#include "buildtools/checkdeps/testdata/requires_review/sub/inherited/foo.h"
#include "buildtools/checkdeps/testdata/requires_review/sub/sub/foo.h"
#include "buildtools/checkdeps/testdata/requires_review/sub/sub/inherited/foo.h"
// Okay since no review required (and added to DEPS anyways).
#include "buildtools/checkdeps/testdata/requires_review/sub/sub/no_review/inherited/foo.h"
