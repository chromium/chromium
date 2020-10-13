// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PLUGINS_PLUGIN_DATA_REMOVER_HELPER_H_
#define CHROME_BROWSER_PLUGINS_PLUGIN_DATA_REMOVER_HELPER_H_

class PluginPrefs;

class PluginDataRemoverHelper {
 public:
  // Like PluginDataRemover::IsSupported, but checks that the returned plugin
  // is enabled by PluginPrefs.
  static bool IsSupported(PluginPrefs* plugin_prefs);
};

#endif  // CHROME_BROWSER_PLUGINS_PLUGIN_DATA_REMOVER_HELPER_H_
