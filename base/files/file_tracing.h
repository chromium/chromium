// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_FILES_FILE_TRACING_H_
#define BASE_FILES_FILE_TRACING_H_

#include <stdint.h>

#include "base/base_export.h"
#include "base/memory/raw_ptr.h"
#include "base/trace_event/trace_event.h"
#include "third_party/perfetto/include/perfetto/tracing/string_helpers.h"

#define FILE_TRACING_PREFIX "File"

#define SCOPED_FILE_TRACE_WITH_SIZE(name, size)          \
  TRACE_EVENT(TRACE_DISABLED_BY_DEFAULT("file"),         \
              FILE_TRACING_PREFIX "::" name,             \
              perfetto::Flow::FromPointer(this), "path", \
              this->path_.AsUTF8Unsafe(), "size", size);

#define SCOPED_FILE_TRACE(name) SCOPED_FILE_TRACE_WITH_SIZE(name, 0)

#endif  // BASE_FILES_FILE_TRACING_H_
