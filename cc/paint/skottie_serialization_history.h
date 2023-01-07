// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_PAINT_SKOTTIE_SERIALIZATION_HISTORY_H_
#define CC_PAINT_SKOTTIE_SERIALIZATION_HISTORY_H_

#include "base/containers/flat_map.h"
#include "base/synchronization/lock.h"
#include "base/thread_annotations.h"
#include "cc/paint/paint_export.h"
#include "cc/paint/paint_flags.h"
#include "cc/paint/paint_image.h"
#include "cc/paint/skottie_frame_data.h"
#include "cc/paint/skottie_resource_metadata.h"
#include "cc/paint/skottie_text_property_value.h"

namespace cc {

class SkottieWrapper;

// For each frame of a Skottie animation that uses out-of-process rasterization:
// 1) The animation's state is captured in a DrawSkottieOp object.
// 2) The object is serialized, transmitted over IPC, then deserialized.
// 3) The animation's state is updated in the target raster process.
// 4) The new frame is rendered/rasterized.
//
// SkottieSerializationHistory keeps track of past animation state that has
// been serialized and transmitted for rasterization. This allows the pipeline
// to be optimized such that only "new"/"updated" animation state is transmitted
// for each frame rather than re-transmitting everything. In other words, step
// 2) can use this history to filter out existing animation state that is
// already reflected in the target process from a previous frame in step 3).
//
// This class is thread-safe.
class CC_PAINT_EXPORT SkottieSerializationHistory {
 public:
  // Number of RequestInactiveAnimationsPurge() calls that must be made before
  // trying to purge stale skottie animations from history (referred to as
  // doing a "purge check"). If an animation has had no activity since the last
  // purge check, it gets deleted.
  static constexpr int kDefaultPurgePeriod = 1000;

  explicit SkottieSerializationHistory(int purge_period = kDefaultPurgePeriod);
  SkottieSerializationHistory(const SkottieSerializationHistory&) = delete;
  SkottieSerializationHistory& operator=(const SkottieSerializationHistory&) =
      delete;
  ~SkottieSerializationHistory();

  // Given the set of |images| and the |text_map| in the |skottie| animation's
  // new frame, filter out the entries whose contents have not changed since the
  // last frame. If an entry *has* changed, it is kept in its corresponding
  // output argument and the history is updated internally.
  void FilterNewSkottieFrameState(const SkottieWrapper& skottie,
                                  SkottieFrameDataMap& images,
                                  SkottieTextPropertyValueMap& text_map);

  // Purges the history of any Skottie animations that have been inactive for
  // a while. Although an animation's history is rather light-weight, this is
  // done as a safeguard to prevent accumulating stale data in the long run.
  //
  // Note this method is intentionally designed to be called frequently; it's
  // cheap and does not trigger an actual purge most of the time. Ideally, an
  // animation's history would be automatically purged when the animation
  // finishes, but that is difficult to observe at this layer of the code.
  void RequestInactiveAnimationsPurge();

 private:
  // Uniquely identifies the contents of a SkottieFrameData instance (used to
  // tell when an image asset's contents change from one animation frame to
  // another).
  struct SkottieFrameDataId {
    explicit SkottieFrameDataId(const SkottieFrameData& frame_data);

    bool operator==(const SkottieFrameDataId& other) const;
    bool operator!=(const SkottieFrameDataId& other) const;

    // Only store the id. Storing the whole PaintImage is doable but can
    // potentially keep some ref-counted members alive longer than necessary. So
    // just store the bare minimum to identify the image.
    PaintImage::Id paint_image_id;
    PaintFlags::FilterQuality quality;
  };

  // History for an individual SkottieWrapper (animation instance).
  class SkottieWrapperHistory {
   public:
    static constexpr int kInitialSequenceId = 1;

    SkottieWrapperHistory(const SkottieFrameDataMap& initial_images,
                          const SkottieTextPropertyValueMap& initial_text_map);
    SkottieWrapperHistory(const SkottieWrapperHistory& other);
    SkottieWrapperHistory& operator=(const SkottieWrapperHistory& other);
    ~SkottieWrapperHistory();

    void FilterNewState(SkottieFrameDataMap& images,
                        SkottieTextPropertyValueMap& text_map);

    // The "sequence_id" is incremented each time the caller tries to update an
    // animation's history, regardless of whether its history is ultimately
    // mutated. This is just used an indicator that the animation is still alive
    // and playing, not for judging when its state has changed.
    int current_sequence_id() const { return current_sequence_id_; }
    int sequence_id_at_last_purge_check() const {
      return sequence_id_at_last_purge_check_;
    }
    void update_sequence_id_at_last_purge_check() {
      sequence_id_at_last_purge_check_ = current_sequence_id_;
    }

   private:
    void FilterNewFrameImages(SkottieFrameDataMap& images);
    void FilterNewTextPropertyValues(SkottieTextPropertyValueMap& text_map);

    int current_sequence_id_ = kInitialSequenceId;
    int sequence_id_at_last_purge_check_ = kInitialSequenceId;
    base::flat_map<SkottieResourceIdHash, SkottieFrameDataId>
        last_frame_data_per_asset_;
    SkottieTextPropertyValueMap accumulated_text_map_;
  };

  base::Lock mutex_;
  base::flat_map</*SkottieWrapper id*/ uint32_t, SkottieWrapperHistory>
      history_per_animation_ GUARDED_BY(mutex_);
  int GUARDED_BY(mutex_) purge_period_counter_ = 0;
  const int purge_period_;
};

}  // namespace cc

#endif  // CC_PAINT_SKOTTIE_SERIALIZATION_HISTORY_H_
