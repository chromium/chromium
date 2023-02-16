// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/slim/test_frame_sink_impl.h"

#include <memory>
#include <utility>

#include "base/task/single_thread_task_runner.h"
#include "components/viz/test/test_context_provider.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "services/viz/public/mojom/compositing/compositor_frame_sink.mojom.h"

namespace cc::slim {

// static
std::unique_ptr<TestFrameSinkImpl> TestFrameSinkImpl::Create() {
  auto task_runner = base::SingleThreadTaskRunner::GetCurrentDefault();
  mojo::PendingAssociatedRemote<viz::mojom::CompositorFrameSink> sink_remote;
  mojo::PendingAssociatedReceiver<viz::mojom::CompositorFrameSink>
      sink_receiver = sink_remote.InitWithNewEndpointAndPassReceiver();
  mojo::PendingReceiver<viz::mojom::CompositorFrameSinkClient> client;
  auto context_provider = viz::TestContextProvider::Create();

  return base::WrapUnique(new TestFrameSinkImpl(
      std::move(task_runner), std::move(sink_remote), std::move(client),
      std::move(context_provider), std::move(sink_receiver)));
}

TestFrameSinkImpl::TestFrameSinkImpl(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    mojo::PendingAssociatedRemote<viz::mojom::CompositorFrameSink>
        compositor_frame_sink_associated_remote,
    mojo::PendingReceiver<viz::mojom::CompositorFrameSinkClient>
        client_receiver,
    scoped_refptr<viz::ContextProvider> context_provider,
    mojo::PendingAssociatedReceiver<viz::mojom::CompositorFrameSink>
        sink_receiver)
    : FrameSinkImpl(std::move(task_runner),
                    std::move(compositor_frame_sink_associated_remote),
                    std::move(client_receiver),
                    std::move(context_provider),
                    base::kInvalidThreadId),
      pending_sink_receiver_(std::move(sink_receiver)) {}

TestFrameSinkImpl::~TestFrameSinkImpl() = default;

bool TestFrameSinkImpl::BindToClient(FrameSinkImplClient* client) {
  DCHECK(!bind_to_client_called_);
  client_ = client;
  bind_to_client_called_ = true;
  return bind_to_client_result_;
}

void TestFrameSinkImpl::SetNeedsBeginFrame(bool needs_begin_frame) {
  needs_begin_frames_ = needs_begin_frame;
}

}  // namespace cc::slim
