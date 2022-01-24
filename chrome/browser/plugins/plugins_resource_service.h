// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PLUGINS_PLUGINS_RESOURCE_SERVICE_H_
#define CHROME_BROWSER_PLUGINS_PLUGINS_RESOURCE_SERVICE_H_

#include "components/web_resource/web_resource_service.h"

class PrefService;
class PrefRegistrySimple;

// This resource service periodically fetches plugin metadata
// from a remote server and updates local state and PluginFinder.
class PluginsResourceService : public web_resource::WebResourceService {
 public:
  explicit PluginsResourceService(PrefService* local_state);

  PluginsResourceService(const PluginsResourceService&) = delete;
  PluginsResourceService& operator=(const PluginsResourceService&) = delete;

  ~PluginsResourceService() override;

  void Init();

  static void RegisterPrefs(PrefRegistrySimple* registry);

 private:
  // WebResourceService override to process the parsed information.
  void Unpack(const base::DictionaryValue& parsed_json) override;
};

#endif  // CHROME_BROWSER_PLUGINS_PLUGINS_RESOURCE_SERVICE_H_
