// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_BASE_SYNCED_PROPERTY_H_
#define CC_BASE_SYNCED_PROPERTY_H_

#include "base/memory/ref_counted.h"

namespace cc {

// This class is the basic primitive used for impl-thread scrolling.  Its job is
// to sanely resolve the case where both the main and impl thread are
// concurrently updating the same value (for example, when Javascript sets the
// scroll offset during an ongoing impl-side scroll).
//
// There are three trees (main, pending, and active) and therefore also three
// places with their own idea of the scroll offsets (and analogous properties
// like page scale).  Objects of this class are meant to be held on the Impl
// side, and contain the canonical reference for the pending and active trees,
// as well as keeping track of the latest delta sent to the main thread (which
// is necessary for conflict resolution).

template <typename T>
class SyncedProperty : public base::RefCounted<SyncedProperty<T>> {
 public:
  using BaseT = typename T::BaseType;
  using DeltaT = typename T::DeltaType;

  // Returns the canonical value for the specified tree, including the sum of
  // all deltas.  The pending tree should use this for activation purposes and
  // the active tree should use this for drawing.
  BaseT Current(bool is_active_tree) const {
    if (is_active_tree)
      return T::ApplyDelta(active_base_, active_delta_);
    return T::ApplyDelta(pending_base_, PendingDelta());
  }

  // Sets the value on the impl thread, due to an impl-thread-originating
  // action.  Returns true if this had any effect.  This will remain
  // impl-thread-only information at first, and will get pulled back to the main
  // thread on the next call of PullDeltaForMainThread.
  bool SetCurrent(BaseT current) {
    DeltaT delta = T::DeltaBetweenBases(current, active_base_);
    if (active_delta_ == delta)
      return false;

    active_delta_ = delta;
    return true;
  }

  // Returns the difference between the last value that was committed and
  // activated from the main thread, and the current total value.
  DeltaT Delta() const { return active_delta_; }

  // Returns the latest active tree delta and also makes a note that this value
  // was sent to the main thread.
  DeltaT PullDeltaForMainThread(bool next_bmf) {
    DeltaT& target = next_bmf ? next_reflected_delta_in_main_tree_
                              : reflected_delta_in_main_tree_;
    DCHECK_EQ(target, T::IdentityDelta());
    target = UnsentDelta();
    return target;
  }

  // Push the latest value from the main thread onto pending tree-associated
  // state. Returns true if pushing the value results in different values
  // between the main layer tree and the pending tree.
  bool PushMainToPending(BaseT main_thread_value) {
    reflected_delta_in_pending_tree_ = reflected_delta_in_main_tree_;
    reflected_delta_in_main_tree_ = next_reflected_delta_in_main_tree_;
    next_reflected_delta_in_main_tree_ = T::IdentityDelta();
    pending_base_ = main_thread_value;

    return Current(false) != main_thread_value;
  }

  // Push the value associated with the pending tree to be the active base
  // value. As part of this, subtract the delta reflected in the pending tree
  // from the active tree delta (which will make the delta zero at steady state,
  // or make it contain only the difference since the last send).
  // Returns true if pushing the update results in:
  // 1) Different values on the pending tree and the active tree.
  // 2) An update to the current value on the active tree.
  // The reason for considering the second case only when pushing to the active
  // tree, as opposed to when pushing to the pending tree, is that only the
  // active tree computes state using this value which is not computed on the
  // pending tree and not pushed during activation (aka scrollbar geometries).
  bool PushPendingToActive() {
    BaseT pending_value_before_push = Current(false);
    BaseT active_value_before_push = Current(true);

    active_base_ = pending_base_;
    active_delta_ = PendingDelta();
    reflected_delta_in_pending_tree_ = T::IdentityDelta();
    clobber_active_value_ = false;

    BaseT current_active_value = Current(true);
    return pending_value_before_push != current_active_value ||
           active_value_before_push != current_active_value;
  }

  void AbortCommit(bool next_bmf, bool main_frame_applied_deltas) {
    // Finish processing the delta that was sent to the main thread, and reset
    // the corresponding the delta_in_main_tree_ variable. If
    // main_frame_applied_deltas is true, we send the delta on to the active
    // tree just as would happen for a successful commit. Otherwise, we treat
    // the delta as never having been sent to the main thread and just drop it.
    if (next_bmf) {
      // The previous main frame has not yet run commit; the aborted main frame
      // corresponds to the delta in the "next" slot (if any).  In this case, if
      // the main thread processed the delta from this aborted commit we can
      // simply add the delta to reflected_delta_in_main_tree_.
      if (main_frame_applied_deltas) {
        reflected_delta_in_main_tree_ = T::CombineDeltas(
            reflected_delta_in_main_tree_, next_reflected_delta_in_main_tree_);
      }
      next_reflected_delta_in_main_tree_ = T::IdentityDelta();
    } else {
      // There is no "next" main frame, this abort was for the primary.
      if (main_frame_applied_deltas) {
        DeltaT delta = reflected_delta_in_main_tree_;
        // This simulates the consequences of the sent value getting committed
        // and activated.
        pending_base_ = T::ApplyDelta(pending_base_, delta);
        active_base_ = T::ApplyDelta(active_base_, delta);
        active_delta_ = T::DeltaBetweenDeltas(active_delta_, delta);
      }
      reflected_delta_in_main_tree_ = T::IdentityDelta();
    }
  }

