// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TREES_LAYER_TREE_MUTATOR_H_
#define CC_TREES_LAYER_TREE_MUTATOR_H_

#include "base/callback_forward.h"
#include "base/optional.h"
#include "base/time/time.h"
#include "cc/cc_export.h"
#include "cc/trees/animation_options.h"

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace cc {

struct CC_EXPORT WorkletAnimationId {
  // Uniquely identifies the animation worklet with which this animation is
  // associated.
  int scope_id;
  // Uniquely identifies the animation within its animation worklet. Note that
  // animation_id is only guaranteed to be unique per animation worklet.
  int animation_id;

  inline bool operator==(const WorkletAnimationId& rhs) const {
    return (this->scope_id == rhs.scope_id) &&
           (this->animation_id == rhs.animation_id);
  }
};

struct CC_EXPORT AnimationWorkletInput {
  struct CC_EXPORT AddAndUpdateState {
    WorkletAnimationId worklet_animation_id;
    // Name associated with worklet animation.
    std::string name;
    // Worklet animation's current time, from its associated timeline.
    double current_time;
    std::unique_ptr<AnimationOptions> options;
    int num_effects;

    AddAndUpdateState(WorkletAnimationId worklet_animation_id,
                      std::string name,
                      double current_time,
                      std::unique_ptr<AnimationOptions> options,
                      int num_effects);

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
  std::vector<WorkletAnimationId> peeked_animations;

  AnimationWorkletInput();
  ~AnimationWorkletInput();

#if DCHECK_IS_ON()
  // Verifies all animation states have the expected scope id.
  bool ValidateScope(int scope_id) const;
#endif
  DISALLOW_COPY_AND_ASSIGN(AnimationWorkletInput);
};

class CC_EXPORT MutatorInputState {
 public:
  MutatorInputState();
  ~MutatorInputState();

  bool IsEmpty() const;
  void Add(AnimationWorkletInput::AddAndUpdateState&& state);
  void Update(AnimationWorkletInput::UpdateState&& state);
  void Remove(WorkletAnimationId worklet_animation_id);
  // |Update| asks for the animation to *animate* given a current time and
  // return the output value while |Peek| only asks for the last output value
  // (if one available) without requiring animate or providing a current time.
  // In particular, composited animations are updated from compositor and peeked
  // from main thread.
  void Peek(WorkletAnimationId worklet_animation_id);

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

  DISALLOW_COPY_AND_ASSIGN(MutatorInputState);
};

struct CC_EXPORT AnimationWorkletOutput {
  struct CC_EXPORT AnimationState {
    explicit AnimationState(WorkletAnimationId);
    AnimationState(const AnimationState&);
    ~AnimationState();

    WorkletAnimationId worklet_animation_id;
    std::vector<base::Optional<base::TimeDelta>> local_times;
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

  virtual void Mutate(std::unique_ptr<MutatorInputState> input_state) = 0;
  // TODO(majidvp): Remove when timeline inputs are known.
  virtual bool HasMutators() = 0;
};

}  // namespace cc

#endif  // CC_TREES_LAYER_TREE_MUTATOR_H_
