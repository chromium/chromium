// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PLUGINS_PLUGIN_METADATA_H_
#define CHROME_BROWSER_PLUGINS_PLUGIN_METADATA_H_

#include <string>

#include "ppapi/buildflags/buildflags.h"

#if !BUILDFLAG(ENABLE_PLUGINS)
#error "Plugins should be enabled"
#endif

// TODO(crbug.com/40123808): Remove this class.
class PluginMetadata {
 public:
  // Security status of the plugin.
  enum SecurityStatus {
    SECURITY_STATUS_REQUIRES_AUTHORIZATION,
    SECURITY_STATUS_FULLY_TRUSTED,
  };

  PluginMetadata(const std::string& identifier,
                 const std::u16string& name,
                 SecurityStatus security_status);

  PluginMetadata(const PluginMetadata&) = delete;
  PluginMetadata& operator=(const PluginMetadata&) = delete;

  ~PluginMetadata();

  // Unique identifier for the plugin.
  const std::string& identifier() const { return identifier_; }

  // Human-readable name of the plugin.
  const std::u16string& name() const { return name_; }

  // Returns the security status for the given plugin.
  SecurityStatus security_status() const { return security_status_; }

 private:
  std::string identifier_;
  std::u16string name_;
  const SecurityStatus security_status_;
};

#endif  // CHROME_BROWSER_PLUGINS_PLUGIN_METADATA_H_
