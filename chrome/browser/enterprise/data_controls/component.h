// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_DATA_CONTROLS_COMPONENT_H_
#define CHROME_BROWSER_ENTERPRISE_DATA_CONTROLS_COMPONENT_H_

namespace data_controls {

// A representation of destinations to which sharing confidential data is
// restricted by DataLeakPreventionRulesList policy. This is only applicable to
// ChromeOS as other platforms don't have the same visibility into applications
// directly outside of Chrome.
enum class Component {
  kUnknownComponent,
  kArc,       // ARC++ as a Guest OS.
  kCrostini,  // Crostini as a Guest OS.
  kPluginVm,  // Plugin VM (Parallels/Windows) as a Guest OS.
  kUsb,       // Removable disk.
  kDrive,     // Google drive for file storage.
  kOneDrive,  // Microsoft OneDrive for file storage.
  kMaxValue = kOneDrive
};

}  // namespace data_controls

#endif  // CHROME_BROWSER_ENTERPRISE_DATA_CONTROLS_COMPONENT_H_
