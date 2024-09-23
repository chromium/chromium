// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_FRAME_SINK_FRAME_SINK_HOLDER_TEST_API_H_
#define ASH_FRAME_SINK_FRAME_SINK_HOLDER_TEST_API_H_

#include "base/memory/raw_ptr.h"
#include "ui/gfx/geometry/size.h"

namespace ash {

class FrameSinkHolder;

class FrameSinkHolderTestApi {
 public:
  explicit FrameSinkHolderTestApi(FrameSinkHolder* frame_sink_holder);

  FrameSinkHolderTestApi(const FrameSinkHolderTestApi&) = delete;
  FrameSinkHolderTestApi& operator=(const FrameSinkHolderTestApi&) = delete;

  ~FrameSinkHolderTestApi();

  const gfx::Size& LastSubmittedFrameSize() const;

  bool IsPendingFrameAck() const;

  bool IsPendingFrame() const;

  bool IsFirstFrameRequested() const;

  bool IsObservingBeginFrameSource() const;

 private:
  raw_ptr<FrameSinkHolder, AcrossTasksDanglingUntriaged> frame_sink_holder_;
};

}  // namespace ash

#endif  // ASH_FRAME_SINK_FRAME_SINK_HOLDER_TEST_API_H_
