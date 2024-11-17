// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#ifndef CC_PAINT_PAINT_OP_BUFFER_H_
#define CC_PAINT_PAINT_OP_BUFFER_H_

#include <limits>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "base/bits.h"
#include "base/check_op.h"
#include "base/functional/callback.h"
#include "base/memory/aligned_memory.h"
#include "base/memory/stack_allocated.h"
#include "cc/paint/paint_export.h"
#include "cc/paint/scroll_offset_map.h"
#include "third_party/skia/include/core/SkM44.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "ui/gfx/display_color_spaces.h"

class SkCanvas;
class SkColorSpace;
class SkImage;
class SkStrikeClient;
class SkStrikeServer;

namespace gpu {
struct Mailbox;
}

namespace cc {

class ClientPaintCache;
class ImageProvider;
class PaintOp;
class PaintRecord;
class ServicePaintCache;
class SkottieSerializationHistory;
class TransferCacheDeserializeHelper;
class TransferCacheSerializeHelper;

enum class PaintOpType : uint8_t;

struct CC_PAINT_EXPORT PlaybackCallbacks {
  STACK_ALLOCATED();

 public:
  PlaybackCallbacks();
  ~PlaybackCallbacks();
  PlaybackCallbacks(const PlaybackCallbacks&);
  PlaybackCallbacks& operator=(const PlaybackCallbacks&);

  using CustomDataRasterCallback =
      base::RepeatingCallback<void(SkCanvas* canvas, uint32_t id)>;
  using DidDrawOpCallback = base::RepeatingCallback<void()>;
  // This callback returns
  // - &op if no conversion;
  // - a pointer (owned by the callback) to a new op;
  // - null if the op should be discarded.
  using ConvertOpCallback =
      base::RepeatingCallback<const PaintOp*(const PaintOp& op)>;

  CustomDataRasterCallback custom_callback;
  DidDrawOpCallback did_draw_op_callback;
  ConvertOpCallback convert_op_callback;
};

struct CC_PAINT_EXPORT PlaybackParams {
  STACK_ALLOCATED();

 public:
  explicit PlaybackParams(
      ImageProvider* image_provider = nullptr,
      const SkM44& original_ctm = SkM44(),
      const PlaybackCallbacks& callbacks = PlaybackCallbacks());
  ~PlaybackParams();

  ImageProvider* image_provider = nullptr;
  SkM44 original_ctm;
  PlaybackCallbacks callbacks;
  std::optional<bool> save_layer_alpha_should_preserve_lcd_text;
  const ScrollOffsetMap* raster_inducing_scroll_offsets = nullptr;
  bool is_analyzing = false;
};

class CC_PAINT_EXPORT SharedImageProvider {
 public:
  enum class Error {
    kNoError,
    kUnknownMailbox,
    kNoAccess,
    kSkImageCreationFailed,
  };

  virtual ~SharedImageProvider() = default;
  virtual sk_sp<SkImage> OpenSharedImageForRead(const gpu::Mailbox& mailbox,
                                                Error& error) = 0;
};

// Defined outside of the class as this const is used in multiple files.
static constexpr int kMinNumberOfSlowPathsForMSAA = 6;

// PaintOpBuffer is a reimplementation of SkLiteDL.
// See: third_party/skia/src/core/SkLiteDL.h.
class CC_PAINT_EXPORT PaintOpBuffer : public SkRefCnt {
 public:
  struct CC_PAINT_EXPORT SerializeOptions {
    STACK_ALLOCATED();

   public:
    SerializeOptions();
    SerializeOptions(
        ImageProvider* image_provider,
        TransferCacheSerializeHelper* transfer_cache,
        ClientPaintCache* paint_cache,
        SkStrikeServer* strike_server,
        sk_sp<SkColorSpace> color_space,
        SkottieSerializationHistory* skottie_serialization_history,
        bool can_use_lcd_text,
        bool context_supports_distance_field_text,
        int max_texture_size,
        const ScrollOffsetMap* raster_inducing_scroll_offsets = nullptr);
    SerializeOptions(const SerializeOptions&);
    SerializeOptions& operator=(const SerializeOptions&);
    ~SerializeOptions();

