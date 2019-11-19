// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_SCHEMA_REGISTRY_SERVICE_BUILDER_H_
#define CHROME_BROWSER_POLICY_SCHEMA_REGISTRY_SERVICE_BUILDER_H_

#include <memory>

#include "base/macros.h"

namespace content {
class BrowserContext;
}

namespace policy {

class CombinedSchemaRegistry;
class Schema;
class SchemaRegistryService;
class SchemaRegistry;

// Creates SchemaRegistryService for BrowserContexts.
std::unique_ptr<SchemaRegistryService> BuildSchemaRegistryServiceForProfile(
    content::BrowserContext* context,
    const Schema& chrome_schema,
    CombinedSchemaRegistry* global_registry);

// Creates SchemaRegistryService without BrowserContext.
std::unique_ptr<SchemaRegistryService> BuildSchemaRegistryService(
    std::unique_ptr<SchemaRegistry> registry,
    const Schema& chrome_schema,
    CombinedSchemaRegistry* global_registry);

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_SCHEMA_REGISTRY_SERVICE_BUILDER_H_
