// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_LAYERS_PICTURE_LAYER_H_
#define CC_LAYERS_PICTURE_LAYER_H_

#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "cc/base/devtools_instrumentation.h"
#include "cc/base/invalidation_region.h"
#include "cc/benchmarks/micro_benchmark_controller.h"
#include "cc/layers/layer.h"
#include "cc/layers/recording_source.h"

namespace cc {

class ContentLayerClient;
class DisplayItemList;
class RasterSource;

class CC_EXPORT PictureLayer : public Layer {
 public:
  static scoped_refptr<PictureLayer> Create(ContentLayerClient* client);

  PictureLayer(const PictureLayer&) = delete;
  PictureLayer& operator=(const PictureLayer&) = delete;

  void ClearClient();

  void SetIsBackdropFilterMask(bool is_backdrop_filter_mask);
  bool is_backdrop_filter_mask() const { return is_backdrop_filter_mask_; }

  // Layer interface.
  std::unique_ptr<LayerImpl> CreateLayerImpl(
      LayerTreeImpl* tree_impl) const override;
  void SetLayerTreeHost(LayerTreeHost* host) override;
  void PushPropertiesTo(LayerImpl* layer,
                        const CommitState& commit_state,
                        const ThreadUnsafeCommitState& unsafe_state) override;
  void SetNeedsDisplayRect(const gfx::Rect& layer_rect) override;
  bool RequiresSetNeedsDisplayOnHdrHeadroomChange() const override;
  sk_sp<const SkPicture> GetPicture() const override;
  bool Update() override;
  void RunMicroBenchmark(MicroBenchmark* benchmark) override;
  void CaptureContent(const gfx::Rect& rect,
                      std::vector<NodeInfo>* content) const override;

  ContentLayerClient* client() { return client_; }

  RecordingSource& GetRecordingSourceForTesting() {
    return recording_source_.Write(*this);
  }
  const RecordingSource& GetRecordingSourceForTesting() const {
    return recording_source_.Read(*this);
  }

 protected:
  explicit PictureLayer(ContentLayerClient* client);
  ~PictureLayer() override;

  bool HasDrawableContent() const override;

  // Can be overridden in tests to customize RasterSource.
  virtual scoped_refptr<RasterSource> CreateRasterSource() const;

 private:
  friend class TestSerializationPictureLayer;

  // Called on impl thread
  void DropRecordingSourceContentIfInvalid(int source_frame_number);

  const DisplayItemList* GetDisplayItemList() const;

  // These fields are not protected because they are only modified during
  // LayerTreeHost::PaintContent().
  raw_ptr<ContentLayerClient, DanglingUntriaged> client_ = nullptr;
  bool is_backdrop_filter_mask_ = false;

  ProtectedSequenceWritable<RecordingSource> recording_source_;
  ProtectedSequenceForbidden<devtools_instrumentation::ScopedLayerObjectTracker>
      instrumentation_object_tracker_;

  ProtectedSequenceWritable<Region> last_updated_invalidation_;

  ProtectedSequenceReadable<int> update_source_frame_number_;
};

}  // namespace cc

#endif  // CC_LAYERS_PICTURE_LAYER_H_
