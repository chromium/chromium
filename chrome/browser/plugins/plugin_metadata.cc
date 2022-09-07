// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/plugins/plugin_metadata.h"

#include <memory>
#include <string>

#include "base/strings/pattern.h"
#include "content/public/common/webplugininfo.h"
#include "url/gurl.h"

// static
const char PluginMetadata::kAdobeReaderGroupName[] = "Adobe Reader";
const char PluginMetadata::kJavaGroupName[] = "Java(TM)";
const char PluginMetadata::kQuickTimeGroupName[] = "QuickTime Player";
const char PluginMetadata::kShockwaveGroupName[] = "Adobe Shockwave Player";
const char PluginMetadata::kRealPlayerGroupName[] = "RealPlayer";
const char PluginMetadata::kSilverlightGroupName[] = "Silverlight";

PluginMetadata::PluginMetadata(const std::string& identifier,
                               const std::u16string& name,
                               const std::u16string& group_name_matcher,
                               SecurityStatus security_status)
    : identifier_(identifier),
      name_(name),
      group_name_matcher_(group_name_matcher),
      security_status_(security_status) {}

PluginMetadata::~PluginMetadata() = default;

bool PluginMetadata::MatchesPlugin(const content::WebPluginInfo& plugin) const {
  return base::MatchPattern(plugin.name, group_name_matcher_);
}

std::unique_ptr<PluginMetadata> PluginMetadata::Clone() const {
  return std::make_unique<PluginMetadata>(
      identifier_, name_, group_name_matcher_, security_status_);
}
