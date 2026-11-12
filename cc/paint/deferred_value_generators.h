// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_PAINT_DEFERRED_VALUE_GENERATORS_H_
#define CC_PAINT_DEFERRED_VALUE_GENERATORS_H_

#include <cstddef>
#include <memory>
#include <optional>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "cc/paint/deferred_property_key.h"
#include "cc/paint/paint_export.h"

namespace cc {

class DeferredValueGenerator;
struct DeferredValueMap;

// Maps each animated property on an element to its own value generator so keys
// sharing a curve track progress independently.
class CC_PAINT_EXPORT DeferredValueGenerators {
 public:
  DeferredValueGenerators();
  DeferredValueGenerators(const DeferredValueGenerators&) = delete;
  DeferredValueGenerators& operator=(const DeferredValueGenerators&) = delete;
  ~DeferredValueGenerators();

  void Set(const DeferredPropertyKey& key,
           std::unique_ptr<DeferredValueGenerator> generator);
  void Erase(const DeferredPropertyKey& key);
  void Clear();

  bool empty() const { return generators_.empty(); }
  size_t size() const { return generators_.size(); }
  bool Contains(const DeferredPropertyKey& key) const {
    return generators_.contains(key);
  }
  const DeferredValueGenerator* Find(const DeferredPropertyKey& key) const;

  // Updates or clears the progress of the generator for `key`, if present.
  // Returns true if the progress changed.
  bool SetProgress(const DeferredPropertyKey& key,
                   std::optional<float> progress);

  // Returns true if the generators or their progress changed since the last
  // `UpdateValues()`.
  [[nodiscard]] bool NeedsUpdate() const { return needs_update_; }

  // Samples the generators and updates `values` in place. Keys without a value
  // are left out so raster uses paint's fallback. Returns the keys whose value
  // was added, changed, or removed.
  [[nodiscard]] base::flat_set<DeferredPropertyKey> UpdateValues(
      DeferredValueMap& values);

  // Updates this map to match the generators in `source` that can run on the
  // impl thread, keeping unchanged ones so their progress and sample carry
  // over. Returns true if anything changed. Must only be called on maps filled
  // by `SyncFrom()`.
  bool SyncFrom(const DeferredValueGenerators& source);

 private:
  base::flat_map<DeferredPropertyKey, std::unique_ptr<DeferredValueGenerator>>
      generators_;
  bool needs_update_ = false;
};

}  // namespace cc

#endif  // CC_PAINT_DEFERRED_VALUE_GENERATORS_H_
