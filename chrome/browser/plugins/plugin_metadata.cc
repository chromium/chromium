// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/plugins/plugin_metadata.h"

#include <stddef.h>

#include "base/check.h"
#include "base/containers/contains.h"
#include "base/memory/ptr_util.h"
#include "base/strings/pattern.h"
#include "base/strings/string_util.h"
#include "content/public/common/webplugininfo.h"

// static
const char PluginMetadata::kAdobeReaderGroupName[] = "Adobe Reader";
const char PluginMetadata::kJavaGroupName[] = "Java(TM)";
const char PluginMetadata::kQuickTimeGroupName[] = "QuickTime Player";
const char PluginMetadata::kShockwaveGroupName[] = "Adobe Shockwave Player";
const char PluginMetadata::kRealPlayerGroupName[] = "RealPlayer";
const char PluginMetadata::kSilverlightGroupName[] = "Silverlight";

PluginMetadata::PluginMetadata(const std::string& identifier,
                               const std::u16string& name,
                               bool url_for_display,
                               const GURL& plugin_url,
                               const GURL& help_url,
                               const std::u16string& group_name_matcher,
                               const std::string& language,
                               bool plugin_is_deprecated)
    : identifier_(identifier),
      name_(name),
      group_name_matcher_(group_name_matcher),
      url_for_display_(url_for_display),
      plugin_url_(plugin_url),
      help_url_(help_url),
      language_(language),
      plugin_is_deprecated_(plugin_is_deprecated) {}

PluginMetadata::~PluginMetadata() = default;

void PluginMetadata::AddVersion(const base::Version& version,
                                SecurityStatus status) {
  DCHECK(versions_.find(version) == versions_.end());
  versions_[version] = status;
}

bool PluginMetadata::MatchesPlugin(const content::WebPluginInfo& plugin) const {
  return base::MatchPattern(plugin.name, group_name_matcher_);
}

// static
bool PluginMetadata::ParseSecurityStatus(
    const std::string& status_str,
    PluginMetadata::SecurityStatus* status) {
  if (status_str == "up_to_date")
    *status = SECURITY_STATUS_UP_TO_DATE;
  else if (status_str == "out_of_date")
    *status = SECURITY_STATUS_OUT_OF_DATE;
  else if (status_str == "requires_authorization")
    *status = SECURITY_STATUS_REQUIRES_AUTHORIZATION;
  else if (status_str == "fully_trusted")
    *status = SECURITY_STATUS_FULLY_TRUSTED;
  else
    return false;

  return true;
}

PluginMetadata::SecurityStatus PluginMetadata::GetSecurityStatus(
    const content::WebPluginInfo& plugin) const {
  if (plugin_is_deprecated())
    return SECURITY_STATUS_DEPRECATED;

  if (versions_.empty()) {
    // Unknown plugins require authorization.
    return SECURITY_STATUS_REQUIRES_AUTHORIZATION;
  }

  base::Version version;
  content::WebPluginInfo::CreateVersionFromString(plugin.version, &version);
  if (!version.IsValid())
    version = base::Version("0");

  // |lower_bound| returns the latest version that is not newer than |version|.
  auto it = versions_.lower_bound(version);
  // If there is at least one version defined, everything older than the oldest
  // defined version is considered out-of-date.
  if (it == versions_.end())
    return SECURITY_STATUS_OUT_OF_DATE;

  return it->second;
}

bool PluginMetadata::VersionComparator::operator() (
    const base::Version& lhs, const base::Version& rhs) const {
  // Keep versions ordered by newest (biggest) first.
  return lhs.CompareTo(rhs) > 0;
}

std::unique_ptr<PluginMetadata> PluginMetadata::Clone() const {
  PluginMetadata* copy = new PluginMetadata(
      identifier_, name_, url_for_display_, plugin_url_, help_url_,
      group_name_matcher_, language_, plugin_is_deprecated_);
  copy->versions_ = versions_;
  return base::WrapUnique(copy);
}
