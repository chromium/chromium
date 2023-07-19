// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_FRAME_SINK_TEST_TEST_BEGIN_FRAME_SOURCE_H_
#define ASH_FRAME_SINK_TEST_TEST_BEGIN_FRAME_SOURCE_H_

#include "components/viz/common/frame_sinks/begin_frame_args.h"
#include "components/viz/common/frame_sinks/begin_frame_source.h"

#include "base/memory/raw_ptr.h"

namespace ash {

// This class helps test various FrameSinkHost implementations by exposing the
// API to request frames asynchronously alongside using
// `TestLayerTreeFrameSink`.
// Sample usage:
// auto layer_tree_frame_sink = std::make_unique<TestLayerTreeFrameSink>();
// auto begin_frame_source = td::make_unique<TestBeginFrameSource>();
//
// // Set the begin_source for the `LayerTreeFrameSinkClient`.
// layer_tree_frame_sink->client()->SetBeginFrameSource(
//        begin_frame_source_.get());
//
// // Request a frame from the client.
// begin_frame_source_->GetBeginFrameObserver()->OnBeginFrame(
//        CreateValidBeginFrameArgsForTesting());
class TestBeginFrameSource : public viz::BeginFrameSource {
 public:
  TestBeginFrameSource();

  TestBeginFrameSource(const TestBeginFrameSource&) = delete;
  TestBeginFrameSource& operator=(const TestBeginFrameSource&) = delete;

  ~TestBeginFrameSource() override;

  // viz::BeginFrameSource:
  void DidFinishFrame(viz::BeginFrameObserver* obs) override;
  void OnGpuNoLongerBusy() override;

  // Currently there can be only a single `BeginFrameObserver`.
  void AddObserver(viz::BeginFrameObserver* obs) override;
  void RemoveObserver(viz::BeginFrameObserver* obs) override;

  viz::BeginFrameObserver* GetBeginFrameObserver() const;

 private:
  raw_ptr<viz::BeginFrameObserver, AcrossTasksDanglingUntriaged> observer_;
};

viz::BeginFrameArgs CreateValidBeginFrameArgsForTesting();

}  // namespace ash

#endif  // ASH_FRAME_SINK_TEST_TEST_BEGIN_FRAME_SOURCE_H_
