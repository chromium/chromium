// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_MEMORY_COORDINATOR_MULTI_MEMORY_CONSUMER_H_
#define BASE_MEMORY_COORDINATOR_MULTI_MEMORY_CONSUMER_H_

#include <memory>
#include <optional>
#include <string_view>

#include "base/base_export.h"
#include "base/containers/flat_map.h"
#include "base/containers/span.h"
#include "base/memory_coordinator/memory_consumer.h"
#include "base/memory_coordinator/traits.h"
#include "base/sequence_checker.h"

namespace base {

namespace internal {
class ForwardingMemoryConsumer;
}

// MultiMemoryConsumer is the interface that clients must implement to receive
// notifications for multiple memory interventions.
class BASE_EXPORT MultiMemoryConsumer {
 public:
  virtual ~MultiMemoryConsumer() = default;

 protected:
  friend class MultiMemoryConsumerRegistration;
  friend class AsyncMultiMemoryConsumerRegistration;
  friend class internal::ForwardingMemoryConsumer;

  // Invoked when memory above the current limit should be freed for the
  // intervention with `name`.
  virtual void OnReleaseMemory(std::string_view name) = 0;

  // Invoked when the memory limit for the intervention with `name` is updated.
  virtual void OnUpdateMemoryLimit(std::string_view name, int limit) = 0;
};

// MultiMemoryConsumerRegistration handles synchronous registration of multiple
// memory interventions.
class BASE_EXPORT MultiMemoryConsumerRegistration {
 public:
  struct Intervention {
    std::string_view name;
    std::optional<MemoryConsumerTraits> traits = std::nullopt;
  };

  using CheckUnregister = MemoryConsumerRegistration::CheckUnregister;
  using CheckRegistryExists = MemoryConsumerRegistration::CheckRegistryExists;

  MultiMemoryConsumerRegistration(
      base::span<const Intervention> interventions,
      MultiMemoryConsumer* consumer,
      CheckUnregister check_unregister = CheckUnregister::kEnabled,
      CheckRegistryExists check_registry_exists =
          CheckRegistryExists::kEnabled);

  MultiMemoryConsumerRegistration(const MultiMemoryConsumerRegistration&) =
      delete;
  MultiMemoryConsumerRegistration& operator=(
      const MultiMemoryConsumerRegistration&) = delete;

  ~MultiMemoryConsumerRegistration();

  // Returns the current memory limit for the specified intervention name.
  int GetMemoryLimit(std::string_view name) const;

  // Returns the current memory limit ratio for the specified intervention name.
  double GetMemoryLimitRatio(std::string_view name) const;

 private:
  class HelperConsumer;
  base::flat_map<std::string, std::unique_ptr<HelperConsumer>> consumers_;
  SEQUENCE_CHECKER(sequence_checker_);
};

class BASE_EXPORT AsyncMultiMemoryConsumerRegistration {
 public:
  using Intervention = MultiMemoryConsumerRegistration::Intervention;
  using CheckUnregister = MultiMemoryConsumerRegistration::CheckUnregister;
  using CheckRegistryExists =
      MultiMemoryConsumerRegistration::CheckRegistryExists;

  AsyncMultiMemoryConsumerRegistration(
      base::span<const Intervention> interventions,
      MultiMemoryConsumer* consumer,
      CheckUnregister check_unregister = CheckUnregister::kEnabled,
      CheckRegistryExists check_registry_exists =
          CheckRegistryExists::kEnabled);

  AsyncMultiMemoryConsumerRegistration(
      const AsyncMultiMemoryConsumerRegistration&) = delete;
  AsyncMultiMemoryConsumerRegistration& operator=(
      const AsyncMultiMemoryConsumerRegistration&) = delete;

  ~AsyncMultiMemoryConsumerRegistration();

  // Returns the current memory limit for the specified intervention name.
  int GetMemoryLimit(std::string_view name) const;

  // Returns the current memory limit ratio for the specified intervention name.
  double GetMemoryLimitRatio(std::string_view name) const;

 private:
  class HelperConsumer;
  base::flat_map<std::string, std::unique_ptr<HelperConsumer>> consumers_;
  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace base

#endif  // BASE_MEMORY_COORDINATOR_MULTI_MEMORY_CONSUMER_H_