    // Required.
    ImageProvider* image_provider = nullptr;
    TransferCacheSerializeHelper* transfer_cache = nullptr;
    ClientPaintCache* paint_cache = nullptr;
    SkStrikeServer* strike_server = nullptr;
    sk_sp<SkColorSpace> color_space;
    SkottieSerializationHistory* skottie_serialization_history = nullptr;
    bool can_use_lcd_text = false;
    bool context_supports_distance_field_text = true;
    int max_texture_size = 0;
    const ScrollOffsetMap* raster_inducing_scroll_offsets = nullptr;

    // TODO(crbug.com/40136055): Cleanup after study completion.
    //
    // If true, perform serializaion in a way that avoids serializing transient
    // members, such as IDs, so that a stable digest can be calculated. This
    // means that serialized output can't be deserialized correctly.
    bool for_identifiability_study = false;
  };

  struct CC_PAINT_EXPORT DeserializeOptions {
    STACK_ALLOCATED();

   public:
    TransferCacheDeserializeHelper* transfer_cache = nullptr;
    ServicePaintCache* paint_cache = nullptr;
    SkStrikeClient* strike_client = nullptr;
    // Used to memcpy Skia flattenables into to avoid TOCTOU issues.
    std::vector<uint8_t>& scratch_buffer;
    // Do a DumpWithoutCrashing when serialization fails.
    bool crash_dump_on_failure = false;
    // True if the deserialization is happening on a privileged gpu channel.
    // e.g. in the case of UI.
    bool is_privileged = false;
    // The HDR headroom to apply when deserializing.
    // TODO(crbug.com/40281980): Move this to playback instead of
    // deserialization.
    float hdr_headroom = 1.f;
    SharedImageProvider* shared_image_provider = nullptr;
  };

  enum { kInitialBufferSize = 4096 };
  static constexpr size_t kPaintOpAlign = 8;
  template <typename Op>
  static constexpr uint16_t ComputeOpAlignedSize() {
    constexpr size_t size = base::bits::AlignUp(sizeof(Op), kPaintOpAlign);
    static_assert(size <= std::numeric_limits<uint16_t>::max());
    return static_cast<uint16_t>(size);
  }

  PaintOpBuffer();
  PaintOpBuffer(const PaintOpBuffer&) = delete;
  // `other` will be reset to the initial state.
  PaintOpBuffer(PaintOpBuffer&& other);
  ~PaintOpBuffer() override;

  PaintOpBuffer& operator=(const PaintOpBuffer&) = delete;
  // `other` will be reset in the initial state.
  PaintOpBuffer& operator=(PaintOpBuffer&& other);

  // Resets the PaintOpBuffer to the initial state, except that the current
  // data buffer is retained.
  void Reset();

  // Replays the paint op buffer into the canvas.
  void Playback(SkCanvas* canvas) const;
  void Playback(SkCanvas* canvas,
                const PlaybackParams& params,
                bool local_ctm = true) const;

  // Deserialize PaintOps from |input|. The original content will be
  // overwritten.
  bool Deserialize(const volatile void* input,
                   size_t input_size,
                   const DeserializeOptions& options);

  static sk_sp<PaintOpBuffer> MakeFromMemory(const volatile void* input,
                                             size_t input_size,
                                             const DeserializeOptions& options);

  // Given the |bounds| of a PaintOpBuffer that would be transformed by |ctm|
  // when rendered, compute the bounds needed to raster the buffer at a fixed
  // scale into an auxiliary image instead of rasterizing at scale dynamically.
  // This is used to enforce scaling decisions made pre-serialization when
  // rasterizing after deserializing the buffer.
  static SkRect GetFixedScaleBounds(const SkMatrix& ctm,
                                    const SkRect& bounds,
                                    int max_texture_size = 0);

