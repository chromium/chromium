// Copyright (c) 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ALLOCATOR_PARTITION_ALLOCATOR_STARSCAN_SNAPSHOT_H_
#define BASE_ALLOCATOR_PARTITION_ALLOCATOR_STARSCAN_SNAPSHOT_H_

#include <memory>

#include "base/allocator/partition_allocator/starscan/pcscan_internal.h"
#include "base/allocator/partition_allocator/starscan/raceful_worklist.h"

namespace base {
namespace internal {

class PCScanInternal;

class StarScanSnapshot final : public AllocatedOnPCScanMetadataPartition {
 public:
  using SuperPageBase = uintptr_t;
  using SuperPagesWorklist = RacefulWorklist<SuperPageBase>;

  class ViewBase {
   public:
    template <typename Function>
    void VisitConcurrently(Function);

    template <typename Function>
    void VisitNonConcurrently(Function);

   protected:
    explicit ViewBase(SuperPagesWorklist& worklist) : worklist_(worklist) {}

   private:
    SuperPagesWorklist& worklist_;
  };

  class ClearingView : public ViewBase {
   public:
    inline explicit ClearingView(StarScanSnapshot& snapshot);
  };
  class ScanningView : public ViewBase {
   public:
    inline explicit ScanningView(StarScanSnapshot& snapshot);
  };
  class SweepingView : public ViewBase {
   public:
    inline explicit SweepingView(StarScanSnapshot& snapshot);
  };
  class UnprotectingView : public ViewBase {
   public:
    inline explicit UnprotectingView(StarScanSnapshot& snapshot);
  };

  static std::unique_ptr<StarScanSnapshot> Create(const PCScanInternal&);

  StarScanSnapshot(const StarScanSnapshot&) = delete;
  StarScanSnapshot& operator=(const StarScanSnapshot&) = delete;

  ~StarScanSnapshot();

 private:
  explicit StarScanSnapshot(const PCScanInternal&);

  SuperPagesWorklist clear_worklist_;
  SuperPagesWorklist scan_worklist_;
  SuperPagesWorklist unprotect_worklist_;
  SuperPagesWorklist sweep_worklist_;
};

template <typename Function>
void StarScanSnapshot::ViewBase::VisitConcurrently(Function f) {
  SuperPagesWorklist::RandomizedView view(worklist_);
  view.Visit(std::move(f));
}

template <typename Function>
void StarScanSnapshot::ViewBase::VisitNonConcurrently(Function f) {
  worklist_.VisitNonConcurrently(std::move(f));
}

StarScanSnapshot::ClearingView::ClearingView(StarScanSnapshot& snapshot)
    : StarScanSnapshot::ViewBase(snapshot.clear_worklist_) {}

StarScanSnapshot::ScanningView::ScanningView(StarScanSnapshot& snapshot)
    : StarScanSnapshot::ViewBase(snapshot.scan_worklist_) {}

StarScanSnapshot::SweepingView::SweepingView(StarScanSnapshot& snapshot)
    : StarScanSnapshot::ViewBase(snapshot.sweep_worklist_) {}

StarScanSnapshot::UnprotectingView::UnprotectingView(StarScanSnapshot& snapshot)
    : StarScanSnapshot::ViewBase(snapshot.unprotect_worklist_) {}

}  // namespace internal
}  // namespace base

#endif  // BASE_ALLOCATOR_PARTITION_ALLOCATOR_STARSCAN_SNAPSHOT_H_
