// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PLUGINS_PLUGIN_METADATA_H_
#define CHROME_BROWSER_PLUGINS_PLUGIN_METADATA_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/strings/string16.h"
#include "base/version.h"
#include "url/gurl.h"

namespace content {
struct WebPluginInfo;
}

class PluginMetadata {
 public:
  // Information about a certain version of the plugin.
  enum SecurityStatus {
    SECURITY_STATUS_UP_TO_DATE,
    SECURITY_STATUS_OUT_OF_DATE,
    SECURITY_STATUS_REQUIRES_AUTHORIZATION,
    SECURITY_STATUS_FULLY_TRUSTED,
  };

  // Used by about:plugins to disable Reader plugin when internal PDF viewer is
  // enabled.
  static const char kAdobeReaderGroupName[];
  static const char kJavaGroupName[];
  static const char kQuickTimeGroupName[];
  static const char kShockwaveGroupName[];
  static const char kAdobeFlashPlayerGroupName[];
  static const char kRealPlayerGroupName[];
  static const char kSilverlightGroupName[];
  static const char kWindowsMediaPlayerGroupName[];
  static const char kGoogleTalkGroupName[];
  static const char kGoogleEarthGroupName[];

  PluginMetadata(const std::string& identifier,
                 const base::string16& name,
                 bool url_for_display,
                 const GURL& plugin_url,
                 const GURL& help_url,
                 const base::string16& group_name_matcher,
                 const std::string& language,
                 bool plugin_is_deprecated);
  ~PluginMetadata();

  // Unique identifier for the plugin.
  const std::string& identifier() const { return identifier_; }

  // Human-readable name of the plugin.
  const base::string16& name() const { return name_; }

  // If |url_for_display| is false, |plugin_url| is the URL of the download page
  // for the plugin, which should be opened in a new tab. If it is true,
  // |plugin_url| is the URL of the plugin installer binary, which can be
  // directly downloaded.
  bool url_for_display() const { return url_for_display_; }
  const GURL& plugin_url() const { return plugin_url_; }

  // URL to open when the user clicks on the "Problems installing?" link.
  const GURL& help_url() const { return help_url_; }

  const std::string& language() const { return language_; }

  // Returns whether the plugin has been deprecated and cannot be updated.
  bool plugin_is_deprecated() const { return plugin_is_deprecated_; }

  bool HasMimeType(const std::string& mime_type) const;
  void AddMimeType(const std::string& mime_type);
  void AddMatchingMimeType(const std::string& mime_type);

  // Adds information about a plugin version.
  void AddVersion(const base::Version& version, SecurityStatus status);

  // Checks if |plugin| mime types match all |matching_mime_types_|.
  // If there is no |matching_mime_types_|, |group_name_matcher_| is used
  // for matching.
  bool MatchesPlugin(const content::WebPluginInfo& plugin);

  // If |status_str| describes a valid security status, writes it to |status|
  // and returns true, else returns false and leaves |status| unchanged.
  static bool ParseSecurityStatus(const std::string& status_str,
                                  SecurityStatus* status);

  // Returns the security status for the given plugin (i.e. whether it is
  // considered out-of-date, etc.)
  SecurityStatus GetSecurityStatus(const content::WebPluginInfo& plugin) const;

  std::unique_ptr<PluginMetadata> Clone() const;

 private:
  struct VersionComparator {
    bool operator()(const base::Version& lhs, const base::Version& rhs) const;
  };

  std::string identifier_;
  base::string16 name_;
  base::string16 group_name_matcher_;
  bool url_for_display_;
  GURL plugin_url_;
  GURL help_url_;
  std::string language_;
  std::map<base::Version, SecurityStatus, VersionComparator> versions_;
  std::vector<std::string> all_mime_types_;
  std::vector<std::string> matching_mime_types_;
  const bool plugin_is_deprecated_;

  DISALLOW_COPY_AND_ASSIGN(PluginMetadata);
};

#endif  // CHROME_BROWSER_PLUGINS_PLUGIN_METADATA_H_