  // Returns the size of the paint op buffer. That is, the number of ops
  // contained in it.
  size_t size() const { return op_count_; }
  bool empty() const { return !size(); }

  // Returns the number of bytes used by the paint op buffer.
  size_t bytes_used() const {
    return sizeof(*this) + reserved_ + subrecord_bytes_used_;
  }
  // Returns the number of bytes used by paint ops.
  size_t paint_ops_size() const { return used_ + subrecord_bytes_used_; }
  // Returns the total number of ops including sub-records.
  size_t total_op_count() const { return op_count_ + subrecord_op_count_; }

  size_t next_op_offset() const { return used_; }
  int num_slow_paths_up_to_min_for_MSAA() const {
    return num_slow_paths_up_to_min_for_MSAA_;
  }
  bool has_non_aa_paint() const { return has_non_aa_paint_; }
  bool has_draw_ops() const { return has_draw_ops_; }
  bool has_draw_text_ops() const { return has_draw_text_ops_; }
  bool has_save_layer_ops() const { return has_save_layer_ops_; }
  bool has_save_layer_alpha_ops() const { return has_save_layer_alpha_ops_; }
  bool has_effects_preventing_lcd_text_for_save_layer_alpha() const {
    return has_effects_preventing_lcd_text_for_save_layer_alpha_;
  }
  bool has_discardable_images() const { return has_discardable_images_; }
  gfx::ContentColorUsage content_color_usage() const {
    return content_color_usage_;
  }
  bool NeedsAdditionalInvalidationForLCDText(
      const PaintOpBuffer& old_buffer) const;

  // Resize the PaintOpBuffer to exactly fit the current amount of used space.
  void ShrinkToFit();

  // Takes the contents of this as a PaintRecord. The result is shrunk to fit.
  // If the shrinking-to-fit allocates a new data buffer, this PaintOpBuffer
  // retains the original data buffer for future use.
  PaintRecord ReleaseAsRecord();
  PaintRecord DeepCopyAsRecord();

  bool EqualsForTesting(const PaintOpBuffer& other) const;

  const PaintOp& GetFirstOp() const {
    DCHECK(!empty());
    return reinterpret_cast<const PaintOp&>(*data_);
  }

  template <typename T, typename... Args>
  const T& push(Args&&... args) {
    DCHECK(is_mutable());
    static_assert(std::is_base_of<PaintOp, T>::value, "T not a PaintOp.");
    static_assert(alignof(T) <= kPaintOpAlign, "");
    uint16_t aligned_size = ComputeOpAlignedSize<T>();
    T* op = reinterpret_cast<T*>(AllocatePaintOp(aligned_size));

    new (op) T{std::forward<Args>(args)...};
    DCHECK_EQ(op->type, static_cast<uint8_t>(T::kType));
    DCHECK_EQ(aligned_size, op->AlignedSize());
    AnalyzeAddedOp(op);
    return *op;
  }

  void UpdateSaveLayerBounds(size_t offset, const SkRect& bounds);

  template <typename T>
  void AnalyzeAddedOp(const T* op) {
    static_assert(!std::is_same<T, PaintOp>::value,
                  "AnalyzeAddedOp needs a subtype of PaintOp");
    DCHECK(is_mutable());
    DCHECK(op->IsValid());

    if (num_slow_paths_up_to_min_for_MSAA_ < kMinNumberOfSlowPathsForMSAA) {
      num_slow_paths_up_to_min_for_MSAA_ += op->CountSlowPathsFromFlags();
      num_slow_paths_up_to_min_for_MSAA_ += op->CountSlowPaths();
    }

    has_non_aa_paint_ |= op->HasNonAAPaint();

    subrecord_bytes_used_ += op->AdditionalBytesUsed();
    subrecord_op_count_ += op->AdditionalOpCount();

    has_draw_ops_ |= op->IsDrawOp();
    has_draw_text_ops_ |= op->HasDrawTextOps();
    has_save_layer_ops_ |= op->HasSaveLayerOps();
    has_save_layer_alpha_ops_ |= op->HasSaveLayerAlphaOps();
    has_effects_preventing_lcd_text_for_save_layer_alpha_ |=
        op->HasEffectsPreventingLCDTextForSaveLayerAlpha();

    has_discardable_images_ |= op->HasDiscardableImages(&content_color_usage_);
    has_discardable_images_ |=
        op->HasDiscardableImagesFromFlags(&content_color_usage_);
  }

