// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/paint/deferred_value_generators.h"

#include <algorithm>
#include <functional>
#include <ranges>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/containers/flat_tree.h"
#include "base/containers/map_util.h"
#include "cc/paint/deferred_value_map.h"
#include "cc/paint/deferred_values.h"

namespace cc {

namespace {

// Updates `values` from the generators of type `Generator`, and adds the keys
// whose value was added, changed, or removed to `changed`.
template <typename Generator, typename Value>
void UpdateTypedValues(
    const base::flat_map<DeferredPropertyKey,
                         std::unique_ptr<DeferredValueGenerator>>& generators,
    Value (Generator::*get_value)() const,
    base::flat_map<DeferredPropertyKey, Value>& values,
    std::vector<DeferredPropertyKey>& changed) {
  auto erase_range = [&](auto first, auto last) {
    // Record every key in `[first, last)` as changed before erasing it.
    for (const auto& [erased_key, _] : std::ranges::subrange(first, last)) {
      changed.push_back(erased_key);
    }
    return values.erase(first, last);
  };

  // Both maps are sorted by key, so update `values` in one pass.
  auto it = values.begin();
  for (const auto& [key, generator] : generators) {
    const Generator* typed = Generator::From(generator.get());
    if (!typed) {
      continue;
    }
    generator->RecomputeValue();
    // Skip unsampled generators so raster falls back to the painted value.
    if (!generator->has_value()) {
      continue;
    }
    // Entries sorted before `key` lost their value, so erase them.
    auto next = std::ranges::find_if(it, values.end(), [&](const auto& entry) {
      return entry.first >= key;
    });
    it = erase_range(it, next);

    const Value value = std::invoke(get_value, typed);
    // `it` is now at `key` or past it.
    const bool found = it != values.end() && it->first == key;
    if (!found) {
      // `key` gained a value, so insert it before `it`.
      it = values.emplace_hint(it, key, value);
      changed.push_back(key);
    } else if (it->second != value) {
      // `key`'s value changed, so overwrite it in place.
      it->second = value;
      changed.push_back(key);
    }
    ++it;
  }
  // Any entries left after the last generator also lost their value.
  erase_range(it, values.end());
}

}  // namespace

DeferredValueGenerators::DeferredValueGenerators() = default;
DeferredValueGenerators::~DeferredValueGenerators() = default;

void DeferredValueGenerators::Set(
    const DeferredPropertyKey& key,
    std::unique_ptr<DeferredValueGenerator> generator) {
  CHECK(generator);
  generators_.insert_or_assign(key, std::move(generator));
  needs_update_ = true;
}

void DeferredValueGenerators::Erase(const DeferredPropertyKey& key) {
  needs_update_ |= generators_.erase(key) > 0;
}

void DeferredValueGenerators::Clear() {
  needs_update_ |= !generators_.empty();
  generators_.clear();
}

const DeferredValueGenerator* DeferredValueGenerators::Find(
    const DeferredPropertyKey& key) const {
  return base::FindPtrOrNull(generators_, key);
}

bool DeferredValueGenerators::SetProgress(const DeferredPropertyKey& key,
                                          std::optional<float> progress) {
  DeferredValueGenerator* generator = base::FindPtrOrNull(generators_, key);
  const bool changed = generator && generator->SetProgress(progress);
  needs_update_ |= changed;
  return changed;
}

base::flat_set<DeferredPropertyKey> DeferredValueGenerators::UpdateValues(
    DeferredValueMap& values) {
  needs_update_ = false;
  std::vector<DeferredPropertyKey> changed;
  // Unpack `values` so adding a member fails to compile until updated here.
  auto& [colors] = values;
  UpdateTypedValues(generators_, &DeferredColorValueGenerator::GetColor, colors,
                    changed);
  // Colors are the only value type for now, so `changed` is already sorted.
  return {base::sorted_unique, std::move(changed)};
}

bool DeferredValueGenerators::SyncFrom(const DeferredValueGenerators& source) {
  bool changed = false;
  auto erase_range = [&](auto first, auto last) {
    if (first != last) {
      changed = true;
    }
    return generators_.erase(first, last);
  };

  // Both maps are sorted by key, so sync `generators_` in one pass.
  auto it = generators_.begin();
  for (const auto& [key, generator] : source.generators_) {
    // Skip generators that stay on the main thread.
    if (!generator->CanRunOnImplThread()) {
      continue;
    }
    // Generators sorted before `key` are no longer synced, so erase them.
    auto next = std::ranges::find_if(
        it, generators_.end(),
        [&](const auto& entry) { return entry.first >= key; });
    it = erase_range(it, next);

    // `it` is now at `key` or past it. A generator whose source matches stays
    // as it is so its progress and sample carry over.
    const bool found = it != generators_.end() && it->first == key;
    if (!found) {
      // `key` is new, so insert a fresh unsampled generator before `it`.
      it = generators_.emplace_hint(it, key, generator->CreateWithSameSource());
      changed = true;
    } else if (!it->second->HasSameSource(*generator)) {
      // The source changed, so replace it with a fresh unsampled generator.
      it->second = generator->CreateWithSameSource();
      changed = true;
    }
    // Every kept generator came from an earlier `SyncFrom()`, so it can run on
    // the impl thread too.
    DCHECK(it->second->CanRunOnImplThread());
    ++it;
  }
  // Any generators left after the last synced key are no longer synced either.
  erase_range(it, generators_.end());
  needs_update_ |= changed;
  return changed;
}

}  // namespace cc
