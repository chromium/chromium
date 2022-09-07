// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PLUGINS_PLUGIN_METADATA_H_
#define CHROME_BROWSER_PLUGINS_PLUGIN_METADATA_H_

#include <memory>
#include <string>

#include "ppapi/buildflags/buildflags.h"
#include "url/gurl.h"

#if !BUILDFLAG(ENABLE_PLUGINS)
#error "Plugins should be enabled"
#endif

namespace content {
struct WebPluginInfo;
}

// TODO(crbug.com/1064647): Remove this class.
class PluginMetadata {
 public:
  // Security status of the plugin.
  // TODO(crbug.com/1064647): Remove unused security statuses.
  enum SecurityStatus {
    SECURITY_STATUS_UP_TO_DATE,
    SECURITY_STATUS_OUT_OF_DATE,
    SECURITY_STATUS_REQUIRES_AUTHORIZATION,
    SECURITY_STATUS_FULLY_TRUSTED,
    // Similar to SECURITY_STATUS_OUT_OF_DATE, but with no hope of updating and
    // running. This is distinct to allow for separate UI treatment.
    SECURITY_STATUS_DEPRECATED,
  };

  // TODO(crbug.com/1064647): Remove after deleting
  // `OutdatedPluginInfoBarDelegate`.
  static const char kAdobeReaderGroupName[];
  static const char kJavaGroupName[];
  static const char kQuickTimeGroupName[];
  static const char kShockwaveGroupName[];
  static const char kRealPlayerGroupName[];
  static const char kSilverlightGroupName[];

  PluginMetadata(const std::string& identifier,
                 const std::u16string& name,
                 const std::u16string& group_name_matcher,
                 SecurityStatus security_status);

  PluginMetadata(const PluginMetadata&) = delete;
  PluginMetadata& operator=(const PluginMetadata&) = delete;

  ~PluginMetadata();

  // Unique identifier for the plugin.
  const std::string& identifier() const { return identifier_; }

  // Human-readable name of the plugin.
  const std::u16string& name() const { return name_; }

  // TODO(crbug.com/1064647): Remove after deleting
  // `OutdatedPluginInfoBarDelegate`.
  bool url_for_display() const { return false; }
  const GURL& plugin_url() const { return plugin_url_; }
  bool plugin_is_deprecated() const { return false; }

  // Returns the security status for the given plugin.
  SecurityStatus security_status() const { return security_status_; }

  // Checks if `group_name_matcher_` matches the name of `plugin`.
  bool MatchesPlugin(const content::WebPluginInfo& plugin) const;

  std::unique_ptr<PluginMetadata> Clone() const;

 private:
  std::string identifier_;
  std::u16string name_;
  std::u16string group_name_matcher_;
  GURL plugin_url_;
  const SecurityStatus security_status_;
};

#endif  // CHROME_BROWSER_PLUGINS_PLUGIN_METADATA_H_