  size_t GetOpOffsetForTracing(const PaintOp& op) const {
    DCHECK_GE(reinterpret_cast<const char*>(&op), data_.get());
    size_t result =
        static_cast<size_t>(reinterpret_cast<const char*>(&op) - data_.get());
    DCHECK_LT(result, used_);
    return result;
  }

  const char* DataBufferForTesting() const { return data_.get(); }

  const PaintOp& GetOpAtForTesting(size_t index) const;

  class Iterator;
  class OffsetIterator;
  class CompositeIterator;
  class PlaybackFoldingIterator;

  // STL-like container support:
  using value_type = PaintOp;
  using const_iterator = Iterator;
  Iterator begin() const;
  Iterator end() const;

 private:
  friend class DisplayItemList;
  friend class PaintOp;
  friend class PaintOpBufferOffsetsTest;
  friend class SolidColorAnalyzer;
  using BufferDataPtr = std::unique_ptr<char, base::AlignedFreeDeleter>;

  bool is_mutable() const { return unique(); }

  void DestroyOps();

  // Replays the paint op buffer into the canvas. If |indices| is specified, it
  // contains indices in an increasing order and only the indices specified in
  // the vector will be replayed.
  void Playback(SkCanvas* canvas,
                const PlaybackParams& params,
                bool local_ctm,
                const std::vector<size_t>* offsets) const;

  // Creates a new buffer sized to `new_size`, copying the old to the new (if
  // the old exists). Returns the old buffer.
  BufferDataPtr ReallocBuffer(size_t new_size);

  // Shrinks the buffer to fit `used_`. Returns the old buffer if this
  // allocated a new buffer, or nullptr.
  BufferDataPtr ReallocIfNeededToFit();

  // Returns the allocated op.
  void* AllocatePaintOp(uint16_t aligned_size) {
    DCHECK(is_mutable());
    if (used_ + aligned_size > reserved_) {
      return AllocatePaintOpSlowPath(aligned_size);
    } else {
      void* op = data_.get() + used_;
      used_ += aligned_size;
      op_count_++;
      return op;
    }
  }
  void* AllocatePaintOpSlowPath(uint16_t aligned_size);

  void ResetRetainingBuffer();

  BufferDataPtr data_;
  size_t used_ = 0;
  size_t reserved_ = 0;
  size_t op_count_ = 0;

  // Record additional bytes used by referenced sub-records and display lists.
  size_t subrecord_bytes_used_ = 0;
  // Record total op count of referenced sub-record and display lists.
  size_t subrecord_op_count_ = 0;
  // Record paths for veto-to-msaa for gpu raster. Counting slow paths can be
  // very expensive, we stop counting them once reaching the minimum number
  // required for an MSAA sample count for raster.
  int num_slow_paths_up_to_min_for_MSAA_ = 0;

  bool has_non_aa_paint_ : 1 = false;
  bool has_draw_ops_ : 1 = false;
  bool has_draw_text_ops_ : 1 = false;
  bool has_save_layer_ops_ : 1 = false;
  bool has_save_layer_alpha_ops_ : 1 = false;
  bool has_effects_preventing_lcd_text_for_save_layer_alpha_ : 1 = false;

  bool has_discardable_images_ : 1 = false;
  gfx::ContentColorUsage content_color_usage_ = gfx::ContentColorUsage::kSRGB;
};

}  // namespace cc

#endif  // CC_PAINT_PAINT_OP_BUFFER_H_
