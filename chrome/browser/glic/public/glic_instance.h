// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_PUBLIC_GLIC_INSTANCE_H_
#define CHROME_BROWSER_GLIC_PUBLIC_GLIC_INSTANCE_H_

#include "base/uuid.h"

namespace glic {

class Host;

// A type alias for the Glic instance identifier.
using InstanceId = base::Uuid;

namespace glic_instance_internal {

// Interface for UI methods that can be called on the instance.
class UIDelegate {
 public:
  virtual ~UIDelegate() = default;

  virtual bool IsShowing() const = 0;
};

}  // namespace glic_instance_internal

// Public interface for one instance of the glic web client.
class GlicInstance : public glic_instance_internal::UIDelegate {
 public:
  // Exposes the UIDelegate interface on GlicInstance::UIDelegate.
  using UIDelegate = glic_instance_internal::UIDelegate;

  // Get this instance's Host which manages the chrome://glic WebContents.
  virtual Host& host() = 0;

  // Get this instance's unique identifier.
  virtual const InstanceId& id() const = 0;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_PUBLIC_GLIC_INSTANCE_H_
