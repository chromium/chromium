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

// Public interface for one instance of the glic web client.
class GlicInstance {
 public:
  virtual ~GlicInstance() = default;

  // Get this instance's Host which manages the chrome://glic WebContents.
  virtual Host& host() = 0;

  // Get this instance's unique identifier.
  virtual const InstanceId& id() const = 0;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_PUBLIC_GLIC_INSTANCE_H_
