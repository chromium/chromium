// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory_coordinator/memory_consumer_registry.h"

#include "base/auto_reset.h"
#include "base/check.h"
#include "base/check_op.h"
#include "base/feature_list.h"
#include "base/hash/hash.h"
#include "base/memory_coordinator/memory_coordinator_features.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/memory_coordinator/memory_consumer_android.h"
#endif

namespace base {

namespace {

MemoryConsumerRegistry* g_memory_consumer_registry = nullptr;

}  // namespace

void MemoryConsumerRegistry::NotifyReleaseMemory(MemoryConsumer* consumer) {
  CHECK(consumer);
  consumer->ReleaseMemory();
}

void MemoryConsumerRegistry::NotifyUpdateMemoryLimit(MemoryConsumer* consumer,
                                                     MemoryLimit memory_limit) {
  CHECK(consumer);
  consumer->UpdateMemoryLimit(memory_limit);
}

// static
bool MemoryConsumerRegistry::Exists() {
  return g_memory_consumer_registry;
}

// static
MemoryConsumerRegistry& MemoryConsumerRegistry::Get() {
  CHECK(g_memory_consumer_registry);
  return *g_memory_consumer_registry;
}

// static
MemoryConsumerRegistry* MemoryConsumerRegistry::MaybeGet() {
  return g_memory_consumer_registry;
}

// static
void MemoryConsumerRegistry::Set(MemoryConsumerRegistry* instance) {
  CHECK_NE(bool(g_memory_consumer_registry), bool(instance));
  g_memory_consumer_registry = instance;
#if BUILDFLAG(IS_ANDROID)
  if (g_memory_consumer_registry) {
    android::FlushPendingMemoryConsumerRegistrations();
  } else {
    android::OnMemoryConsumerRegistryDestroyed();
  }
#endif
}

MemoryConsumerRegistry::MemoryConsumerRegistry() = default;

MemoryConsumerRegistry::~MemoryConsumerRegistry() {
  // Checks that implementations of this class correctly call
  // NotifyDestruction().
  CHECK(destruction_observers_notified_);

  // Checks that implementations of the destruction observer interface correctly
  // unregister themselves.
  CHECK(destruction_observers_.empty());
}

void MemoryConsumerRegistry::AddMemoryConsumer(std::string_view consumer_name,
                                               MemoryConsumerTraits traits,
                                               MemoryConsumer* consumer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (traits.consumer_type == MemoryConsumerTraits::ConsumerType::kPassive) {
    CHECK(consumer->IsPassive())
        << "Active MemoryConsumer registered with Passive traits: "
        << consumer_name;
  }
  // TODO(crbug.com/489671163): Re-enable the check that passive consumers
  // don't register with active traits once all clients have been migrated.

  uint32_t consumer_id = PersistentHash(consumer_name);

  if (!disabled_consumer_ids_) {
    // Remember the consumers that registered before the overrides are known,
    // so that OnFeatureListInitialized() can remove the disabled ones.
    early_consumers_.emplace_back(consumer_id, consumer);
  } else if (disabled_consumer_ids_->contains(consumer_id)) {
    // Disabled consumers are never forwarded to the implementation.
    return;
  }

  // Suppress the consumer's callbacks for the duration of the registration,
  // which typically happens during its construction. Implementations may
  // still push an initial limit to the consumer synchronously; only
  // `memory_limit()` is updated in that case.
  AutoReset<bool> is_registering(&consumer->is_registering_, true);
  OnMemoryConsumerAdded(consumer_id, consumer_name, traits, consumer);
}

void MemoryConsumerRegistry::RemoveMemoryConsumer(
    std::string_view consumer_name,
    MemoryConsumer* consumer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  uint32_t consumer_id = PersistentHash(consumer_name);

  if (!disabled_consumer_ids_) {
    size_t removed = std::erase_if(early_consumers_, [&](const auto& entry) {
      return entry.first == consumer_id && entry.second == consumer;
    });
    CHECK_EQ(removed, 1u);
  } else if (disabled_consumer_ids_->contains(consumer_id)) {
    // Disabled consumers were either never forwarded to the implementation, or
    // already removed from it by OnFeatureListInitialized().
    return;
  }

  OnMemoryConsumerRemoved(consumer_id, consumer);
}

void MemoryConsumerRegistry::AddDestructionObserver(
    PassKey<MemoryConsumerRegistration>,
    MemoryConsumerRegistryDestructionObserver* observer) {
  CHECK(!is_notifying_destruction_);
  bool inserted = destruction_observers_.insert(observer).second;
  CHECK(inserted);
}

void MemoryConsumerRegistry::RemoveDestructionObserver(
    PassKey<MemoryConsumerRegistration>,
    MemoryConsumerRegistryDestructionObserver* observer) {
  size_t removed = destruction_observers_.erase(observer);
  CHECK_EQ(removed, 1u);
}

void MemoryConsumerRegistry::OnFeatureListInitialized() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(FeatureList::GetInstance());
  CHECK(!disabled_consumer_ids_);

  disabled_consumer_ids_.emplace();
  for (const auto& [consumer_name, mode] :
       internal::ParseMemoryConsumerModeOverrides()) {
    if (mode == internal::MemoryConsumerMode::kDisabled) {
      disabled_consumer_ids_->insert(PersistentHash(consumer_name));
    }
  }

  // Remove the disabled consumers that registered early from the
  // implementation, so that all instances of a consumer in this process behave
  // consistently.
  for (const auto& [consumer_id, consumer] :
       std::exchange(early_consumers_, {})) {
    if (!disabled_consumer_ids_->contains(consumer_id)) {
      continue;
    }
    // The implementation must not have notified the consumer yet.
    CHECK(consumer->memory_limit() == MemoryLimit::Default());
    OnMemoryConsumerRemoved(consumer_id, consumer);
  }
}

void MemoryConsumerRegistry::NotifyDestruction() {
  AutoReset<bool> is_notifying_destruction(&is_notifying_destruction_, true);
  for (auto it = destruction_observers_.begin();
       it != destruction_observers_.end();) {
    // Advance the iterator before notifying, as the observer removes itself.
    // Erasing from a flat_hash_set never rehashes, so `it` remains valid.
    MemoryConsumerRegistryDestructionObserver* observer = *it++;
    const size_t size_before = destruction_observers_.size();
    observer->OnBeforeMemoryConsumerRegistryDestroyed();
    // Each observer must remove exactly itself and nothing else, otherwise `it`
    // could be invalidated.
    CHECK_EQ(destruction_observers_.size(), size_before - 1);
  }
  destruction_observers_notified_ = true;
}

}  // namespace base
