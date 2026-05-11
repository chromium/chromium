// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory_coordinator/multi_memory_consumer.h"

#include "base/check.h"
#include "base/memory/raw_ptr.h"
#include "base/memory_coordinator/async_memory_consumer_registration.h"

namespace base {

namespace internal {

class ForwardingMemoryConsumer : public MemoryConsumer {
 public:
  ForwardingMemoryConsumer(MultiMemoryConsumer* parent, std::string_view name)
      : parent_(parent), name_(std::string(name)) {}
  ~ForwardingMemoryConsumer() override = default;

  const std::string& name() const { return name_; }

  // MemoryConsumer implementation:
  void OnReleaseMemory() override { parent_->OnReleaseMemory(name_); }
  void OnUpdateMemoryLimit() override {
    parent_->OnUpdateMemoryLimit(name_, memory_limit());
  }

 private:
  raw_ptr<MultiMemoryConsumer> parent_;
  std::string name_;
};

}  // namespace internal

// MultiMemoryConsumerRegistration::HelperConsumer -----------------------------

class MultiMemoryConsumerRegistration::HelperConsumer
    : public internal::ForwardingMemoryConsumer {
 public:
  HelperConsumer(MultiMemoryConsumer* parent,
                 std::string_view name,
                 std::optional<MemoryConsumerTraits> traits,
                 CheckUnregister check_unregister,
                 CheckRegistryExists check_registry_exists)
      : internal::ForwardingMemoryConsumer(parent, name),
        registration_(name,
                      traits,
                      this,
                      check_unregister,
                      check_registry_exists) {}

  ~HelperConsumer() override = default;

 private:
  MemoryConsumerRegistration registration_;
};

// MultiMemoryConsumerRegistration ---------------------------------------------

MultiMemoryConsumerRegistration::MultiMemoryConsumerRegistration(
    base::span<const Intervention> interventions,
    MultiMemoryConsumer* consumer,
    CheckUnregister check_unregister,
    CheckRegistryExists check_registry_exists) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(consumer);
  for (const auto& intervention : interventions) {
    auto [_, inserted] =
        consumers_.emplace(std::string(intervention.name),
                           std::make_unique<HelperConsumer>(
                               consumer, intervention.name, intervention.traits,
                               check_unregister, check_registry_exists));
    CHECK(inserted);
  }
}

MultiMemoryConsumerRegistration::~MultiMemoryConsumerRegistration() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

int MultiMemoryConsumerRegistration::GetMemoryLimit(
    std::string_view name) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto it = consumers_.find(name);
  CHECK(it != consumers_.end());
  return it->second->memory_limit();
}

double MultiMemoryConsumerRegistration::GetMemoryLimitRatio(
    std::string_view name) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto it = consumers_.find(name);
  CHECK(it != consumers_.end());
  return it->second->memory_limit_ratio();
}

// AsyncMultiMemoryConsumerRegistration::HelperConsumer ------------------------

class AsyncMultiMemoryConsumerRegistration::HelperConsumer
    : public internal::ForwardingMemoryConsumer {
 public:
  HelperConsumer(MultiMemoryConsumer* parent,
                 std::string_view name,
                 std::optional<MemoryConsumerTraits> traits,
                 CheckUnregister check_unregister,
                 CheckRegistryExists check_registry_exists)
      : internal::ForwardingMemoryConsumer(parent, name),
        registration_(name,
                      traits,
                      this,
                      check_unregister,
                      check_registry_exists) {}

  ~HelperConsumer() override = default;

 private:
  AsyncMemoryConsumerRegistration registration_;
};

// AsyncMultiMemoryConsumerRegistration ----------------------------------------

AsyncMultiMemoryConsumerRegistration::AsyncMultiMemoryConsumerRegistration(
    base::span<const Intervention> interventions,
    MultiMemoryConsumer* consumer,
    CheckUnregister check_unregister,
    CheckRegistryExists check_registry_exists) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(consumer);
  for (const auto& intervention : interventions) {
    auto [_, inserted] =
        consumers_.emplace(std::string(intervention.name),
                           std::make_unique<HelperConsumer>(
                               consumer, intervention.name, intervention.traits,
                               check_unregister, check_registry_exists));
    CHECK(inserted);
  }
}

AsyncMultiMemoryConsumerRegistration::~AsyncMultiMemoryConsumerRegistration() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

int AsyncMultiMemoryConsumerRegistration::GetMemoryLimit(
    std::string_view name) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto it = consumers_.find(name);
  CHECK(it != consumers_.end());
  return it->second->memory_limit();
}

double AsyncMultiMemoryConsumerRegistration::GetMemoryLimitRatio(
    std::string_view name) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto it = consumers_.find(name);
  CHECK(it != consumers_.end());
  return it->second->memory_limit_ratio();
}

}  // namespace base
