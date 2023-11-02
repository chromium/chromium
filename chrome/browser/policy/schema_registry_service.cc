// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/schema_registry_service.h"

#include <utility>

#include "components/policy/core/common/policy_namespace.h"
#include "components/policy/core/common/schema.h"
#include "components/policy/core/common/schema_registry.h"

namespace policy {

SchemaRegistryService::SchemaRegistryService(
    std::unique_ptr<SchemaRegistry> registry,
    const Schema& chrome_schema,
    CombinedSchemaRegistry* global_registry)
    : registry_(std::move(registry)) {
  if (chrome_schema.valid()) {
    registry_->RegisterComponent(PolicyNamespace(POLICY_DOMAIN_CHROME, ""),
                                 chrome_schema);
  }
  registry_->SetDomainReady(POLICY_DOMAIN_CHROME);
  if (global_registry)
    global_registry->Track(registry_.get());
}

SchemaRegistryService::~SchemaRegistryService() {}

}  // namespace policy
