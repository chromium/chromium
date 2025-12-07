// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TRACE_EVENT_BASE_TRACING_FORWARD_H_
#define BASE_TRACE_EVENT_BASE_TRACING_FORWARD_H_

// This header is a wrapper around perfetto's traced_value_forward.h that
// handles Chromium's ENABLE_BASE_TRACING buildflag.

#include "base/tracing_buildflags.h"

#include "third_party/perfetto/include/perfetto/tracing/traced_value_forward.h"  // nogncheck

#endif  // BASE_TRACE_EVENT_BASE_TRACING_FORWARD_H_
