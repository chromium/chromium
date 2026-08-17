// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_FRAME_SINK_FRAME_SINK_HOLDER_TEST_API_H_
#define ASH_FRAME_SINK_FRAME_SINK_HOLDER_TEST_API_H_

#include <cstddef>

#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "components/viz/common/resources/resource_id.h"
#include "ui/gfx/geometry/size.h"

namespace cc {
class ResourcePool;
}  // namespace cc

namespace viz {
class ClientResourceProvider;
}  // namespace viz

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

  cc::ResourcePool* resource_pool();
  viz::ClientResourceProvider* client_resource_provider();

  size_t GetExportedResourcesCount() const;

 private:
  raw_ptr<FrameSinkHolder, AcrossTasksDanglingUntriaged> frame_sink_holder_;
};

}  // namespace ash

#endif  // ASH_FRAME_SINK_FRAME_SINK_HOLDER_TEST_API_H_
