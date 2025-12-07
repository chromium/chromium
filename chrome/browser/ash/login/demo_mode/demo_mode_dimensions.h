// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_DEMO_MODE_DEMO_MODE_DIMENSIONS_H_
#define CHROME_BROWSER_ASH_LOGIN_DEMO_MODE_DEMO_MODE_DIMENSIONS_H_

#include <string>

#include "base/version.h"
#include "components/policy/proto/device_management_backend.pb.h"

namespace ash {
namespace demo_mode {

// User-selected country, returned as two-letter country code.
std::string Country();

// User-entered (canonicalized) name of the retailer that the demo device is
// running in.
std::string RetailerName();

// User-entered number identifying the store that a demo device is running in.
std::string StoreNumber();

// Whether the demo device falls under the Cloud Gaming device branding
// category.
bool IsCloudGamingDevice();

// Whether the demo device has additional features enabled by the feature
// management module.
bool IsFeatureAwareDevice();

// The demo mode app component version.
base::Version AppVersion();

// The demo mode resources component version.
base::Version ResourcesVersion();

// Construct the full version string. It has the format
// R{Chrome Browser Milestone}-{Platform Version}_{Channel}, e.g.
// R127-15919.0.0_stable-channel. Fall back to 0 for versions or
// "unknown-channel" for the channel if corresponding info is not available.
std::string GetChromeOSVersionString();

// The board of the demo device.
std::string Board();

// The model of the demo device. It returns an empty string if the model info is
// not available.
std::string_view Model();

// The locale of the demo session.
std::string Locale();

// Builds and returns a DemoModeDimensions proto from the individual dimension
// values
enterprise_management::DemoModeDimensions GetDemoModeDimensions();
}  // namespace demo_mode
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_DEMO_MODE_DEMO_MODE_DIMENSIONS_H_