  // Values sent to the main thread and not yet resolved in the pending or
  // active tree.
  DeltaT reflected_delta_in_main_tree() const {
    return reflected_delta_in_main_tree_;
  }
  DeltaT next_reflected_delta_in_main_tree() const {
    return next_reflected_delta_in_main_tree_;
  }
  // Values as last pushed to the pending or active tree respectively, with no
  // impl-thread delta applied.
  BaseT PendingBase() const { return pending_base_; }
  BaseT ActiveBase() const { return active_base_; }

  // The new delta we would use if we decide to activate now.  This delta
  // excludes the amount that we know is reflected in the pending tree.
  DeltaT PendingDelta() const {
    if (clobber_active_value_)
      return T::IdentityDelta();

    return T::DeltaBetweenDeltas(active_delta_,
                                 reflected_delta_in_pending_tree_);
  }

  DeltaT UnsentDelta() const {
    return T::DeltaBetweenDeltas(PendingDelta(), reflected_delta_in_main_tree_);
  }

  void set_clobber_active_value() { clobber_active_value_ = true; }

 private:
  friend class base::RefCounted<SyncedProperty<T>>;
  ~SyncedProperty() = default;

  // Value last committed to the pending tree.
  BaseT pending_base_ = T::IdentityBase();
  // Value last committed to the active tree on the last activation.
  BaseT active_base_ = T::IdentityBase();
  // The difference between |active_base_| and the user-perceived value.
  DeltaT active_delta_ = T::IdentityDelta();
  // A value sent to the main thread on a BeginMainFrame, but not yet applied to
  // the resulting pending tree.
  DeltaT reflected_delta_in_main_tree_ = T::IdentityDelta();
  // A value sent to the main thread on a BeginMainFrame at a time when
  // the previous BeginMainFrame is in the ready-to-commit state.
  DeltaT next_reflected_delta_in_main_tree_ = T::IdentityDelta();
  // The value that was sent to the main thread for BeginMainFrame for the
  // current pending tree. This is always identity outside of the
  // BeginMainFrame to activation interval.
  DeltaT reflected_delta_in_pending_tree_ = T::IdentityDelta();
  // When true the pending delta is always identity so that it does not change
  // and will clobber the active value on push.
  bool clobber_active_value_ = false;
};

// SyncedProperty's delta-based conflict resolution logic makes sense for any
// mathematical group.  In practice, there are two that are useful:
// 1. Numbers/classes with addition and subtraction operations, and
//    identity = constructor() (like gfx::Vector2dF for scroll offset and
//    scroll delta)
// 2. Real numbers with multiplication and division operations, and
//    identity = 1 (like page scale)

template <typename BaseT, typename DeltaT = BaseT>
class AdditionGroup {
 public:
  using BaseType = BaseT;
  using DeltaType = DeltaT;
  static constexpr BaseT IdentityBase() { return BaseT(); }
  static constexpr DeltaT IdentityDelta() { return DeltaT(); }
  static BaseT ApplyDelta(BaseT v, DeltaT delta) { return v + delta; }
  static DeltaT DeltaBetweenBases(BaseT v1, BaseT v2) { return v1 - v2; }
  static DeltaT DeltaBetweenDeltas(DeltaT d1, DeltaT d2) { return d1 - d2; }
  static DeltaT CombineDeltas(DeltaT d1, DeltaT d2) { return d1 + d2; }
};

class ScaleGroup {
 public:
  using BaseType = float;
  using DeltaType = float;
  static constexpr float IdentityBase() { return 1.f; }
  static constexpr float IdentityDelta() { return 1.f; }
  static float ApplyDelta(float v, float delta) { return v * delta; }
  static float DeltaBetweenBases(float v1, float v2) { return v1 / v2; }
  static float DeltaBetweenDeltas(float d1, float d2) { return d1 / d2; }
  static float CombineDeltas(float d1, float d2) { return d1 * d2; }
};

}  // namespace cc

#endif  // CC_BASE_SYNCED_PROPERTY_H_
