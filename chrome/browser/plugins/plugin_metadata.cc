// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/plugins/plugin_metadata.h"

#include <string>

#include "content/public/common/webplugininfo.h"

PluginMetadata::PluginMetadata(const std::string& identifier,
                               const std::u16string& name,
                               SecurityStatus security_status)
    : identifier_(identifier), name_(name), security_status_(security_status) {}

PluginMetadata::~PluginMetadata() = default;
