// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TREES_LAYER_TREE_MUTATOR_H_
#define CC_TREES_LAYER_TREE_MUTATOR_H_

#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "base/check.h"
#include "base/functional/callback_forward.h"
#include "base/time/time.h"
#include "cc/cc_export.h"
#include "cc/trees/animation_effect_timings.h"
#include "cc/trees/animation_options.h"

namespace cc {

// TOOD(kevers): Remove kDrop once confirmed that it is no longer needed under
// any circumstances.
enum class MutateQueuingStrategy {
  kDrop,                           // Discard request if busy.
  kQueueHighPriority,              // Queues request if busy. This request is
                                   // is next to run in the queue. Only one
                                   // high priority request can be in-flight
                                   // at any point in time.
  kQueueAndReplaceNormalPriority,  // Queues request if busy. This request
                                   // replaces an existing normal priority
                                   // request. In the case of mutations cycles
                                   // that cannot keep up with the frame rate,
                                   // replaced mutation requests are dropped
                                   // from the queue.
};

enum class MutateStatus {
  kCompletedWithUpdate,  // Mutation cycle successfully ran to completion with
                         // at least one update.
  kCompletedNoUpdate,    // Mutation cycle successfully ran to completion but
                         // no update was applied.
  kCanceled              // Mutation cycle dropped from the input queue.
};

struct CC_EXPORT WorkletAnimationId {
  // Uniquely identifies the animation worklet with which this animation is
  // associated.
  int worklet_id;
  // Uniquely identifies the animation within its animation worklet. Note that
  // animation_id is only guaranteed to be unique per animation worklet.
  int animation_id;

  // Initialize with invalid id.
  WorkletAnimationId() : worklet_id(0), animation_id(0) {}
  WorkletAnimationId(int worklet_id, int animation_id)
      : worklet_id(worklet_id), animation_id(animation_id) {}

  inline bool operator==(const WorkletAnimationId& rhs) const {
    return (this->worklet_id == rhs.worklet_id) &&
           (this->animation_id == rhs.animation_id);
  }
  // Returns true if the WorkletAnimationId has been initialized with a valid
  // id.
  explicit operator bool() const { return !!worklet_id || !!animation_id; }
};

struct CC_EXPORT AnimationWorkletInput {
  struct CC_EXPORT AddAndUpdateState {
    WorkletAnimationId worklet_animation_id;
    // Name associated with worklet animation.
    std::string name;
    // Worklet animation's current time, from its associated timeline.
    double current_time;
    std::unique_ptr<AnimationOptions> options;
    std::unique_ptr<AnimationEffectTimings> effect_timings;

    AddAndUpdateState(WorkletAnimationId worklet_animation_id,
                      std::string name,
                      double current_time,
                      std::unique_ptr<AnimationOptions> options,
                      std::unique_ptr<AnimationEffectTimings> effect_timings);

    AddAndUpdateState(AddAndUpdateState&&);
    ~AddAndUpdateState();
  };
  struct CC_EXPORT UpdateState {
    WorkletAnimationId worklet_animation_id;
    // Worklet animation's current time, from its associated timeline.
    double current_time = 0;
  };

  // Note: When adding any new fields please also update ValidateScope to
  // reflect them if necessary.
  std::vector<AddAndUpdateState> added_and_updated_animations;
  std::vector<UpdateState> updated_animations;
  std::vector<WorkletAnimationId> removed_animations;

  AnimationWorkletInput();
  AnimationWorkletInput(const AnimationWorkletInput&) = delete;
  ~AnimationWorkletInput();

  AnimationWorkletInput& operator=(const AnimationWorkletInput&) = delete;

#if DCHECK_IS_ON()
  // Verifies all animation states have the expected worklet id.
  bool ValidateId(int worklet_id) const;
#endif
};

class CC_EXPORT MutatorInputState {
 public:
  MutatorInputState();
  MutatorInputState(const MutatorInputState&) = delete;
  ~MutatorInputState();

  MutatorInputState& operator=(const MutatorInputState&) = delete;

  bool IsEmpty() const;
  void Add(AnimationWorkletInput::AddAndUpdateState&& state);
  void Update(AnimationWorkletInput::UpdateState&& state);
  void Remove(WorkletAnimationId worklet_animation_id);

  // Returns input for animation worklet with the given |scope_id| and nullptr
  // if there is no input.
  std::unique_ptr<AnimationWorkletInput> TakeWorkletState(int scope_id);

 private:
  using InputMap =
      std::unordered_map<int, std::unique_ptr<AnimationWorkletInput>>;

  // Maps a scope id to its associated AnimationWorkletInput instance.
  // Only contains scope ids for which there is a non-empty input.
  InputMap inputs_;

  // Returns iterator pointing to the entry in |inputs_| map whose key is id. It
  // inserts a new entry if none exists.
  AnimationWorkletInput& EnsureWorkletEntry(int id);
};

struct CC_EXPORT AnimationWorkletOutput {
  struct CC_EXPORT AnimationState {
    explicit AnimationState(WorkletAnimationId);
    AnimationState(const AnimationState&);
    ~AnimationState();

    WorkletAnimationId worklet_animation_id;
    std::vector<std::optional<base::TimeDelta>> local_times;
  };

  AnimationWorkletOutput();
  ~AnimationWorkletOutput();

  std::vector<AnimationState> animations;
};

// LayerTreeMutatorClient processes worklet outputs individually so we can
// define mutator output to be the same as animation worklet output.
using MutatorOutputState = AnimationWorkletOutput;

class LayerTreeMutatorClient {
 public:
  // Called when mutator needs to update its output.
  //
  // |output_state|: Most recent output of the mutator.
  virtual void SetMutationUpdate(
      std::unique_ptr<MutatorOutputState> output_state) = 0;
};

class CC_EXPORT LayerTreeMutator {
 public:
  virtual ~LayerTreeMutator() {}

  virtual void SetClient(LayerTreeMutatorClient* client) = 0;

  using DoneCallback = base::OnceCallback<void(MutateStatus)>;

  virtual bool Mutate(std::unique_ptr<MutatorInputState> input_state,
                      MutateQueuingStrategy queueing_strategy,
                      DoneCallback done_callback) = 0;
  // TODO(majidvp): Remove when timeline inputs are known.
  virtual bool HasMutators() = 0;
};

}  // namespace cc

#endif  // CC_TREES_LAYER_TREE_MUTATOR_H_
