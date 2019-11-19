// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_SCHEMA_REGISTRY_SERVICE_H_
#define CHROME_BROWSER_POLICY_SCHEMA_REGISTRY_SERVICE_H_

#include <memory>

#include "base/macros.h"

namespace policy {

class CombinedSchemaRegistry;
class Schema;
class SchemaRegistry;

// A KeyedService associated with a Profile that contains a SchemaRegistry.
class SchemaRegistryService {
 public:
  // This |registry| will initially contain only the |chrome_schema|, if
  // it's valid. The optional |global_registry| must outlive this, and will
  // track |registry|.
  SchemaRegistryService(std::unique_ptr<SchemaRegistry> registry,
                        const Schema& chrome_schema,
                        CombinedSchemaRegistry* global_registry);
  ~SchemaRegistryService();

  SchemaRegistry* registry() const { return registry_.get(); }

 private:
  std::unique_ptr<SchemaRegistry> registry_;

  DISALLOW_COPY_AND_ASSIGN(SchemaRegistryService);
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_SCHEMA_REGISTRY_SERVICE_H_
