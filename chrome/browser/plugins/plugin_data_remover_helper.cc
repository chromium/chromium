// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/plugins/plugin_data_remover_helper.h"

#include "chrome/browser/plugins/plugin_prefs.h"
#include "content/public/browser/plugin_data_remover.h"
#include "content/public/common/webplugininfo.h"

// static
bool PluginDataRemoverHelper::IsSupported(PluginPrefs* plugin_prefs) {
  std::vector<content::WebPluginInfo> plugins;
  content::PluginDataRemover::GetSupportedPlugins(&plugins);
  for (std::vector<content::WebPluginInfo>::const_iterator it = plugins.begin();
       it != plugins.end(); ++it) {
    if (plugin_prefs->IsPluginEnabled(*it))
      return true;
  }
  return false;
}
