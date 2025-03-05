// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACCESSIBILITY_TREE_FIXING_AX_TREE_FIXING_SERVICES_ROUTER_H_
#define CHROME_BROWSER_ACCESSIBILITY_TREE_FIXING_AX_TREE_FIXING_SERVICES_ROUTER_H_

#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/browser_context.h"

namespace tree_fixing {

// This class is owned by the BrowserContext and should be created the first
// time accessibility is enabled. This class handles the communication between
// the browser process and any downstream services used to fix the AXTree, such
// as: the optimization guide, Screen2x, Aratea, etc.
class AXTreeFixingServicesRouter : public KeyedService {
 public:
  explicit AXTreeFixingServicesRouter(content::BrowserContext* context);
  AXTreeFixingServicesRouter(const AXTreeFixingServicesRouter&) = delete;
  AXTreeFixingServicesRouter& operator=(const AXTreeFixingServicesRouter&) =
      delete;
  ~AXTreeFixingServicesRouter() override;

  // TODO(mschillaci): Stubbed. Fill in with mojom bindings.
};

}  // namespace tree_fixing

#endif  // CHROME_BROWSER_ACCESSIBILITY_TREE_FIXING_AX_TREE_FIXING_SERVICES_ROUTER_H_
