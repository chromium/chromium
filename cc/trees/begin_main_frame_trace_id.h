// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TREES_BEGIN_MAIN_FRAME_TRACE_ID_H_
#define CC_TREES_BEGIN_MAIN_FRAME_TRACE_ID_H_

#include "base/types/id_type.h"

namespace cc {

using BeginMainFrameTraceId = base::IdTypeU64<class BeginMainFrameTraceIdTag>;

}  // namespace cc

#endif  // CC_TREES_BEGIN_MAIN_FRAME_TRACE_ID_H_
